// -*- C++ -*-
// test.hpp
//

#pragma once

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

namespace lucid::test
{
template <std::invocable G, typename T, typename V>
double
estimate(const unsigned n, G&& g, T t, V v) noexcept
{
    return static_cast<double>(ranges::accumulate(
               ranges::views::generate_n(std::forward<G>(g), n) |
                   std::ranges::transform(
                       [&](auto feed) noexcept -> std::size_t
                       { return v(lucid::maybe_apply(t, feed), feed); }),
               0u)) /
           static_cast<double>(n);
}

template <std::invocable<std::default_random_engine> G, typename T, typename V>
double
random_estimate(const unsigned n, G&& g, T t, V v) noexcept
{
    std::random_device         rd;
    std::default_random_engine e(rd());
    return estimate(
        n,
        [&]() noexcept { return std::invoke(std::forward<G>(g), e); },
        std::forward<T>(t),
        std::forward<V>(v));
}

#define _OK(r)               \
    fmt::styled(             \
        r ? "OK  " : "FAIL", \
        fmt::fg(r ? fmt::terminal_color::green : fmt::terminal_color::red))

template <std::predicate A>
int
check_assertion(std::string_view name, A&& a) noexcept
{
    const bool passed = std::invoke(std::forward<A>(a));
    fmt::print("{} {}\n", _OK(passed), name);
    return passed;
}

template <typename F, std::predicate<std::invoke_result_t<F>> A>
int
test_and_check(std::string_view name, F&& f, A&& a) noexcept
{
    return check_assertion(name, [&]() noexcept { return a(f()); });
}

template <std::invocable<std::default_random_engine>              G,
          maybe_applicable<std::invoke_result_t<std::decay_t<G>>> T,
          typename V>
struct TestSuite
{
    std::string_view name;
    double           threshold;
    G                g;
    T                t;
    V                v;
};

template <typename TS>
int
check_suite(unsigned n, TS s) noexcept
{
    const double err    = random_estimate(n, s.g, s.t, s.v);
    const bool   passed = err <= s.threshold;
    fmt::print("{:>4} {:.3f}% {}\n", _OK(passed), err, s.name);
    return passed;
}

template <typename T, typename R>
class matcher
{
    T _val;
    R _r{};

  public:
    constexpr matcher(T val, R r = R{}) noexcept : _val(val), _r(r) {}

    constexpr bool
    operator()(const T& test) const noexcept
    {
        return std::invoke(_r, _val, test);
    }
};

template <typename T1, typename T2, typename R>
class matcher<std::pair<T1, T2>, R>
{
    using value_type = std::pair<T1, T2>;
    value_type _val;
    R          _r{};

    constexpr bool
    operator()(const T1& t1, const T2& t2) const noexcept
    {
        return matcher<T1, R>{_val.first, _r}(t1) &&
               matcher<T2, R>{_val.second}(t2);
    }

  public:
    constexpr matcher(const value_type& val, R r = R{}) noexcept :
        _val(val), _r(r)
    {
    }

    constexpr bool
    operator()(const value_type& test) const noexcept
    {
        return operator()(test.first, test.second);
    }
};

template <typename... Ts, typename R>
class matcher<std::tuple<Ts...>, R>
{
    using value_type = std::tuple<Ts...>;
    value_type _val;
    R          _r{};

    template <typename F, std::size_t... Idxs>
    constexpr bool
    match_elems(const value_type& test,
                std::index_sequence<Idxs...>) const noexcept
    {
        return (... && matcher<std::tuple_element_t<Idxs, value_type>, R>{
                           std::get<Idxs>(_val),
                           _r}(std::get<Idxs>(test)));
    }

  public:
    constexpr matcher(const value_type& val, R r = R{}) noexcept :
        _val(val), _r(r)
    {
    }

    constexpr bool
    operator()(const value_type& test) const noexcept
    {
        return match_elems(test, std::index_sequence_for<Ts...>{});
    }
};
} // namespace lucid::test

#undef _OK

#define TESTS_BEGIN     \
    int main()          \
    {                   \
        int passed = 0; \
        int total  = 0;

#define TESTS_END                                             \
    const int ret = passed != total;                          \
    fmt::print(fmt::emphasis::bold |                          \
                   fmt::fg(ret ? fmt::terminal_color::red :   \
                                 fmt::terminal_color::green), \
               "{}/{} TESTS PASSED\n",                        \
               passed,                                        \
               total);                                        \
    return ret;                                               \
    }

#define EQUAL(VAL) \
    lucid::test::matcher<std::decay_t<decltype(VAL)>, std::equal_to<>>(VAL)

#define CHECK(...) \
    ++total;       \
    passed += lucid::test::check_assertion(__VA_ARGS__);

#define CHECK_EQ(name, val, ...) \
    CHECK(name, [&]() noexcept { return EQUAL(val)(__VA_ARGS__()); });

#define CHECK_EQ_PAIR(name, v1, v2, ...) \
    CHECK_EQ(name, (std::pair{v1, v2}), __VA_ARGS__)
