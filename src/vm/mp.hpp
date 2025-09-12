// -*- C++ -*-
/// @file mp.hpp
/// @brief Multi-processor evaluation.

#pragma once

#include "core.hpp"

#include <utils/common.hpp>
#include <utils/debug.hpp>
#include <utils/immovable_vector.hpp>
#include <utils/memory.hpp>
#include <utils/threadpool.hpp>
#include <utils/tuple.hpp>

#include <range/v3/view/subrange.hpp>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <iterator>
#include <mutex>
#include <ranges>

namespace lucid::mp
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

template <typename _Program,
          template <typename> typename StreamCompiler,
          typename Lock = std::mutex>
struct Context
{
    StreamCompiler<_Program> stream;
    Lock                     lock;

    union alignas(cache_line_size)
    {
        struct
        {
            std::atomic_flag need_sort{};
            std::atomic_flag exchanged{};
        };
        std::uint8_t __padding[cache_line_size];
    };

    FORBID_ALL(Context);

    Context(unsigned n) noexcept : stream(n) {}

    bool
    try_sort() noexcept
    {
        if(need_sort.test(std::memory_order_acquire))
        {
            if(lock.try_lock())
            {
                stream.sort();
                lock.unlock();
                need_sort.clear(std::memory_order_release);
                exchanged.clear(std::memory_order_release);
                return false;
            }
            else
                return true;
        }
        return false;
    }

    template <typename InputIter, typename Sentinel, typename OutIter>
    void
    try_exchange(InputIter& ib, const Sentinel s, OutIter& out) noexcept
    {
        if(ib != s && !exchanged.test(std::memory_order_acquire) &&
           !need_sort.test(std::memory_order_acquire) && lock.try_lock())
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
            exchanged.test_and_set(std::memory_order_release);
        }
    }

    void
    step() noexcept
    {
        stream();
        need_sort.test_and_set(std::memory_order_release);
        try_sort();
    }
};

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
        return m_flag.test(std::memory_order_relaxed);
    }

    bool
    set() noexcept
    {
        return m_flag.test_and_set(std::memory_order_relaxed);
    }
};

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
            lucid::for_each(
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

                    if(ctx.try_sort()) return; // will try next time

                    ctx.step();
                });
    }
};

template <std::ranges::input_range... Inputs>
auto
make_input_collection(Inputs&&... inputs) noexcept
{
    return std::tuple{detail::make_input_pair(std::forward<Inputs>(inputs))...};
}

template <typename... Containers>
auto
make_output_collection(Containers&... containers) noexcept
{
    return std::tuple{std::back_inserter(containers)...};
}

template <typename InputCollection>
bool
is_complete(const InputCollection& ic) noexcept
{
    return detail::is_complete_impl(
        std::make_index_sequence<std::tuple_size_v<InputCollection>>{},
        ic);
}

template <typename _Ps, template <typename> typename _Context, std::size_t N>
    requires(N != 0)
class ShopBase : public Signal
{
    using bench_t   = typelist::to<typelist::map<_Context, _Ps>, unique_tuple>;
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
        lucid::for_each(m_benches, std::forward<F>(f));
    }

    template <typename F>
    void
    visit(F&& f) const
    {
        lucid::for_each(m_benches, std::forward<F>(f));
    }

    [[nodiscard]] thread_pool<N>
    get_thread_pool() noexcept
    {
        return map_collect<thread_pool<N>>(
            [s = static_cast<Signal*>(this)]<typename Bench>(Bench& b) noexcept
            { return Worker<Bench>{&b, s}; },
            m_benches);
    }
};

template <typename _Ps, template <typename> typename _Context>
class ShopBase<_Ps, _Context, dynamic_size> : public Signal
{
    using bench_t   = typelist::to<typelist::map<_Context, _Ps>, unique_tuple>;
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

    [[nodiscard]] thread_pool<dynamic_size>
    get_thread_pool() noexcept
    {
        return {std::views::transform(
            m_benches,
            [s = static_cast<Signal*>(this)]<typename W>(W& w) noexcept
            { return Worker<W>{&w, s}; })};
    }
};

template <typename _Ps,
          template <typename> typename _Context,
          std::size_t N = num_workers>
class Shop : public ShopBase<_Ps, _Context, N>
{
    using base = ShopBase<_Ps, _Context, N>;

  public:
    FORBID_ALL(Shop);

    using base::base;

    template <std::ranges::input_range... Inputs>
        requires(sizeof...(Inputs) == typelist::length<_Ps>)
    [[nodiscard]] auto
    bind_inputs(Inputs&&... inputs)
    {
        auto ret = make_input_collection(std::forward<Inputs>(inputs)...);
        base::visit(
            [&ret](auto& bench) noexcept
            {
                lucid::zip_with_for_each(
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
                lucid::zip_with_for_each(
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
                lucid::zip_with_for_each(
                    [](auto& ctx, auto out) noexcept
                    { std::ranges::copy(ctx.stream.get_output_zipped(), out); },
                    bench,
                    oc);
            });
    }
};
} // namespace lucid::mp
