// -*- C++ -*-
// lang_root_finding.cpp
#include "root_finding.hpp"

#include <lang/bse.hpp>
#include <utils/test.hpp>
#include <utils/typeinfo.hpp>

#include <fmt/core.h>

using namespace lucid;
using namespace lucid::test;

template <std::floating_point T>
struct test_suite
{
    using cnsts                      = constants<T>;
    static constexpr T err_threshold = cnsts::err_threshold;
    static constexpr T lbound        = cnsts::lbound;
    static constexpr T rbound        = cnsts::rbound;
    using errt                       = cnsts::errt;

    using cubic_eval_p = cubic_eval_program<T>;
    using cubic_rc_p   = cubic_rc_program<T>;
    using cubic_fb_p   = cubic_fb_program<T>;
    using cubic_fz_p   = cubic_fz_program<T, errt>;

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

        bse::function<cubic_eval_p> bse_cubic;
        bse::function<cubic_rc_p>   bse_cubic_rc;
        bse::function<cubic_fb_p>   bse_cubic_fb;
        bse::function<cubic_fz_p>   bse_cubic_fz;

        track.check(
            NamedFuzzyTest(tm.fuzzy_ulp(g_cubic, bse_cubic, val_cubic_eval, 10),
                           fmt::format("({}) BSE cubic eval", ti),
                           1e-2));
        track.check(
            NamedTest(tm.with_ref(g_cubrc, bse_cubic_rc, val_cubic_rc),
                      fmt::format("({}) BSE cubic root condition", ti)));
        track.check(NamedTest(tm.with_ref(g_c4, bse_cubic_fb, val_cubic_fb),
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

        return track;
    }
};

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns<100000>(argc, argv)};
    RandomState   rs;

    test_suite<float>  tsf;
    test_suite<double> tsd;
    track = tsd(track, tm, rs);
    track = tsf(track, tm, rs);

    return track.ret_code();
}
