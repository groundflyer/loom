// -*- C++ -*-
// test.hpp
/// @file test.hpp
/// @brief Testing utilities.

#pragma once

#include "argparse.hpp"
#include "functional.hpp"
#include "string.hpp"
#include "tuple.hpp"

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/generate_n.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/zip_with.hpp>

#include <fmt/color.h>
#include <fmt/ranges.h>

#include <concepts>
#include <functional>
#include <random>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace loom::test
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
        zip_with_for_each([](auto& v, const auto& gr) noexcept
                          { v.push_back(gr); },
                          vs,
                          grs);
    }
};

template <typename T>
struct total_counter_helper : std::integral_constant<std::size_t, 1>
{
};

template <typename... Ts>
struct total_counter_helper<std::tuple<Ts...>> :
    std::integral_constant<
        std::size_t,
        (... + total_counter_helper<std::remove_cvref_t<Ts>>::value)>
{
};

template <typename T1, typename T2>
struct total_counter_helper<std::pair<T1, T2>> :
    std::integral_constant<
        std::size_t,
        total_counter_helper<std::remove_cvref_t<T1>>::value +
            total_counter_helper<std::remove_cvref_t<T2>>::value>
{
};

template <typename... Ts>
struct total_counter_helper<unique_tuple<Ts...>> :
    std::integral_constant<
        std::size_t,
        (... + total_counter_helper<std::remove_cvref_t<Ts>>::value)>
{
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

template <template <typename> typename Array, typename T>
struct maybe_toa
{
    using type = Array<T>;
};

template <template <typename> typename Array, typename T1, typename T2>
struct maybe_toa<Array, std::pair<T1, T2>>
{
    using type = std::pair<Array<T1>, Array<T2>>;
};

template <template <typename> typename Array, typename... Ts>
struct maybe_toa<Array, std::tuple<Ts...>>
{
    using type = std::tuple<Array<Ts>...>;
};

template <typename Tuple, typename Cmp, std::size_t... Idxs>
constexpr bool
tuple_cmp_impl(const Tuple& a,
               const Tuple& b,
               const Cmp&   cmps,
               std::index_sequence<Idxs...>) noexcept
{
    return (... && std::get<Idxs>(cmps)(std::get<Idxs>(a), std::get<Idxs>(b)));
}
} // namespace detail

/// @brief Tuple of arrays.
template <template <typename> typename Array, typename T>
using maybe_toa = detail::maybe_toa<Array, T>::type;

/// @brief Associates a test with a name.
template <typename Test, std::convertible_to<std::string_view> Name>
    requires(std::predicate<Test> ||
             is_strictly_invocable_rp_v<std::is_floating_point, Test>)
struct NamedTest
{
    /// @brief Predicate that doesn't accept parameters.
    ///
    /// An invocation resulting with @p true is considered as pass
    /// otherwise as fail.
    Test test;
    /// @brief Logged name.
    Name name;

    constexpr NamedTest(Test _t, Name _n) noexcept : test(_t), name(_n) {}
};

/// @brief Associates a fuzzy test with name and failing threshold.
///
/// @tparam Test function object that doesn't accept parameters
/// and returns relative ratio of failed runs.
template <typename Test, typename Name>
    requires(is_strictly_invocable_rp_v<std::is_floating_point, Test>)
struct NamedFuzzyTest : NamedTest<Test, Name>
{
    /// @brief The portion of fails when test is considered as fail.
    std::invoke_result_t<Test> threshold;

    constexpr NamedFuzzyTest(Test                       _t,
                             Name                       _n,
                             std::invoke_result_t<Test> _thr) noexcept :
        NamedTest<Test, Name>(_t, _n), threshold(_thr)
    {
    }
};

/// @brief Keeps track of tests and prints their status.
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

    template <typename Test, typename Name>
    static bool
    run_print(const NamedTest<Test, Name>& ntest, unsigned l = 0) noexcept
    {
        RUN_NTEST;
    }

    template <typename Test, typename Name>
    static bool
    run_print(NamedTest<Test, Name>& ntest, unsigned l = 0) noexcept
    {
        RUN_NTEST;
    }

#undef RUN_NTEST

