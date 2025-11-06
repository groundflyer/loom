// -*- C++ -*-
/// @file threadpool.hpp
/// @brief Basic thread pool.

#pragma once

#include "common.hpp"
#include "debug.hpp"
#include "tmp.hpp"
#include "tuple.hpp"

#include <concepts>
#include <ranges>
#include <thread>
#include <vector>

#include <pthread.h>

namespace loom
{
namespace detail
{
static int
set_thread_affinity(pthread_t thread, std::size_t i) noexcept
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(i, &set);
    const int s = pthread_setaffinity_np(thread, sizeof(set), &set);
    ASSERT(s == 0, "pthread_setaffinity_np failed");
    return s;
}
} // namespace detail
constexpr inline std::size_t dynamic_size = static_cast<std::size_t>(-1);

template <std::size_t N>
class thread_pool
{
    using threads_t =
        typelist::to<typelist::repeat<N, std::thread>, unique_tuple>;
    threads_t m_threads;

    void
    join_all() noexcept
    {
        loom::for_each(m_threads, [](std::thread& t) noexcept { t.join(); });
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

    thread_pool&
    affine(const std::size_t step = 1) noexcept
    {
        std::size_t idx = 0;
        loom::for_each(
            m_threads,
            [step](std::thread& t, std::size_t& idx) noexcept
            {
                detail::set_thread_affinity(t.native_handle(), idx);
                idx += step;
            },
            idx);

        return *this;
    }

    ~thread_pool() { join_all(); }
};

template <>
class thread_pool<dynamic_size>
{
    std::vector<std::thread> m_threads;

    void
    join_all() noexcept
    {
        for(std::thread& t: m_threads) t.join();
    }

  public:
    FORBID_DEFAULT(thread_pool);
    FORBID_COPY(thread_pool);

    template <typename... Args, std::regular_invocable<Args...> Worker>
    thread_pool(std::size_t n, Worker worker, Args... args) noexcept
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

    thread_pool&
    affine(const std::size_t step = 1) noexcept
    {
        std::size_t idx = 0;
        std::ranges::for_each(m_threads,
                              [step, &idx](std::thread& t) noexcept
                              {
                                  detail::set_thread_affinity(t.native_handle(),
                                                              idx);
                                  idx += step;
                              });
        return *this;
    }

    ~thread_pool() { join_all(); }
};
} // namespace loom
