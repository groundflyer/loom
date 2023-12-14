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

#define _OK(r)             \
    fmt::styled(           \
        r ? "OK" : "FAIL", \
        fmt::fg(r ? fmt::terminal_color::green : fmt::terminal_color::red))

namespace lucid::test
{
class Track
{
    unsigned m_total{};
    unsigned m_passed{};
    unsigned m_parm_n;

    bool
    inc(bool p) noexcept
    {
        m_passed += p;
        ++m_total;
        return p;
    }

    bool
    status(std::string_view name, bool p) noexcept
    {
        fmt::print("{:<4} {:>8} {}\n", _OK(p), ' ', name);
        return inc(p);
    }

    bool
    fuzzy_status(std::string_view name, double threshold, double error) noexcept
    {
        const bool p = error <= threshold;
        fmt::print("{:<4} {:>7.3f}% {}\n", _OK(p), error * 100., name);
        return inc(p);
    }

    template <std::predicate A>
    bool
    check(std::string_view name, A&& a) noexcept
    {
        return status(name, std::invoke(std::forward<A>(a)));
    }

    template <typename F, std::predicate<std::invoke_result_t<F>> A>
    bool
    test_and_check(std::string_view name, F&& f, A&& a) noexcept
    {
        return status(
            name,
            std::invoke(compose(std::forward<A>(a), std::forward<F>(f))));
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
    checks(std::string_view name, A&& a, Rest&&... rest) noexcept
    {
        bool r = check(name, std::forward<A>(a));
        if constexpr(sizeof...(Rest))
            return r && checks(std::forward<Rest>(rest)...);
    }

    template <std::predicate<unsigned> A>
    bool
    check_n(std::string_view name, A&& a) noexcept
    {
        return status(name, std::invoke(std::forward<A>(a), m_parm_n));
    }

    template <typename F,
              std::predicate<std::invoke_result_t<F>> A,
              typename... Rest>
    bool
    test_and_checks(std::string_view name,
                    F&&              f,
                    A&&              a,
                    Rest&&... rest) noexcept
    {
        bool r = test_and_check(name, std::forward<F>(f), std::forward<A>(a));
        if constexpr(sizeof...(Rest))
            return r && test_and_checks(std::forward<Rest>(rest)...);
    }

    template <typename... GTV>
    bool
    parm_test(std::string_view name, GTV&&... gtv) noexcept
    {
        return status(name,
                      std::ranges::all_of(
                          gen_test_val(m_parm_n, std::forward<GTV>(gtv)...),
                          std::identity{}));
    }

    template <std::invocable                            G,
              maybe_applicable<std::invoke_result_t<G>> T,
              maybe_applicable<std::invoke_result_t<G>> ValT,
              typename Cmp = std::equal_to<>>
    bool
    parm_test_val_cmp(std::string_view name,
                      G&&              g,
                      T&&              t,
                      ValT             valt,
                      Cmp              cmp = Cmp{})
    {
        return parm_test(
            name,
            std::forward<G>(g),
            std::forward<T>(t),
            [&]<typename Tr, typename Gr>(Tr&& tr, Gr&& gr) noexcept
            { return cmp(std::forward<Gr>(tr), valt(std::forward<Gr>(gr))); });
    }

    template <std::invocable G,
              typename T,
              typename V,
              typename Cmp = decltype(std::ranges::equal)>
    bool
    parm_range_test(std::string_view name,
                    G&&              g,
                    T&&              t,
                    V&&              v,
                    Cmp              cmp = std::ranges::equal) noexcept
    {
        std::vector<std::invoke_result_t<G>> in;
        in.reserve(m_parm_n);
        std::ranges::generate_n(std::back_inserter(in),
                                m_parm_n,
                                std::forward<G>(g));
        std::ranges::range auto tr =
            std::invoke(std::forward<T>(t), m_parm_n, in);
        std::ranges::range auto vr =
            std::invoke(std::forward<V>(v), m_parm_n, in);
        std::ranges::sort(tr);
        std::ranges::sort(vr);
        return status(name, std::invoke(std::forward<Cmp>(cmp), tr, vr));
    }

    template <std::invocable G,
              typename T,
              typename V,
              typename Cmp = decltype(std::ranges::equal)>
    bool
    parm_range_test_gen(std::string_view name,
                        G&&              g,
                        T&&              t,
                        V&&              v,
                        Cmp              cmp = std::ranges::equal) noexcept
    {
        return parm_range_test(
            name,
            g,
            t,
            [&]<std::ranges::range In>(unsigned n, In&& in) noexcept
            {
                std::vector<
                    std::invoke_result_t<std::decay_t<V>,
                                         std::ranges::range_value_t<In>>>
                    out;
                out.reserve(n);
                std::ranges::transform(std::forward<In>(in),
                                       std::back_inserter(out),
                                       v);
                return out;
            },
            std::forward<Cmp>(cmp));
    }

    template <typename... GTV>
    bool
    fuzzy_parm_test(std::string_view name,
                    double           threshold,
                    GTV&&... gtv) noexcept
    {
        return fuzzy_status(name,
                            threshold,
                            estimate(m_parm_n, std::forward<GTV>(gtv)...));
    }

    template <typename G,
              typename T,
              typename ValT,
              typename Cmp = std::equal_to<>>
    bool
    fuzzy_parm_test_val_cmp(std::string_view name,
                            double           threshold,
                            G&&              g,
                            T&&              t,
                            ValT             valt,
                            Cmp              cmp = Cmp{}) noexcept
    {
        return fuzzy_parm_test(
            name,
            threshold,
            std::forward<G>(g),
            std::forward<T>(t),
            [&]<typename Tr, typename Gr>(Tr&& tr, Gr&& gr) noexcept {
                return std::invoke(cmp,
                                   std::forward<Tr>(tr),
                                   valt(std::forward<Gr>(gr)));
            });
    }

    template <typename G, typename T, typename ValT>
    bool
    fuzzy_parm_test_ulp(std::string_view name,
                        double           threshold,
                        G&&              g,
                        T&&              t,
                        ValT             valt,
                        const unsigned   ulp) noexcept
    {
        return fuzzy_parm_test_val_cmp(name,
                                       threshold,
                                       std::forward<G>(g),
                                       std::forward<T>(t),
                                       valt,
                                       [&](auto t, auto v) noexcept
                                       { return almost_equal(t, v, ulp); });
    }

    int
    code() const noexcept
    {
        return m_passed != m_total;
    }

    ~Track()
    {
        fmt::print(fmt::emphasis::bold |
                       fmt::fg(code() ? fmt::terminal_color::red :
                                        fmt::terminal_color::green),
                   "{}/{} TESTS PASSED\n",
                   m_passed,
                   m_total);
    }
};

template <std::invocable G, typename T, typename V>
auto
gen_test_val(const unsigned n, G&& g, T&& t, V&& v) noexcept
{
    return ranges::views::generate_n(std::forward<G>(g), n) |
           std::ranges::views::transform(
               [&](auto feed) noexcept
               {
                   return std::invoke(
                       std::forward<V>(v),
                       lucid::maybe_apply(std::forward<T>(t), feed),
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

template <typename F = std::equal_to<>, typename A, typename B, typename... Ts>
bool
cmp_all(A a, B b, Ts... rest) noexcept
{
    bool ret = F{}(a, b);
    if constexpr(sizeof...(Ts)) ret &= cmp_all(rest...);
    return ret;
}

namespace detail
{
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

template <typename T>
using random_dist = typename detail::random_dist<T>::type;

class RandomState
{
    std::default_random_engine m_e;

  public:
    RandomState() noexcept : m_e(std::random_device()()) {}

    template <typename T, typename... Args>
    auto
    make_gen(Args&&... args) noexcept
    {
        return
            [&,
             d = random_dist<T>(std::forward<Args>(args)...)]() mutable noexcept
        { return d(m_e); };
    }
};
} // namespace lucid::test

#undef _OK