#define RUN_NFTEST                                \
    const auto error = run_test(nftest.test);     \
    const bool p     = error <= nftest.threshold; \
    print_status(nftest.name, p, error, l);       \
    return p

    template <typename Test, typename Name>
    static bool
    run_print(const NamedFuzzyTest<Test, Name>& nftest, unsigned l = 0) noexcept
    {
        RUN_NFTEST;
    }

    template <typename Test, typename Name>
    static bool
    run_print(NamedFuzzyTest<Test, Name>& nftest, unsigned l = 0) noexcept
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

    /// @brief Print conclusion.
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

  public:
    /// @brief Run @p test and log the result.
    ///
    /// Test considered passed when the result is true.
    ///
    /// @param test an instance NamedTest or NamedFuzzyTest.
    template <typename Test>
    bool
    check(Test&& test) noexcept
    {
        const bool p = run_print(std::forward<Test>(test));
        ++m_total;
        m_passed += p;
        return p;
    }

    /// @brief Run and log @p tests successively: the invocation of each next
    /// test depends on the result of the previous one.
    ///
    /// @param tests instances of NamedTest or NamedFuzzy test.
    /// Can be grouped with std::pair and std::tuple.
    ///
    /// A failed test halts the entire run or the run of its group, the
    /// following tests considered failed.
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

    /// @brief The number of passed tests.
    unsigned
    passed() const noexcept
    {
        return m_passed;
    }

    /// @brief The number of checked tests.
    unsigned
    total() const noexcept
    {
        return m_total;
    }

    /// @brief Returns non-zero if there are failed tests.
    int
    ret_code() const noexcept
    {
        return m_passed != m_total;
    }

    ~Track() { message(); }
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
        for_each(ret, [n](auto& v) noexcept { v.reserve(n); });
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
                   return std::invoke(std::forward<V>(v),
                                      invopply(std::forward<T>(t), feed),
                                      feed);
               });
}

template <std::invocable G, typename T, typename V>
auto
gen_test_val_inv(const unsigned n, G&& g, T&& t, V&& v) noexcept
{
    return ranges::views::generate_n(std::forward<G>(g), n) |
           std::ranges::views::transform(
               [&](auto&& feed) noexcept
               {
                   return invopply(std::forward<T>(t),
                                   std::forward<decltype(feed)>(feed));
               }) |
           std::ranges::views::transform(std::forward<V>(v));
}

template <std::invocable G, typename T, typename V>
auto
gen_test_val_rng_inv(const unsigned n, G&& g, T&& t, V&& v) noexcept
{
    return std::invoke(std::forward<T>(t),
                       ranges::views::generate_n(std::forward<G>(g), n)) |
           std::views::transform(std::forward<V>(v));
}

template <std::ranges::input_range Rng>
double
estimate_error(Rng&& rng, const std::size_t count) noexcept
{
    return 1. - static_cast<double>(
                    ranges::accumulate(std::forward<Rng>(rng), 0ul)) /
                    static_cast<double>(count);
}

template <std::invocable G, typename T, typename V>
double
estimate(const std::size_t n, G&& g, T&& t, V&& v) noexcept
{
    return estimate_error(gen_test_val(n,
                                       std::forward<G>(g),
                                       std::forward<T>(t),
                                       std::forward<V>(v)),
                          n);
}

template <typename Cmp = std::equal_to<>>
struct range_cmp_estimator
{
    template <std::ranges::input_range Lhs, std::ranges::input_range Rhs>
        requires(std::predicate<Cmp,
                                std::ranges::range_value_t<Lhs>,
                                std::ranges::range_value_t<Rhs>>)
    constexpr double
    operator()(const Lhs& lhs, const Rhs& rhs) const noexcept
    {
        return estimate_error(
            ranges::views::zip_with(Cmp{}, lhs, rhs),
            std::min(std::ranges::size(lhs), std::ranges::size(rhs)));
    }
};

/// @brief Helper class to make parametric tests.
struct ParmTestMaker
{
    /// @brief Number of runs.
    unsigned num_runs;

    ParmTestMaker(unsigned n) noexcept : num_runs(n)
    {
        fmt::print("Number of test iterations {}\n", n);
    }

    /// @brief Make standard parametric test.
    ///
    /// @param g test data generator. If @p t accepts few parameters must return
    /// std::pair or std::tuple.
    /// @param t testing function.
    /// @param v validation function.
    ///
    /// The check process is following:
    ///
    /// 1. Generate test data with @p g.
    ///
    /// 2. Invoke test @p t with the data.
    ///
    /// 3. Validate test results by invoking @p v with the data and the result
    /// of test invocation deciding if check was successful.
    ///
    /// To pass requires all runs to succeed.
    template <std::invocable                         G,
              invopplicable<std::invoke_result_t<G>> T,
              typename V>
    auto
    test(G&& g, T&& t, V&& v) const noexcept
    {
        return [&]() noexcept
        {
            return std::ranges::all_of(gen_test_val(num_runs,
                                                    std::forward<G>(g),
                                                    std::forward<T>(t),
                                                    std::forward<V>(v)),
                                       std::identity{});
        };
    }

