// -*- C++ -*-
// test.hpp
//

#pragma once

#include "argparse.hpp"
#include "functional.hpp"
#include "tuple.hpp"

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/generate_n.hpp>

#include <fmt/color.h>

#include <concepts>
#include <functional>
#include <random>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::test
{
namespace detail
{
template <typename T>
using def_vec = std::vector<T>;

template <typename T>
struct multi_pusher
{
    void
    operator()(std::vector<T>& v, const T& gr) const noexcept
    {
        v.push_back(gr);
    }
};

template <typename T1, typename T2>
struct multi_pusher<std::pair<T1, T2>>
{
    void
    operator()(std::pair<std::vector<T1>, std::vector<T2>>& vs,
               const std::pair<T1, T2>&                     grs) const noexcept
    {
        vs.first.push_back(grs.first);
        vs.second.push_back(grs.second);
    }
};

template <typename... Ts>
struct multi_pusher<std::tuple<Ts...>>
{
    void
    operator()(std::tuple<std::vector<Ts>...>& vs,
               const std::tuple<Ts...>&        grs) const noexcept
    {
        for_each2([](auto& v, const auto& gr) noexcept { v.push_back(gr); },
                  vs,
                  grs);
    }
};

template <typename T>
struct total_counter_helper
{
    static constexpr std::size_t value = 1;
};

template <typename... Ts>
struct total_counter_helper<std::tuple<Ts...>>
{
    static constexpr std::size_t value =
        (... + total_counter_helper<std::remove_cvref_t<Ts>>::value);
};

template <typename T1, typename T2>
struct total_counter_helper<std::pair<T1, T2>>
{
    static constexpr std::size_t value =
        total_counter_helper<std::remove_cvref_t<T1>>::value +
        total_counter_helper<std::remove_cvref_t<T2>>::value;
};

template <typename... Ts>
struct total_counter_helper<steady_tuple<Ts...>>
{
    static constexpr std::size_t value =
        (... + total_counter_helper<std::remove_cvref_t<Ts>>::value);
};

template <typename T>
struct random_dist;

template <>
struct random_dist<bool>
{
    using type = std::bernoulli_distribution;
};

template <std::integral T>
struct random_dist<T>
{
    using type = std::uniform_int_distribution<T>;
};

template <std::floating_point T>
struct random_dist<T>
{
    using type = std::uniform_real_distribution<T>;
};
} // namespace detail

template <typename Test>
    requires(std::predicate<Test> ||
             is_strictly_invocable_rp_v<std::is_floating_point, Test>)
struct NamedTest
{
    Test             test;
    std::string_view name;

    constexpr NamedTest(Test _t, std::string_view _n) noexcept :
        test(_t), name(_n)
    {
    }
};

template <typename Test>
    requires(is_strictly_invocable_rp_v<std::is_floating_point, Test>)
struct NamedFuzzyTest : NamedTest<Test>
{
    std::invoke_result_t<Test> threshold;

    constexpr NamedFuzzyTest(Test                       _t,
                             std::string_view           _n,
                             std::invoke_result_t<Test> _thr) noexcept :
        NamedTest<Test>(_t, _n),
        threshold(_thr)
    {
    }
};

class Track
{
#define _OK(r)               \
    fmt::styled(             \
        r ? "PASS" : "FAIL", \
        fmt::fg(r ? fmt::terminal_color::green : fmt::terminal_color::red))

    static void
    print_status(std::string_view name, bool p, unsigned l = 0) noexcept
    {
        if(l)
            fmt::print("{:<4} {:>8} {:\u2500<{}} {}\n",
                       _OK(p),
                       ' ',
                       "\u251c",
                       l,
                       name);
        else
            fmt::print("{:<4} {:>8} {}\n", _OK(p), ' ', name);
    }

    static void
    print_status(std::string_view         name,
                 bool                     p,
                 std::floating_point auto error,
                 unsigned                 l = 0) noexcept
    {
        const std::floating_point auto e = error * 100;

        if(l)
            fmt::print("{:<4} {:>7.3f}% {:\u2500<{}} {}\n",
                       _OK(p),
                       e,
                       "\u251c",
                       l,
                       name);
        else
            fmt::print("{:<4} {:>7.3f}% {}\n", _OK(p), e, name);
    }

#undef _OK

    static void
    skip_message(unsigned n) noexcept
    {
        using namespace std::literals::string_view_literals;

        fmt::print("{:<4} {:.>8} Skipping {} dependent {}\n",
                   fmt::styled("SKIP", fmt::fg(fmt::terminal_color::yellow)),
                   '.',
                   n,
                   n > 1 ? "tests"sv : "test"sv);
    }

    static void
    check_skip(unsigned c, unsigned t) noexcept
    {
        if(c < t) skip_message(t - c);
    }

    template <typename Test>
    static auto
    run_test(Test&& test) noexcept
    {
        return std::invoke(std::forward<Test>(test));
    }

#define RUN_NTEST                        \
    const bool p = run_test(ntest.test); \
    print_status(ntest.name, p, l);      \
    return p

    template <typename Test>
    static bool
    run_print(const NamedTest<Test>& ntest, unsigned l = 0) noexcept
    {
        RUN_NTEST;
    }

    template <typename Test>
    static bool
    run_print(NamedTest<Test>& ntest, unsigned l = 0) noexcept
    {
        RUN_NTEST;
    }

#undef RUN_NTEST

#define RUN_NFTEST                                \
    const auto error = run_test(nftest.test);     \
    const bool p     = error <= nftest.threshold; \
    print_status(nftest.name, p, error, l);       \
    return p

    template <typename Test>
    static bool
    run_print(const NamedFuzzyTest<Test>& nftest, unsigned l = 0) noexcept
    {
        RUN_NFTEST;
    }

    template <typename Test>
    static bool
    run_print(NamedFuzzyTest<Test>& nftest, unsigned l = 0) noexcept
    {
        RUN_NFTEST;
    }

#undef RUN_NFTEST

    unsigned m_total{};
    unsigned m_passed{};

    template <typename Test>
    bool
    run_print_count(Test&& test, unsigned& counter, unsigned l = 0) noexcept
    {
        using tt = std::remove_cvref_t<Test>;
        if constexpr(is_tuple_v<tt>)
        {
            constexpr unsigned ltotal = std::tuple_size_v<tt>;
            const unsigned     lp     = std::apply(
                [&]<typename... Tests>(Tests&&... tests) noexcept
                {
                    return (0u + ... +
                            static_cast<unsigned>(
                                run_print_count(std::forward<Tests>(tests),
                                                counter,
                                                l)));
                },
                std::forward<Test>(test));
            return lp == ltotal;
        }
        else
        {
            const bool p = run_print(std::forward<Test>(test), l);
            counter++;
            m_passed += p;
            return p;
        }
    }

    template <typename... Tests, std::size_t... Is>
    bool
    dep_run(unsigned& counter,
            std::index_sequence<Is...>,
            Tests&&... tests) noexcept
    {
        return (... &&
                run_print_count(std::forward<Tests>(tests), counter, Is + 1));
    }

  public:
    template <typename Test>
    bool
    check(Test&& test) noexcept
    {
        const bool p = run_print(std::forward<Test>(test));
        ++m_total;
        m_passed += p;
        return p;
    }

    template <typename... Tests>
    bool
    dep_check(Tests&&... tests) noexcept
    {
        constexpr unsigned total =
            (... +
             detail::total_counter_helper<std::remove_cvref_t<Tests>>::value);
        unsigned   counter = 0;
        const bool ret     = dep_run(counter,
                                 std::index_sequence_for<Tests...>{},
                                 std::forward<Tests>(tests)...);

        check_skip(counter, total);

        m_total += total;

        return ret;
    }

    unsigned
    passed() const noexcept
    {
        return m_passed;
    }

    unsigned
    total() const noexcept
    {
        return m_total;
    }

    int
    ret_code() const noexcept
    {
        return m_passed != m_total;
    }

    void
    message() const noexcept
    {
        fmt::print(fmt::emphasis::bold |
                       fmt::fg(ret_code() ? fmt::terminal_color::red :
                                            fmt::terminal_color::green),
                   "{}/{} TESTS PASSED\n",
                   m_passed,
                   m_total);
    }

    ~Track() { message(); }
};

struct range_val_id
{
    template <typename In>
    constexpr In
    operator()(unsigned, In&& in) const noexcept
    {
        return std::forward<In>(in);
    }
};

template <typename... ArgRngs,
          std::invocable<std::ranges::range_value_t<ArgRngs>...> G>
auto
multi_gen(const unsigned n, G&& g, ArgRngs&&... args) noexcept
{
    using result_t =
        std::invoke_result_t<G, std::ranges::range_value_t<ArgRngs>...>;
    maybe_toa<detail::def_vec, result_t>     ret;
    constexpr detail::multi_pusher<result_t> mp{};
    if constexpr(is_tuple_v<decltype(ret)>)
        for_each([n](auto& v) noexcept { v.reserve(n); }, ret);
    else
        ret.reserve(n);

    for(unsigned i = 0; i < n; ++i)
        mp(ret,
           std::invoke(std::forward<G>(g), std::forward<ArgRngs>(args)[i]...));
    return ret;
}

template <std::invocable G, typename T, typename V>
auto
gen_test_val(const unsigned n, G&& g, T&& t, V&& v) noexcept
{
    return ranges::views::generate_n(std::forward<G>(g), n) |
           std::ranges::views::transform(
               [&]<typename Feed>(Feed&& feed) noexcept
               {
                   return std::invoke(
                       std::forward<V>(v),
                       lucid::maybe_apply(std::forward<T>(t),
                                          std::forward<Feed>(feed)),
                       feed);
               });
}

template <std::invocable G, typename T, typename V>
double
estimate(const unsigned n, G&& g, T&& t, V&& v) noexcept
{
    return 1. - static_cast<double>(
                    ranges::accumulate(gen_test_val(n,
                                                    std::forward<G>(g),
                                                    std::forward<T>(t),
                                                    std::forward<V>(v)),
                                       0u)) /
                    static_cast<double>(n);
}

struct ParmTestMaker
{
    unsigned m_num;

    template <std::invocable G, typename T, typename V>
    auto
    test(G&& g, T&& t, V&& v) const noexcept
    {
        return [&]() noexcept
        {
            return std::ranges::all_of(gen_test_val(m_num,
                                                    std::forward<G>(g),
                                                    std::forward<T>(t),
                                                    std::forward<V>(v)),
                                       std::identity{});
        };
    }

    template <std::invocable                            G,
              maybe_applicable<std::invoke_result_t<G>> T,
              maybe_applicable<std::invoke_result_t<G>> ValT,
              typename Cmp = std::equal_to<>>
    auto
    test_cmp(G&& g, T&& t, ValT valt, Cmp cmp = Cmp{}) const noexcept
    {
        return test(std::forward<G>(g),
                    std::forward<T>(t),
                    [&, valt, cmp]<typename Tr, typename Gr>(Tr&& tr,
                                                             Gr&& gr) noexcept {
                        return cmp(std::forward<Tr>(tr),
                                   maybe_apply(valt, std::forward<Gr>(gr)));
                    });
    }

    template <bool           do_sort = true,
              std::invocable G,
              typename T,
              typename V   = range_val_id,
              typename Cmp = decltype(std::ranges::equal)>
    auto
    rng_cmp(G&& g,
            T&& t,
            V&& v   = range_val_id{},
            Cmp cmp = std::ranges::equal) const noexcept
    {
#define LAMBDA_BODY(MY_V)                                                \
    auto make_args = [&]() noexcept                                      \
    {                                                                    \
        const auto gr = multi_gen(m_num, std::forward<G>(g));            \
        if constexpr(is_tuple_v<decltype(gr)>)                           \
            return std::tuple_cat(std::tuple{m_num}, gr);                \
        else                                                             \
            return std::pair{m_num, gr};                                 \
    };                                                                   \
    const auto              args = make_args();                          \
    std::ranges::range auto tr   = std::apply(std::forward<T>(t), args); \
    std::ranges::range auto vr   = std::apply(MY_V, args);               \
    if constexpr(do_sort)                                                \
    {                                                                    \
        std::ranges::sort(tr);                                           \
        std::ranges::sort(vr);                                           \
    }                                                                    \
    return std::invoke(cmp, tr, vr)

        if constexpr(std::is_rvalue_reference_v<decltype(v)>)
            return [&, v = std::move(v), cmp]() noexcept { LAMBDA_BODY(v); };
        else
            return [&, cmp]() noexcept { LAMBDA_BODY(std::forward<V>(v)); };

#undef LAMBDA_BODY
    }

    template <std::invocable G,
              typename T,
              typename V,
              typename Cmp = decltype(std::ranges::equal)>
    auto
    rng_sval(G&& g, T&& t, V&& v, Cmp cmp = std::ranges::equal) const noexcept
    {
        return rng_cmp(
            std::forward<G>(g),
            std::forward<T>(t),
            [&v]<typename... Grs>(unsigned n, Grs&&... grs) noexcept {
                return multi_gen(n,
                                 std::forward<V>(v),
                                 std::forward<Grs>(grs)...);
            },
            cmp);
    }

    template <std::invocable G, typename T, typename V>
    auto
    fuzzy(G&& g, T&& t, V&& v) const noexcept
    {
        return [&]() noexcept
        {
            return estimate(m_num,
                            std::forward<G>(g),
                            std::forward<T>(t),
                            std::forward<V>(v));
        };
    }

    template <typename G,
              typename T,
              typename ValT,
              typename Cmp = std::equal_to<>>
    auto
    fuzzy_cmp(G&& g, T&& t, ValT valt, Cmp cmp = Cmp{}) const noexcept
    {
        return fuzzy(
            std::forward<G>(g),
            std::forward<T>(t),
            [&, cmp]<typename Tr, typename Gr>(Tr&& tr, Gr&& gr) noexcept
            {
                return std::invoke(cmp,
                                   std::forward<Tr>(tr),
                                   maybe_apply(valt, std::forward<Gr>(gr)));
            });
    }

    template <typename G, typename T, typename ValT>
    auto
    fuzzy_ulp(G&& g, T&& t, ValT valt, const unsigned ulp) const noexcept
    {
        return fuzzy_cmp(std::forward<G>(g),
                         std::forward<T>(t),
                         valt,
                         [&](auto t, auto v) noexcept
                         { return almost_equal(t, v, ulp); });
    }
};

template <typename T>
using random_dist = detail::random_dist<T>::type;

class RandomState
{
    std::default_random_engine m_e;

  public:
    RandomState() noexcept : m_e(std::random_device()()) {}

    template <typename T, typename... Args>
    auto
    make_gen(Args&&... args) noexcept
    {
        if constexpr(std::is_floating_point_v<T> && !sizeof...(Args))
            return [&]() noexcept
            { return std::generate_canonical<T, 10>(m_e); };
        else
            return [&,
                    d = random_dist<T>(std::forward<Args>(
                        args)...)]() mutable noexcept { return d(m_e); };
    }
};

unsigned
parse_numruns(int argc, char* argv[]) noexcept
{
    return argparse::parse(
               std::tuple{argparse::option<'n', "numruns">(
                   "Number of runs for parametric tests. Default: 1000",
                   "N",
                   1000,
                   [](std::string_view word) noexcept
                   { return std::atoi(word.data()); })},
               {argc, argv})
        .get_opt<'n'>();
}
} // namespace lucid::test
