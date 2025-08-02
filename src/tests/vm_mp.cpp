// -*- C++ -*-
// vm_mp.cpp
#include <utils/test.hpp>
#include <vm/bve.hpp>
#include <vm/mp.hpp>
#include <vm/simd.hpp>

#include "programs.hpp"
#include "programs_generators.hpp"
#include "root_finding.hpp"

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/zip_with.hpp>

#include <algorithm>
#include <vector>

using namespace lucid;
using namespace test;

template <typename P>
using BVEStreamCompiler = bve::stream<P, bve::INTROSORT>;

template <typename P>
using SIMDStreamCompiler =
    simd::stream<P, simd::MINIKERNEL, bve::COUNTING_SORT, true>;

template <typename P>
using BVEContext = mp::Context<P, BVEStreamCompiler>;

template <typename P>
using SIMDContext = mp::Context<P, SIMDStreamCompiler>;

using fac_p = factorial_int<std::uint64_t>;
using fib_p = fib<std::uint64_t>;
using fz_p  = cubic_fz_program<double, CS(10)>;

using programs = type_sequence<fac_p, fib_p, fz_p>;

template <std::floating_point T>
auto
gm_fz(RandomState& rs) noexcept
{
    constexpr T lbound = constants<T>::lbound;
    constexpr T rbound = constants<T>::rbound;

    return [g_a1 = [g_a = rs.make_gen<T>(1, rbound),
                    g_b = rs.make_gen<bool>(0.5)]() mutable noexcept
            { return g_a() * (g_b() ? T(-1) : T(1)); },
            g_f = rs.make_gen<T>(lbound, rbound)]() mutable noexcept
    { return std::tuple{g_a1(), g_f(), g_f(), g_f()}; };
}

template <typename G>
auto
gen_input(unsigned num_runs, G& g) noexcept
{
    std::vector<decltype(g())> v;
    v.reserve(num_runs);
    std::ranges::generate_n(std::back_inserter(v), num_runs, g);
    return v;
}

template <typename T, typename V>
auto
mk_val(const std::vector<T>& i, V v) noexcept
{
    auto l = [v](const auto& in) noexcept { return maybe_apply(v, in); };
    std::vector<decltype(l(i[0]))> ret;
    ret.reserve(i.size());
    std::ranges::transform(i, std::back_inserter(ret), l);
    std::ranges::sort(ret);
    return ret;
}

int
main(int argc, char* argv[])
{
    Track          track;
    RandomState    rs;
    const unsigned num_runs = parse_numruns<1000000>(argc, argv);

    auto g_fac = GM_2_10<std::uint64_t>{}(rs);
    auto g_fib = GM_FIB<std::uint64_t>{}(rs);
    auto g_fz  = gm_fz<double>(rs);

    const std::vector<input_type<fac_p>> in_fac = gen_input(num_runs, g_fac);
    const std::vector<input_type<fib_p>> in_fib = gen_input(num_runs, g_fib);
    const std::vector<input_type<fz_p>>  in_fz  = gen_input(num_runs, g_fz);

    const std::vector<output_type<fac_p>> out_v_fac = mk_val(in_fac, val_fac);
    const std::vector<output_type<fib_p>> out_v_fib = mk_val(in_fib, val_fib);
    const std::vector<output_type<fz_p>> out_v_fz = mk_val(in_fz, val_cubic_fz);

    std::vector<output_type<fac_p>> out_fac;
    std::vector<output_type<fib_p>> out_fib;
    std::vector<output_type<fz_p>>  out_fz;

    auto sort_outs = [&]() noexcept
    {
        std::ranges::sort(out_fac);
        std::ranges::sort(out_fib);
        std::ranges::sort(out_fz);
    };
    auto reset_outs = [&]() noexcept
    {
        out_fac.clear();
        out_fib.clear();
        out_fz.clear();

        out_fac.reserve(num_runs);
        out_fib.reserve(num_runs);
        out_fz.reserve(num_runs);
    };

    auto oc = mp::make_output_collection(out_fac, out_fib, out_fz);

    auto test_shop = [&](auto& shop) noexcept
    {
        auto ic = shop.bind_inputs(in_fac, in_fib, in_fz);
        {
            auto tp = shop.get_thread_pool();
            while(!mp::is_complete(ic)) shop.exchange(ic, oc);
            shop.set();
        }
        shop.finalize(oc);
        sort_outs();
        const double r_fac = std::ranges::equal(out_fac, out_v_fac) * num_runs;
        const double r_fib = std::ranges::equal(out_fib, out_v_fib) * num_runs;
        const double r_fz  = static_cast<double>(
            ranges::accumulate(ranges::views::zip_with(
                                   [](const output_type<fz_p>& lhs,
                                      const output_type<fz_p>& rhs) noexcept
                                   {
                                       const auto [lx, li] = lhs;
                                       const auto [rx, ri] = rhs;
                                       return ulp_cmp{10}(lx, rx) && li == ri;
                                   },
                                   out_fz,
                                   out_v_fz),
                               0ul));
        return 1. - (r_fac + r_fib + r_fz) / (num_runs * 3.);
    };

    track.check(NamedFuzzyTest(
        [&]() noexcept
        {
            mp::Shop<programs, SIMDContext, 4> shop(1000);
            return test_shop(shop);
        },
        "BVE static 4 threads",
        0.1));

    reset_outs();

    track.check(NamedFuzzyTest(
        [&]() noexcept
        {
            mp::Shop<programs, SIMDContext, dynamic_size> shop(4, 1000);
            return test_shop(shop);
        },
        "BVE dynamic 4 threads",
        0.1));

    return track.ret_code();
}