    /// @brief Make invariant parametric test.
    ///
    /// @param g test data generator. If @p t accepts few parameters must return
    /// std::pair or std::tuple.
    /// @param t testing function.
    /// @param v validation function.
    ///
    /// Unlike in ParmTestMaker::test validation can be done without test data.
    /// Only the result of test run @p t is passed to validator @p v.
    template <std::invocable                         G,
              invopplicable<std::invoke_result_t<G>> T,
              typename V>
    auto
    invariant(G&& g, T&& t, V&& v) const noexcept
    {
        return [&]() noexcept
        {
            return std::ranges::all_of(gen_test_val_inv(num_runs,
                                                        std::forward<G>(g),
                                                        std::forward<T>(t),
                                                        std::forward<V>(v)),
                                       std::identity{});
        };
    }

    /// @brief Make parametric test with a hint.
    ///
    /// @param gh generates a pair of values. The first value goes to testing
    /// function, the second value is the hint.
    /// @param t testing function.
    /// @param cmp comparator. By default checks for equality.
    ///
    /// This case is useful when it's easier to generate a valid result
    /// immediately along with test data.
    ///
    /// Here generator @p gh produces a pair of value.
    /// The first value is then passed to @p t, the second is supposed to be a
    /// reference value
    template <std::invocable GH, typename T, typename Cmp = std::equal_to<>>
    auto
    with_hint(GH&& gh, T&& t, Cmp cmp = Cmp{}) const noexcept
    {
        return test(
            std::forward<GH>(gh),
            [t = std::move(t)]<typename Feed>(Feed&& feed) noexcept
            {
                return invopply(std::move(t),
                                std::get<0>(std::forward<Feed>(feed)));
            },
            [cmp](auto tr, const auto& feed) noexcept
            { return cmp(tr, std::get<1>(feed)); });
    }

    /// @brief Make parametric test with a reference implementation.
    ///
    /// @param g test data generator. If @p t accepts few parameters must return
    /// std::pair or std::tuple.
    /// @param t testing function.
    /// @param ri reference implementation, always produces correct results.
    /// @param cmp comparator. By default checks for equality.
    ///
    /// The test is passed when comparator @p cmp invoked with the results
    /// produced by @p t and @p ri with the same data returns true.
    template <std::invocable                         G,
              invopplicable<std::invoke_result_t<G>> T,
              invopplicable<std::invoke_result_t<G>> RI,
              typename Cmp = std::equal_to<>>
    auto
    with_ref(G&& g, T&& t, RI ri, Cmp cmp = Cmp{}) const noexcept
    {
        return test(
            std::forward<G>(g),
            std::forward<T>(t),
            [&, ri, cmp]<typename Tr, typename Gr>(Tr&& tr, Gr&& gr) noexcept
            {
                return cmp(std::forward<Tr>(tr),
                           invopply(ri, std::forward<Gr>(gr)));
            });
    }

    template <std::invocable G, typename T, typename V = std::identity>
    auto
    rng_invariant(G&& g, T&& t, V&& v = V{}) const noexcept
    {
        return [&]() noexcept
        {
            return std::ranges::all_of(gen_test_val_rng_inv(num_runs,
                                                            std::forward<G>(g),
                                                            std::forward<T>(t),
                                                            std::forward<V>(v)),
                                       std::identity{});
        };
    }

    /// @brief Make parametric test for function producing ranges.
    ///
    /// @tparam do_sort sort testing and validation ranges before comparison.
    /// @copydetails with_ref
    ///
    /// Practically the same as ParmTestMaker::with_ref,
    /// but here @p t and @p ri produce
    /// ranges thus @p cmp must be able to compare ranges.
    template <bool           do_sort = true,
              std::invocable G,
              typename T,
              typename RI,
              typename Cmp = decltype(std::ranges::equal)>
    auto
    rng_with_ref(G&&  g,
                 T&&  t,
                 RI&& ri,
                 Cmp  cmp = std::ranges::equal) const noexcept
    {
#define LAMBDA_BODY(MY_V)                                                   \
    const auto              args = multi_gen(num_runs, std::forward<G>(g)); \
    std::ranges::range auto tr   = invopply(std::forward<T>(t), args);      \
    std::ranges::range auto vr   = invopply(MY_V, args);                    \
    if constexpr(do_sort)                                                   \
    {                                                                       \
        std::ranges::sort(tr);                                              \
        std::ranges::sort(vr);                                              \
    }                                                                       \
    return std::invoke(cmp, tr, vr)

        if constexpr(std::is_rvalue_reference_v<decltype(ri)>)
            return [&, ri = std::move(ri), cmp]() noexcept { LAMBDA_BODY(ri); };
        else
            return [&, cmp]() noexcept { LAMBDA_BODY(std::forward<RI>(ri)); };

#undef LAMBDA_BODY
    }

