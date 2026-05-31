// -*- C++ -*-
/// @file threadpool.hpp
/// @brief Basic thread pool.

#pragma once

#include "debug.hpp"
#include "forbid.hpp"
#include "memory.hpp"
#include "tmp.hpp"
#include "tuple.hpp"

#include <concepts>
#include <ranges>
#include <thread>
#include <type_traits>
#include <vector>

#include <hwloc.h>
#include <pthread.h>

namespace loom
{
/// @brief Set pthread affinity to a core or a few cores.
/// @param i logical core index or input range of indices.
template <typename T>
    requires(std::is_nothrow_convertible_v<T, std::size_t> ||
             (std::ranges::input_range<T> &&
              std::is_nothrow_convertible_v<std::ranges::range_value_t<T>,
                                            std::size_t>))
static int
set_thread_affinity(pthread_t thread, T&& i) noexcept
{
    cpu_set_t set;
    CPU_ZERO(&set);

    if constexpr(std::is_nothrow_convertible_v<T, std::size_t>)
        CPU_SET(i, &set);
    else if constexpr(std::ranges::input_range<T> &&
                      std::is_nothrow_convertible_v<
                          std::ranges::range_value_t<T>,
                          std::size_t>)
        for(std::size_t v: std::forward<T>(i)) CPU_SET(v, &set);

    const int s = pthread_setaffinity_np(thread, sizeof(set), &set);
    ASSERT(s == 0, "pthread_setaffinity_np failed");
    return s;
}

/// @brief Range view exposing cores topology of CPUs with SMP.
///
/// Iterates over CPU cores and extracts sets of logical siblings.
class CPUTopology : public std::ranges::view_interface<CPUTopology>
{
    class Siblings : public std::ranges::view_interface<Siblings>
    {
        hwloc_topology_t p_topology;
        hwloc_obj_t      p_core;

        class _iterator : public std::input_iterator_tag
        {
            hwloc_topology_t p_topology;
            hwloc_obj_t      p_core;

            int         m_pu_idx{};
            hwloc_obj_t p_pu;

            void
            next() noexcept
            {
                p_pu = hwloc_get_obj_inside_cpuset_by_type(p_topology,
                                                           p_core->cpuset,
                                                           HWLOC_OBJ_PU,
                                                           ++m_pu_idx);
            }

          public:
            using value_type      = unsigned;
            using difference_type = int;

            _iterator() noexcept = default;
            _iterator(hwloc_topology_t topology, hwloc_obj_t core) noexcept :
                p_topology(topology), p_core(core),
                p_pu(hwloc_get_obj_inside_cpuset_by_type(p_topology,
                                                         p_core->cpuset,
                                                         HWLOC_OBJ_PU,
                                                         m_pu_idx))
            {
            }

            bool
            operator==(std::default_sentinel_t) const noexcept
            {
                return p_pu == NULL;
            }

            value_type
            operator*() const noexcept
            {
                return p_pu->os_index;
            }

            _iterator&
            operator++() noexcept
            {
                next();
                return *this;
            }

            _iterator
            operator++(int) noexcept
            {
                _iterator ret(*this);
                next();
                return ret;
            }
        };

      public:
        Siblings(hwloc_topology_t topology, hwloc_obj_t core) noexcept :
            p_topology(topology), p_core(core)
        {
        }

        _iterator
        begin() const noexcept
        {
            return {p_topology, p_core};
        }

        std::default_sentinel_t
        end() const noexcept
        {
            return std::default_sentinel;
        }
    };

    class _iterator : public std::input_iterator_tag
    {
        hwloc_topology_t p_topology;
        int              m_depth;
        int              m_ncores;
        int              m_idx{};

      public:
        using difference_type = int;
        using value_type      = Siblings;

        _iterator(hwloc_topology_t topology) noexcept :
            p_topology(topology),
            m_depth(hwloc_get_type_depth(p_topology, HWLOC_OBJ_CORE)),
            m_ncores(hwloc_get_nbobjs_by_depth(p_topology, m_depth))
        {
        }

        bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_idx == m_ncores;
        }

        value_type
        operator*() const noexcept
        {
            return {p_topology,
                    hwloc_get_obj_by_depth(p_topology, m_depth, m_idx)};
        }

        _iterator&
        operator++() noexcept
        {
            ++m_idx;
            return *this;
        }

        _iterator
        operator++(int) noexcept
        {
            _iterator ret(*this);
            ++m_idx;
            return ret;
        }
    };

