// -*- C++ -*-
// mp.hpp
//

#pragma once

#include "core.hpp"

#include <utils/common.hpp>
#include <utils/debug.hpp>
#include <utils/immovable_vector.hpp>
#include <utils/threadpool.hpp>
#include <utils/tuple.hpp>

#include <range/v3/view/subrange.hpp>

#include <algorithm>
#include <bitset>
#include <mutex>
#include <ranges>
#include <vector>

namespace lucid::mp
{
namespace detail
{
template <std::ranges::input_range Input>
std::pair<std::ranges::iterator_t<Input>, std::ranges::iterator_t<Input>>
make_input_pair(Input&& input) noexcept
{
    return std::pair{std::ranges::begin(input), std::ranges::end(input)};
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
    bool                     exchanged = false;
    bool                     done      = false;

    FORBID_ALL(Context);

    Context(unsigned n) noexcept : stream(n) {}
};

template <typename Bench>
struct Worker
{
    using types = typelist::from<Bench>;

    Bench* const bench;

    void
    operator()() const noexcept
    {
        std::bitset<typelist::length<types>> done_flags;

        while(!done_flags.all())
            lucid::for_each(*bench,
                            [&done_flags]<typename C>(C& context) noexcept
                            {
                                if(context.lock.try_lock())
                                {
                                    if(context.done)
                                    {
                                        done_flags.set(
                                            typelist::index<C, types>);
                                        // don't ulock the context - we don't
                                        // need it anymore
                                        // context.lock.unlock();
                                        return;
                                    }

                                    if(context.exchanged) context.stream.sort();

                                    context.stream();
                                    context.stream.sort();
                                    context.lock.unlock();
                                }
                            });
    }
};

template <std::ranges::input_range... Inputs>
using input_collection =
    std::tuple<std::pair<std::ranges::iterator_t<Inputs>,
                         std::ranges::iterator_t<Inputs>>...>;

template <std::ranges::input_range... Inputs>
input_collection<Inputs...>
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

template <typename _Ps,
          template <typename> typename _Context,
          std::size_t N = num_workers>
    requires(N != 0)
class ShopBase
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

    template <std::invocable<bench_t&> F>
    void
    visit(F&& f)
    {
        lucid::for_each(m_benches, std::forward<F>(f));
    }

    template <std::invocable<bench_t&> F>
    void
    visit(F&& f) const
    {
        lucid::for_each(m_benches, std::forward<F>(f));
    }

    [[nodiscard]] thread_pool<N>
    get_thread_pool() noexcept
    {
        return map_collect<thread_pool<N>>([]<typename Bench>(Bench& b) noexcept
                                           { return Worker<Bench>{&b}; },
                                           m_benches);
    }
};

template <typename _Ps, template <typename> typename _Context>
class ShopBase<_Ps, _Context, dynamic_size>
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

    template <std::invocable<bench_t&> F>
    void
    visit(F&& f)
    {
        std::ranges::for_each(m_benches, std::forward<F>(f));
    }

    template <std::invocable<bench_t&> F>
    void
    visit(F&& f) const
    {
        std::ranges::for_each(m_benches, std::forward<F>(f));
    }

    [[nodiscard]] thread_pool<dynamic_size>
    get_thread_pool() noexcept
    {
        return {std::views::transform(m_benches,
                                      []<typename W>(W& w) noexcept
                                      { return Worker<W>{&w}; })};
    }
};

template <typename _Ps, template <typename> typename _Context, std::size_t N>
class Shop : public ShopBase<_Ps, _Context, N>
{
    using base = ShopBase<_Ps, _Context, N>;

  public:
    FORBID_ALL(Shop);

    using base::base;

    template <std::ranges::input_range... Inputs>
        requires(sizeof...(Inputs) == typelist::length<_Ps>)
    [[nodiscard]] input_collection<Inputs...>
    bind_inputs(Inputs&&... inputs)
    {
        input_collection<Inputs...> ret =
            make_input_collection(std::forward<Inputs>(inputs)...);
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
    exchange(InputCollection& ic, OutputCollection& oc)
    {
        base::visit(
            [&ic, &oc](auto& bench) noexcept
            {
                lucid::zip_with_for_each(
                    [](auto& ctx, auto& in, auto& out) noexcept
                    {
                        if(ctx.lock.try_lock())
                        {
                            auto& [ib, ie] = in;
                            auto exs =
                                ctx.stream.exchange(ranges::subrange(ib, ie));
                            ib += std::ranges::size(exs);
                            out = std::ranges::copy(exs, out).out;
                            ctx.lock.unlock();
                        }
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
