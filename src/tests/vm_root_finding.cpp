// -*- C++ -*-
// lang_root_finding.cpp
#include "root_finding.hpp"
#include "stream_wrapper.hpp"

#include <common/test.hpp>
#include <common/typeinfo.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <vector>

using namespace loom;
using namespace vm;
using namespace test;

template <typename G, typename F>
double
val_fb_rng(F& f, G& g, unsigned n) noexcept
{
    std::vector<decltype(g())> in;
    in.reserve(n);
    std::ranges::generate_n(std::back_inserter(in), n, g);
    const auto r = f.template operator()<true>(in);
    return test::estimate_error(
        std::views::transform(r,
                              [](const auto& v) noexcept
                              {
                                  const auto [l, r] = v;
                                  return std::signbit(l) != std::signbit(r);
                              }),
        n);
}

template <std::floating_point T>
struct test_suite
{
    using cnsts                      = constants<T>;
    static constexpr T err_threshold = cnsts::err_threshold;
    static constexpr T lbound        = cnsts::lbound;
    static constexpr T rbound        = cnsts::rbound;
    using errt                       = cnsts::errt;

    using eval_p           = eval_program<T>;
    using root_condition_p = root_condition_program<T>;
    using find_bounds_p    = find_bounds_program<T>;
    using cubic_fz_p       = bisection<T, errt>;

    static constexpr auto ti = TypeInfo<T>::chars;

    Track&
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const
    {
        auto g_f  = rs.make_gen<T>(lbound, rbound);
        auto g_b  = rs.make_gen<bool>(0.5);
        auto g_a  = rs.make_gen<T>(1, rbound);
        auto g_a1 = [&]() noexcept { return g_a() * (g_b() ? T(-1) : T(1)); };
        auto g_c4 = [&]() noexcept
        { return std::tuple{g_a1(), g_f(), g_f(), g_f()}; };
        auto g_cubic = [&]() noexcept
        { return std::tuple{g_a1(), g_f(), g_f(), g_f(), g_f()}; };
        auto g_cubrc = [&]() noexcept
        {
            bool b = g_b();
            return std::tuple{g_a1(),
                              g_f(),
                              g_f(),
                              g_f(),
                              b ? T(-100) : T(100),
                              b ? T(100) : T(-100)};
        };

        bse::function<eval_p>           bse_cubic;
        bse::function<root_condition_p> bse_root_condition;
        bse::function<find_bounds_p>    bse_find_bounds;
        bse::function<cubic_fz_p>       bse_cubic_fz;

        simd::function<find_bounds_p, simd::WAVEFRONT, simd::bve_INTROSORT>
            simd_find_bounds(tm.num_runs);
        simd::function<cubic_fz_p, simd::WAVEFRONT, simd::bve_INTROSORT>
            simd_cubic_fz(tm.num_runs);

        simd::stream<cubic_fz_p, simd::MASKED, simd::COUNTING_SORT, true>
                      simd_stream_fz(tm.num_runs / 4);
        StreamWrapper simd_stream_wrapper_fz{&simd_stream_fz};

        track.check(
            NamedFuzzyTest(tm.fuzzy_ulp(g_cubic, bse_cubic, val_eval, 10),
                           fmt::format("({}) BSE cubic eval", ti),
                           1e-2));
        track.check(NamedTest(
            tm.with_ref(g_cubrc, bse_root_condition, val_root_condition),
            fmt::format("({}) BSE cubic root condition", ti)));
        track.check(
            NamedTest(tm.with_ref(g_c4, bse_find_bounds, val_find_bounds),
                      fmt::format("({}) BSE cubic find bounds", ti)));
        track.check(NamedFuzzyTest(
            tm.fuzzy_with_ref(
                g_c4,
                bse_cubic_fz,
                val_cubic_fz,
                tuple_cmp<std::tuple<T, uint_of_size_t<sizeof(T)>>, poly_cmp>{
                    {{5u}, {}}
        }),
            fmt::format("({}) BSE cubic find zero", ti),
            1e-2));

        track.check(NamedFuzzyTest(
            [&]() noexcept
            { return val_fb_rng(simd_find_bounds, g_c4, tm.num_runs); },
            fmt::format("({}) SIMD cubic find bounds", ti),
            1e-2));
        track.check(NamedFuzzyTest(
            tm.fuzzy_rng_with_scalar_ref(
                g_c4,
                [&simd_cubic_fz, n = tm.num_runs](auto&&... vs) noexcept
                {
                    auto view = simd_cubic_fz.template operator()<true>(
                        std::forward<decltype(vs)>(vs)...);
                    std::vector<output_type<cubic_fz_p>> out;
                    out.reserve(n);
                    std::ranges::copy(view, std::back_inserter(out));
                    return out;
                },
                val_cubic_fz),
            fmt::format("({}) SIMD cubic find zero", ti),
            1e-2));
        track.check(NamedFuzzyTest(
            [&simd_stream_wrapper_fz, &g_c4, n = tm.num_runs]() noexcept
            {
                std::vector<input_type<cubic_fz_p>> grs;
                grs.reserve(n);
                std::ranges::generate_n(std::back_inserter(grs), n, g_c4);

                auto vals = simd_stream_wrapper_fz(grs);
                std::ranges::sort(vals);

                std::vector<output_type<cubic_fz_p>> ts;
                ts.reserve(n);
                std::ranges::transform(
                    grs,
                    std::back_inserter(ts),
                    [](const auto& gs) noexcept
                    { return std::apply(val_cubic_fz, gs); });
                std::ranges::sort(ts);
                return range_cmp_estimator<>{}(vals, ts);
            },
            fmt::format("({}) SIMD stream find zero", ti),
            1e-2));

        return track;
    }
};

int
main(int argc, char* argv[])
{
    simd::print_info();
    Track         track;
    ParmTestMaker tm{align_top(parse_numruns<10000>(argc, argv), 64u)};
    RandomState   rs;

    test_suite<float>  tsf;
    test_suite<double> tsd;
    track = tsd(track, tm, rs);
    track = tsf(track, tm, rs);

    return track.ret_code();
}
