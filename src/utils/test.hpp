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

#define _OK(r)               \
    fmt::styled(             \
        r ? "OK  " : "FAIL", \
        fmt::fg(r ? fmt::terminal_color::green : fmt::terminal_color::red))

namespace lucid::test
{
class Track
{
    unsigned m_total{};
    unsigned m_passed{};
    unsigned m_parm_n;

    template <std::predicate A>
    void
    check(std::string_view name, A a) noexcept
    {
        ++m_total;
        m_passed += std::invoke(std::forward<A>(a));
        fmt::print("{} {}\n", _OK(m_passed), name);
    }

    template <typename F, std::predicate<std::invoke_result_t<F>> A>
    void
    test_and_check(std::string_view name, F f, A a) noexcept
    {
        check(name, [&]() noexcept { return compose(a, f)(); });
    }

  public:
    Track()             = delete;
    Track(const Track&) = delete;
    Track(Track&&)      = delete;
    Track&
    operator=(const Track&) = delete;
    Track&
    operator=(Track&&) = delete;

    Track(int argc, char* argv[]) noexcept :
        m_parm_n(argparse::parse(
                     std::tuple{argparse::option<'n', "numruns">(
                         [](std::string_view word) noexcept
                         { return std::atoi(word.data()); },
                         1000,
                         "Number of runs of parametric tests. Default: 1000",
                         "N")},
                     {argc, argv})
                     .get_opt<'n'>())
    {
    }

    template <std::predicate A, typename... Rest>
    void
    checks(std::string_view name, A a, Rest... rest) noexcept
    {
        check(name, a);
        if constexpr(sizeof...(Rest)) checks(rest...);
    }

    template <typename F,
              std::predicate<std::invoke_result_t<F>> A,
              typename... Rest>
    void
    test_and_checks(std::string_view name, F f, A a, Rest... rest) noexcept
    {
        test_and_check(name, f, a);
        if constexpr(sizeof...(Rest)) test_and_checks(rest...);
    }

    template <typename Fz>
    void
    fuzzy_test(unsigned n, Fz s) noexcept
    {
        const double err = random_estimate(n, s.g, s.t, s.v);
        ++m_total;
        m_passed += err <= s.threshold;
        fmt::print("{:>4} {:.3f}% {}\n", _OK(m_passed), err, s.name);
    }

    int
    code() const noexcept
    {
        return m_passed != m_total;
    }

    ~Track()
    {
        int fail = code();
        fmt::print(fmt::emphasis::bold |
                       fmt::fg(fail ? fmt::terminal_color::red :
                                      fmt::terminal_color::green),
                   "{}/{} TESTS PASSED\n",
                   m_passed,
                   m_total);
    }
};

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

template <std::invocable<std::default_random_engine>              G,
          maybe_applicable<std::invoke_result_t<std::decay_t<G>>> T,
          typename V>
struct Fuzzy
{
    std::string_view name;
    double           threshold;
    G                g;
    T                t;
    V                v;
};

template <typename F = std::equal_to<>, typename A, typename B, typename... Ts>
bool
cmp_all(A a, B b, Ts... rest) noexcept
{
    bool ret = F{}(a, b);
    if constexpr(sizeof...(Ts)) ret &= cmp_all(rest...);
    return ret;
}
} // namespace lucid::test

#undef _OK
