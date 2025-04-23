// -*- C++ -*-
// lang_root_finding.cpp
#include "root_finding.hpp"

#include <lang/bse.hpp>
#include <utils/test.hpp>

using cubic_eval_f = cubic_eval_program<float>;
using cubic_rc_f   = cubic_rc_program<float>;
using cubic_fb_f   = cubic_fb_program<float>;
using cubic_fz_f   = cubic_fz_program<float>;

using namespace lucid;
using namespace lucid::test;

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;
    auto          g_float = rs.make_gen<float>(-10.f, 10.f);
    auto          g_b     = rs.make_gen<bool>(0.5);
    auto          g_a     = rs.make_gen<float>(1.f, 10.f);
    auto g_a1 = [&]() noexcept { return g_a() * (g_b() ? -1.f : 1.f); };
    auto g_c4 = [&]() noexcept
    { return std::tuple{g_a1(), g_float(), g_float(), g_float()}; };
    auto g_cubic = [&]() noexcept
    { return std::tuple{g_a1(), g_float(), g_float(), g_float(), g_float()}; };
    auto g_cubrc = [&]() noexcept
    {
        bool b = g_b();
        return std::tuple{g_a1(),
                          g_float(),
                          g_float(),
                          g_float(),
                          b ? -100.f : 100.f,
                          b ? 100.f : -100.f};
    };

    bse::function<cubic_eval_f> bse_cubic;
    bse::function<cubic_rc_f>   bse_cubic_rc;
    bse::function<cubic_fb_f>   bse_cubic_fb;
    bse::function<cubic_fz_f>   bse_cubic_fz;

    track.check(NamedTest(tm.with_ref(g_cubic, bse_cubic, val_cubic_eval),
                          "BSE cubic eval"));
    track.check(NamedTest(tm.with_ref(g_cubrc, bse_cubic_rc, val_cubic_rc),
                          "BSE cubic root condition"));
    track.check(NamedTest(tm.with_ref(g_c4, bse_cubic_fb, val_cubic_fb),
                          "BSE cubic find bounds"));
    track.check(NamedTest(
        tm.with_ref(g_c4, bse_cubic_fz, val_cubic_fz, transparent_cmp{}),
        "BSE cubic find zero"));

    return track.ret_code();
}
