// -*- C++ -*-
/// @file mp.hpp
/// @brief Multi-processor evaluation.

#pragma once

#include "core.hpp"

#include <common/forbid.hpp>
#include <common/debug.hpp>
#include <common/immovable_vector.hpp>
#include <common/memory.hpp>
#include <common/threadpool.hpp>
#include <common/tuple.hpp>

#include <range/v3/view/subrange.hpp>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <iterator>
#include <mutex>
#include <ranges>

namespace loom::vm::mp
{
namespace detail
{
template <std::ranges::input_range Input>
auto
make_input_pair(Input&& input) noexcept
{
    return std::pair{std::ranges::begin(input), std::ranges::end(input)};
}

template <typename InputCollection, std::size_t... Idxs>
bool
is_complete_impl(std::index_sequence<Idxs...>,
                 const InputCollection& ic) noexcept
{
    return (... && (std::get<Idxs>(ic).first == std::get<Idxs>(ic).second));
}
} // namespace detail

#ifdef NUM_THREADS
constexpr inline std::size_t num_workers = NUM_THREADS;
#else
constexpr inline std::size_t num_workers = dynamic_size;
#endif

/// @brief Kernel execution context. Manages concurrent exchange and kernel
/// execution.
/// @tparam K kernel to manage.
/// @tparam StreamCompiler class template that implements stream with support
/// for concurrent exchange/execution, e.h. simd::stream.
/// @tparam Lock should implement try_lock and unlock.
template <Kernel K,
          template <typename> typename StreamCompiler,
          typename Lock = std::mutex>
struct Context
{
    StreamCompiler<K> stream;
    Lock              lock; // protects sorting vs. exchange
    bool              need_sort = false;

    union alignas(cache_line_size)
    {
        std::atomic_flag exchange_available{};
        std::uint8_t     __padding[cache_line_size];
    };

    FORBID_ALL(Context);

    Context(unsigned n) noexcept : stream(n) {}

    /// @brief Attempt to sort. Used in worker thread.
    void
    try_sort() noexcept
    {
        if(need_sort && lock.try_lock())
        {
            const bool ex = stream.sort();
            lock.unlock();
            need_sort = false;

            if(ex) exchange_available.test_and_set(std::memory_order_release);
        }
    }

    /// @brief Attempt to compute and then attempts to sort. Used in worker
    /// thread. Can be executed during exchange concurrently.
    void
    try_step() noexcept
    {
        if(!need_sort)
        {
            stream();
            need_sort = true;
        }
        try_sort();
    }

    /// @brief Attempt to exchange. Used in supervisor/main thread.
    template <typename InputIter, typename Sentinel, typename OutIter>
    void
    try_exchange(InputIter& ib, const Sentinel s, OutIter& out) noexcept
    {
        if(ib != s && exchange_available.test(std::memory_order_acquire) &&
           lock.try_lock())
        {
            auto exs = stream.exchange(ranges::subrange(ib, s));
            // copy and update input and output iterators
            std::ranges::for_each(exs,
                                  [&ib, &out](const auto& ex) noexcept
                                  {
                                      out = ex;
                                      ++out;
                                      ++ib;
                                  });
            lock.unlock();
            exchange_available.clear(std::memory_order_release);
        }
    }
};

/// @brief One-sided signal.
class alignas(cache_line_size) Signal
{
    union
    {
        std::atomic_flag m_flag{};
        std::uint8_t     __padding[cache_line_size];
    };

  public:
    Signal() noexcept = default;
    FORBID_COPY(Signal);
    FORBID_MOVE(Signal);

    operator bool() const noexcept
    {
        return m_flag.test(std::memory_order_acquire);
    }

    bool
    set() noexcept
    {
        return m_flag.test_and_set(std::memory_order_release);
    }
};

/// @brief Worker thread functional object.
/// @tparam Bench tuple of contexts of unique kernels.
template <typename Bench>
struct Worker
{
    using types = typelist::from<Bench>;

    Bench* const  bench;
    Signal* const signal;

    void
    operator()() const noexcept
    {
        std::bitset<typelist::length<types>> done_flags;

        while(!done_flags.all())
            loom::for_each(
                *bench,
                [&done_flags, signal = signal]<typename C>(C& ctx) noexcept
                {
                    constexpr std::size_t idx     = typelist::index<C, types>;
                    bool                  is_done = done_flags.test(idx);
                    if(is_done) return;

                    if(!is_done && *signal && ctx.lock.try_lock())
                    {
                        if(ctx.stream.done()) done_flags.set(idx);

                        ctx.lock.unlock();
                    }

                    ctx.try_step();
                });
    }
};

/// @brief Helper function that ties kernel inputs together.
/// @return Tuple of pairs of an input iterator and a sentinel.
template <std::ranges::input_range... Inputs>
auto
make_input_collection(Inputs&&... inputs) noexcept
{
    // using make_tuple to avoid pair converting to tuple in case of one input
    return std::make_tuple(
        detail::make_input_pair(std::forward<Inputs>(inputs))...);
}

/// @brief Helper function that ties together output iterators.
/// @return tuple of output iterators.
template <typename... Containers>
auto
make_output_collection(Containers&... containers) noexcept
{
    return std::tuple{std::back_inserter(containers)...};
}

/// @brief Checks whether kernel inputs are completely consumed.
template <typename InputCollection>
bool
is_complete(const InputCollection& ic) noexcept
{
    return detail::is_complete_impl(
        std::make_index_sequence<std::tuple_size_v<InputCollection>>{},
        ic);
}

/// @brief Workshop basic functionality for static number of workers.
/// @tparam Ks kernels.
/// @tparam Ctx class template constructing context.
/// @tparam N number of workers.
template <typelist::List Ks, template <typename> typename Ctx, std::size_t N>
    requires(N != 0)
class ShopBase : public Signal
{
    using bench_t   = typelist::to<typelist::map<Ctx, Ks>, unique_tuple>;
    using benches_t = typelist::to<typelist::repeat<N, bench_t>, unique_tuple>;
    benches_t m_benches;