    /// @brief Special case of ParmTestMaker::rng_with_ref:
    /// reference implementation produces scalar values.
    /// @copydetails rng_with_ref
    template <bool           do_sort = true,
              std::invocable G,
              typename T,
              typename RI,
              typename Cmp = decltype(std::ranges::equal)>
    auto
    rng_with_scalar_ref(G&&  g,
                        T&&  t,
                        RI&& ri,
                        Cmp  cmp = std::ranges::equal) const noexcept
    {
        return rng_with_ref<do_sort>(
            std::forward<G>(g),
            std::forward<T>(t),
            [&ri, n = num_runs]<std::ranges::input_range... Grs>(
                const Grs&... grs) noexcept
            {
                auto view =
                    ranges::views::zip(grs...) |
                    std::views::transform(
                        [&ri](const auto& t) noexcept
                        { return std::apply(std::forward<RI>(ri), t); });
                std::vector<std::ranges::range_value_t<decltype(view)>> out;
                out.reserve(n);
                std::ranges::copy(view, std::back_inserter(out));
                return out;
            },
            cmp);
    }

    /// @brief Fuzzy version of ParmTestMaker::test, i.e. a portion of runs
    /// is allowed to fail. Use with NamedFuzzyTest.
    ///
    /// @param g test data generator. If @p t accepts few parameters must return
    /// std::pair or std::tuple.
    /// @param t testing function.
    /// @param v validation function.
    template <std::invocable G, typename T, typename V>
    auto
    fuzzy(G&& g, T&& t, V&& v) const noexcept
    {
        if constexpr(std::is_rvalue_reference_v<decltype(v)>)
            return [&g, &t, v = std::move(v), n = num_runs]() noexcept
            {
                return estimate(n,
                                std::forward<G>(g),
                                std::forward<T>(t),
                                std::move(v));
            };
        else
            return [&, n = num_runs]() noexcept
            {
                return estimate(n,
                                std::forward<G>(g),
                                std::forward<T>(t),
                                std::forward<V>(v));
            };
    }

    /// @brief Fuzzy version of ParmTestMaker::with_ref.
    /// @copydetails with_ref
    template <typename G,
              typename T,
              typename RI,
              typename Cmp = std::equal_to<>>
    auto
    fuzzy_with_ref(G&& g, T&& t, RI&& ri, Cmp cmp = Cmp{}) const noexcept
    {
        if constexpr(std::is_rvalue_reference_v<decltype(ri)>)
            return fuzzy(
                std::forward<G>(g),
                std::forward<T>(t),
                [ri = std::move(ri),
                 cmp]<typename Tr, typename Gr>(Tr&& tr, Gr&& gr) noexcept
                {
                    return std::invoke(cmp,
                                       std::forward<Tr>(tr),
                                       invopply(ri, std::forward<Gr>(gr)));
                });
        else
            return fuzzy(
                std::forward<G>(g),
                std::forward<T>(t),
                [&, cmp]<typename Tr, typename Gr>(Tr&& tr, Gr&& gr) noexcept
                {
                    return std::invoke(cmp,
                                       std::forward<Tr>(tr),
                                       invopply(ri, std::forward<Gr>(gr)));
                });
    }

    /// @brief Special case for ParmTestMaker::fuzzy_with_ref with
    /// loom::ulp_cmp as the comparator.
    /// @copydetails fuzzy_with_ref
    ///
    /// @param ulp unit in the last place.
    template <typename G, typename T, typename RI>
    auto
    fuzzy_ulp(G&& g, T&& t, RI&& ri, const unsigned ulp) const noexcept
    {
        return fuzzy_with_ref(std::forward<G>(g),
                              std::forward<T>(t),
                              std::forward<RI>(ri),
                              ulp_cmp{ulp});
    }

    /// @brief Fuzzy version of rng_with_ref.
    /// @copydetails rng_with_ref
    /// @tparam comparison function for elements.
    template <bool do_sort     = true,
              typename ElemCmp = std::equal_to<>,
              std::invocable G,
              typename T,
              typename RI>
    auto
    fuzzy_rng_with_ref(G&& g, T&& t, RI&& ri) const noexcept
    {
        return rng_with_ref<do_sort>(std::forward<G>(g),
                                     std::forward<T>(t),
                                     std::forward<RI>(ri),
                                     range_cmp_estimator<ElemCmp>{});
    }

