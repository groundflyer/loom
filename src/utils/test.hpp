// -*- C++ -*-
// test.hpp
//

#pragma once

#include "tuple.hpp"

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/generate_n.hpp>

#include <fmt/color.h>

#include <concepts>
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

#define CHECK(...) \
    ++total;                 \
    passed += lucid::test::check_assertion(__VA_ARGS__);