    hwloc_topology_t p_topology;

  public:
    CPUTopology() noexcept
    {
        hwloc_topology_init(&p_topology);
        hwloc_topology_load(p_topology);
    }

    _iterator
    begin() const noexcept
    {
        return {p_topology};
    }

    std::default_sentinel_t
    end() const noexcept
    {
        return std::default_sentinel;
    }

    ~CPUTopology() { hwloc_topology_destroy(p_topology); }
};

/// @brief Threadpool affinity method.
///
/// Extra threads (if threadpool size exceeds the number of CPUs) never affined.
///
/// Physical affinity example:
/// The topology of a SMT2 CPU with 4 physical cores and 8
/// logical cores is [[0, 4], [1, 5], [2, 6], [3, 7]]. First physical core
/// provides two logical cores indexed 0 and 4, second - 1 and 5, etc.
/// Correspondingly first thread is affined to logical cores 0 and 4,
/// second - to 1 and 5, etc.
enum class AFFINITY
{
    NONE,    /*!< Don't affine threads. */
    FULL,    /*!< Affine every thread to a separate logical core. */
    PHYSICAL /*!< Affine every thread to logical siblings of each physical core.
              */
};

constexpr inline std::size_t dynamic_size = static_cast<std::size_t>(-1);

/// @brief Constant sized thread pool.
/// @tparam N number of threads.
///
/// By default, threads join synchronously upon thread pool destruction, thus
/// the current scope might be blocked until all threads complete their tasks.
/// To avoid this behavior use detach().
template <std::size_t N>
class thread_pool
{
    using threads_t =
        typelist::to<typelist::repeat<N, std::thread>, unique_tuple>;
    threads_t m_threads;

    void
    join_all() noexcept
    {
        visit(
            [](std::thread& t) noexcept
            {
                if(t.joinable()) t.join();
            });
    }

    void
    affine_full() noexcept
    {
        std::size_t idx = 0;

        return visit(
            [](std::thread& t, std::size_t& idx) noexcept
            {
                if(idx < std::thread::hardware_concurrency())
                {
                    set_thread_affinity(t.native_handle(), idx);
                    ++idx;
                }
                else
                    return;
            },
            idx);
    }

    void
    affine_physical() noexcept
    {
        CPUTopology top;
        auto        iter = top.begin();

        return visit(
            [&top](std::thread& t, auto& iter) noexcept
            {
                if(iter != top.end())
                {
                    set_thread_affinity(t.native_handle(), *iter);
                    ++iter;
                }
                else
                    return;
            },
            iter);
    }

  public:
    FORBID_DEFAULT(thread_pool);
    FORBID_COPY(thread_pool);

    template <typename... Args, std::regular_invocable<Args...> Worker>
    thread_pool(Worker worker, Args... args) noexcept :
        m_threads(loom::uniform_init, worker, args...)
    {
    }

    template <std::regular_invocable... Workers>
        requires(sizeof...(Workers) == N)
    thread_pool(Workers&&... workers) noexcept :
        m_threads(std::move(workers)...)
    {
    }

    thread_pool(thread_pool&& other) noexcept :
        m_threads(std::move(other.m_threads))
    {
    }

    thread_pool&
    operator=(thread_pool&& rhs) noexcept
    {
        join_all();
        m_threads = std::move(rhs.m_threads);
    }

    /// @brief Visit each thread.
    template <typename Visitor, typename... Args>
    requires(std::is_invocable_v<Visitor, std::thread&, Args...>)
    void
    visit(Visitor&& v, Args&&... args) noexcept(
        std::is_nothrow_invocable_v<Visitor, std::thread&, Args...>)
    {
        loom::for_each(m_threads,
                       std::forward<Visitor>(v),
                       std::forward<Args>(args)...);
    }

    /// @brief Affine threads.
    void
    affine(const AFFINITY a = AFFINITY::FULL) noexcept
    {
        switch(a)
        {
        case AFFINITY::FULL: affine_full(); break;
        case AFFINITY::PHYSICAL: affine_physical(); break;
        default: break;
        }
    }

    /// @brief Detach all threads.
    void
    detach()
    {
        visit([](std::thread& t) { if(t.joinable()) t.detach(); });
    }

    ~thread_pool() { join_all(); }
};

/// @brief Thread pool with size determined at runtime.
///
/// By default, threads join synchronously upon thread pool destruction, thus
/// the current scope might be blocked until all threads complete their tasks.
/// To avoid this behavior use detach().
template <>
class thread_pool<dynamic_size>
{
    std::vector<std::thread> m_threads;