    /// @brief Fuzzy version of rng_with_scalar_ref.
    /// @copydetails fuzzy_rng_with_ref
    template <bool do_sort     = true,
              typename ElemCmp = std::equal_to<>,
              std::invocable G,
              typename T,
              typename RI>
    auto
    fuzzy_rng_with_scalar_ref(G&& g, T&& t, RI&& ri) const noexcept
    {
        return rng_with_scalar_ref<do_sort>(std::forward<G>(g),
                                            std::forward<T>(t),
                                            std::forward<RI>(ri),
                                            range_cmp_estimator<ElemCmp>{});
    }
};

template <typename T>
using random_dist = detail::random_dist<T>::type;

struct RandomState
{
    std::default_random_engine g;

    RandomState() noexcept : g(std::random_device()()) {}

    template <typename T, typename... Args>
    auto
    make_gen(Args&&... args) noexcept
    {
        if constexpr(std::is_floating_point_v<T> && !sizeof...(Args))
            return [&]() noexcept { return std::generate_canonical<T, 10>(g); };
        else
            return [&,
                    d = random_dist<T>(std::forward<Args>(
                        args)...)]() mutable noexcept { return d(g); };
    }
};

template <unsigned def = 1000>
unsigned
parse_numruns(int argc, char* argv[]) noexcept
{
    return argparse::parse(std::tuple{argparse::option<'n', "numruns">(
                               "number of runs for parametric tests",
                               "N",
                               def,
                               read_number<unsigned, true>)},
                           {argc, argv})
        .template get_opt<'n'>();
}

struct transparent_id
{
    template <typename T>
    bool
    operator()(T val) const noexcept
    {
        if constexpr(is_tuple_v<T>)
            fmt::print("{}\n", val);
        else
            fmt::print("{:>8}\n", val);
        return val;
    }
};

template <typename Cmp = std::equal_to<>>
struct transparent_cmp
{
    static constexpr Cmp cmp{};

    template <typename T>
    bool
    operator()(const T& a, const T& b) const noexcept
    {
        const bool r = cmp(a, b);
        if(!r)
        {
            if constexpr(is_tuple_v<T>)
                fmt::print("{} != {}\n", a, b);
            else
                fmt::print("{:>8} != {:>8}\n", a, b);
        }

        return r;
    }
};

template <typename Cmp = std::equal_to<>>
struct transparent_range_cmp
{
    static constexpr Cmp cmp{};

    template <std::ranges::range A, std::ranges::range B>
    bool
    operator()(A&& a, B&& b) const noexcept
    {
        return std::ranges::equal(
            std::forward<A>(a),
            std::forward<B>(b),
            [](const auto& a, const auto& b) noexcept
            {
                const bool r = cmp(a, b);
                if(!r)
                {
                    if constexpr(is_tuple_v<std::remove_cvref_t<decltype(a)>> ||
                                 is_tuple_v<std::remove_cvref_t<decltype(b)>>)
                        fmt::print("{} != {}\n", a, b);
                    else
                        fmt::print("{:>8} != {:>8}\n", a, b);
                }
                return r;
            });
    }
};

template <typename Tuple, template <typename> typename Cmp = std::equal_to>
struct tuple_cmp;

template <typename T1, typename T2, template <typename> typename Cmp>
struct tuple_cmp<std::pair<T1, T2>, Cmp>
{
    using t = std::pair<T1, T2>;

    Cmp<T1> cmp1;
    Cmp<T2> cmp2;

    constexpr bool
    operator()(const t& a, const t& b) const noexcept
    {
        return cmp1(a.first, b.first) && cmp2(a.second, b.second);
    }
};

template <typename... Ts, template <typename> typename Cmp>
struct tuple_cmp<std::tuple<Ts...>, Cmp>
{
    using t = std::tuple<Ts...>;

    std::tuple<Cmp<Ts>...> cmps;

    constexpr bool
    operator()(const t& a, const t& b) const noexcept
    {
        return detail::tuple_cmp_impl(a,
                                      b,
                                      cmps,
                                      std::index_sequence_for<Ts...>{});
    }
};

template <typename T>
struct poly_cmp : std::equal_to<T>
{
};

template <std::floating_point T>
struct poly_cmp<T> : ulp_cmp
{
};
} // namespace loom::test