  public:
    FORBID_ALL(ShopBase);

    ShopBase(unsigned stream_size) noexcept :
        m_benches(uniform_init, uniform_init, stream_size)
    {
    }

    template <typename F>
    void
    visit(F&& f)
    {
        loom::for_each(m_benches, std::forward<F>(f));
    }

    template <typename F>
    void
    visit(F&& f) const
    {
        loom::for_each(m_benches, std::forward<F>(f));
    }

    /// @brief Constructs loom::thread_pool of static size with Worker.
    [[nodiscard]] thread_pool<N>
    get_thread_pool() noexcept
    {
        return map_collect<thread_pool<N>>(
            [s = static_cast<Signal*>(this)]<typename Bench>(Bench& b) noexcept
            { return Worker<Bench>{&b, s}; },
            m_benches);
    }
};

/// @brief Workshop basic functionality for dynamic number of workers.
/// @tparam Ks kernels.
/// @tparam Ctx class template constructing context.
template <typename Ks, template <typename> typename Ctx>
class ShopBase<Ks, Ctx, dynamic_size> : public Signal
{
    using bench_t   = typelist::to<typelist::map<Ctx, Ks>, unique_tuple>;
    using benches_t = ImmovableVector<bench_t>;

    benches_t m_benches;

  public:
    FORBID_ALL(ShopBase);

    ShopBase(std::size_t _num_workers, unsigned stream_size) noexcept :
        m_benches(_num_workers, uniform_init, stream_size)
    {
        ASSERT(_num_workers != 0, "make more workers");
    }

    template <typename F>
    void
    visit(F&& f)
    {
        std::ranges::for_each(m_benches, std::forward<F>(f));
    }

    template <typename F>
    void
    visit(F&& f) const
    {
        std::ranges::for_each(m_benches, std::forward<F>(f));
    }

    /// @brief Constructs loom::thread_pool of dynamic sizes with Worker.
    [[nodiscard]] thread_pool<dynamic_size>
    get_thread_pool() noexcept
    {
        return {std::views::transform(
            m_benches,
            [s = static_cast<Signal*>(this)]<typename W>(W& w) noexcept
            { return Worker<W>{&w, s}; })};
    }
};

/// @brief Implements workshop.
/// @tparam Ks kernels.
/// @tparam Ctx class template constructing context.
/// @tparam N number of workers.
template <typelist::List Ks,
          template <typename> typename Ctx,
          std::size_t N = num_workers>
class Shop : public ShopBase<Ks, Ctx, N>
{
    using base = ShopBase<Ks, Ctx, N>;

  public:
    FORBID_ALL(Shop);

    using base::base;

    template <std::ranges::input_range... Inputs>
        requires(sizeof...(Inputs) == typelist::length<Ks>)
    [[nodiscard]] auto
    bind_inputs(Inputs&&... inputs)
    {
        auto ret = make_input_collection(std::forward<Inputs>(inputs)...);
        base::visit(
            [&ret](auto& bench) noexcept
            {
                loom::zip_with_for_each(
                    [](auto& ctx, auto& iters) noexcept
                    {
                        auto& [b, e] = iters;
                        b = ctx.stream.set_inputs(ranges::subrange(b, e));
                    },
                    bench,
                    ret);
            });
        return ret;
    }

    template <typename InputCollection, typename OutputCollection>
    void
    exchange(InputCollection& ic, OutputCollection& oc) noexcept
    {
        base::visit(
            [&ic, &oc](auto& bench) noexcept
            {
                loom::zip_with_for_each(
                    [](auto& ctx, auto& in, auto& out) noexcept
                    {
                        auto& [ib, ie] = in;
                        ctx.try_exchange(ib, ie, out);
                    },
                    bench,
                    ic,
                    oc);
            });
    }

    template <typename OutputCollection>
    void
    finalize(OutputCollection& oc)
    {
        base::visit(
            [&oc](auto& bench) noexcept
            {
                loom::zip_with_for_each(
                    [](auto& ctx, auto out) noexcept
                    { std::ranges::copy(ctx.stream.get_output_zipped(), out); },
                    bench,
                    oc);
            });
    }
};
} // namespace loom::vm::mp