    void
    affine_full() noexcept
    {
        for(std::size_t i = 0;
            i < std::min(static_cast<std::size_t>(
                             std::thread::hardware_concurrency()),
                         m_threads.size());
            ++i)
            set_thread_affinity(m_threads[i].native_handle(), i);
    }

    void
    affine_physical() noexcept
    {
        CPUTopology top;
        auto        iter = top.begin();

        for(std::size_t i = 0;
            i < std::min(static_cast<std::size_t>(
                             std::thread::hardware_concurrency()),
                         m_threads.size()) &&
            iter != top.end();
            ++i, ++iter)
            set_thread_affinity(m_threads[i].native_handle(), *iter);
    }

    void
    join_all() noexcept
    {
        for(std::thread& t: m_threads) if(t.joinable()) t.join();
    }

  public:
    FORBID_DEFAULT(thread_pool);
    FORBID_COPY(thread_pool);

    template <typename... Args, std::regular_invocable<Args...> Worker>
    thread_pool(std::size_t n, Worker worker, Args... args)
    {
        m_threads.reserve(n);
        for(std::size_t i = 0; i < n; ++i)
            m_threads.emplace_back(worker, args...);
    }

    template <std::ranges::input_range Workers, typename... Args>
        requires(std::regular_invocable<std::ranges::range_value_t<Workers>,
                                        Args...>)
    thread_pool(Workers&& workers, Args... args) noexcept
    {
        std::ranges::for_each(
            std::forward<Workers>(workers),
            [&threads = this->m_threads, args...](auto&& worker)
            { threads.emplace_back(std::move(worker), args...); });
        m_threads.shrink_to_fit();
    }

    thread_pool(thread_pool&& other) noexcept :
        m_threads(std::move(other.m_threads))
    {
    }

    thread_pool&
    operator=(thread_pool&& rhs) noexcept
    {
        join_all();
        m_threads = std::move(rhs.m_threads);
        return *this;
    }

    /// @brief Visit each thread.
    template <typename Visitor, typename... Args>
    requires(std::is_invocable_v<Visitor, std::thread&, Args...>)
    void
    visit(Visitor&& v, Args&&... args) noexcept(
        std::is_nothrow_invocable_v<Visitor, std::thread&, Args...>)
    {
        for(std::thread& t: m_threads) std::invoke(v, t, args...);
    }

    /// @brief Affine threads.
    void
    affine(const AFFINITY a = AFFINITY::FULL) noexcept
    {
        switch(a)
        {
        case AFFINITY::FULL: affine_full(); break;
        case AFFINITY::PHYSICAL: affine_physical(); break;
        default: break;
        }
    }

    /// @brief Detach all threads.
    void
    detach()
    {
        for(std::thread& t: m_threads) if(t.joinable()) t.detach();
    }

    ~thread_pool() { join_all(); }
};

/// @brief Apply a given function object @p f to the elements of the source
/// range @p r in parallel.
/// @tparam N static thread pool size.
/// @param r source range.
/// @param f function to apply.
/// @param a threads affinity.
/// @param cs number of elements processed together in one thread.
/// @param nw dynamic thread pool size.
template <std::size_t                      N = dynamic_size,
          std::ranges::random_access_range R,
          typename F>
    requires(std::ranges::sized_range<R>)
void
parallel_for_each(
    R&&               r,
    F&&               f,
    const std::size_t cs = 64,
    const AFFINITY    a  = AFFINITY::NONE,
    const std::size_t nw =
        N == dynamic_size ?
            std::thread::hardware_concurrency() :
            N) noexcept(is_nothrow_invopplicable_v<F,
                                                   std::ranges::range_value_t<
                                                       R>>)
{
    PADDED_VALUE(std::atomic_size_t, counter);

    const std::size_t s = std::ranges::size(r);

    const auto get_tp = [&](auto worker) noexcept
    {
        if constexpr(N == dynamic_size)
            return thread_pool<N>(nw, worker);
        else
            return thread_pool<N>(worker);
    };

    auto tp = get_tp(
        [&, &c = counter]()
        {
            while(true)
            {
                const std::size_t b =
                    c.fetch_add(cs, std::memory_order_relaxed);

                if(b >= s) return;

                std::ranges::for_each(
                    std::views::counted(std::ranges::begin(r) + b, cs),
                    f);
            }
        });

    tp.affine(a);
}
} // namespace loom
