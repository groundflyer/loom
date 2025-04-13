// -*- C++ -*-
// lang_root_finding.cpp
#include <lang/bse.hpp>
#include <lang/macros.hpp>
#include <utils/functional.hpp>
#include <utils/test.hpp>

// clang-format off
// a * x^3 + b * x^2 + c * x + d
TMACRO_BEGIN4(cubic_eval, result, x, x2, x3)
    TMUL_CS(x2, x, x),
    TMUL_CS(x3, x2, x),
    TMUL_CS(result, CS(a), x3),
    TMUL_CS(x3, CS(b), x2),
    TADD_CS(result, result, x3),
    TMUL_CS(x3, CS(c), x),
    TADD_CS(result, result, x3),
    TADD_CS(result, result, CS(d))
MACRO_END;

// signbit(eval_at(ks, l)) != signbit(eval_at(ks, r))
// root condition
TMACRO_BEGIN4(cubic_rc, result, t1, t2, t3)
    INSERT_TMACRO(cubic_eval, result, CS(l), t2, t3),
    TLT_CS(result, result, CS(0)),
    INSERT_TMACRO(cubic_eval, t1, CS(r), t2, t3),
    TLT_CS(t1, t1, CS(0)),
    TNEQ_CS(result, result, t1)
MACRO_END;

// find_bounds
// l = -10
// r = 10
// rc = root_condition
// rc = rc != 0
// pjmp2 rc loop end
// loop:
// l *= 10
// r *= 10
// rc = root_condition
// rc = rc != 0
// pjmp2 rc loop end
// end
TMACRO_BEGIN4(cubic_fb, rc, t1, t2, t3)
    TMOV_CS(CS(l), CS(-10)),
    TMOV_CS(CS(r), CS(10)),
    INSERT_TMACRO(cubic_rc, rc, t1, t2, t3),
    TNEQ_CS(rc, rc, CS(0)),
    PJMP2_CS(rc, fb_loop, fb_end),
    LABEL(fb_loop),
    LABEL(useless), // to test label optimizations
    TMUL_CS(CS(l), CS(l), CS(10)),
    TMUL_CS(CS(r), CS(r), CS(10)),
    INSERT_TMACRO(cubic_rc, rc, t1, t2, t3),
    TNEQ_CS(rc, rc, CS(0)),
    PJMP2_CS(rc, fb_loop, fb_end),
    LABEL(fb_end)
MACRO_END;

template <std::floating_point T>
TBEGIN5(cubic_eval_program, a, b, c, d, x)
    INSERT_TMACRO(cubic_eval, CS(r), CS(x), CS(t1), CS(t2))
TEND1(r);

template <std::floating_point T>
TBEGIN6(cubic_rc_program, a, b, c, d, l, r)
    INSERT_TMACRO(cubic_rc, CS(rc), CS(t1), CS(t2), CS(t3))
END(IVAR(rc));

template <std::floating_point T>
TBEGIN4(cubic_fb_program, a, b, c, d)
    INSERT_TMACRO(cubic_fb, CS(rc), CS(t1), CS(t2), CS(t3))
TEND2(l, r);
// clang-format on

namespace fn
{
template <std::floating_point T>
constexpr T
val_cubic_eval(T a, T b, T c, T d, T x) noexcept
{
    T x2 = x * x;
    T x3 = x2 * x;
    return a * x3 + b * x2 + c * x + d;
}

template <std::floating_point T>
constexpr bool
val_cubic_rc(T a, T b, T c, T d, T l, T r) noexcept
{
    return std::signbit(val_cubic_eval(a, b, c, d, l)) !=
           std::signbit(val_cubic_eval(a, b, c, d, r));
}

template <std::floating_point T>
std::tuple<T, T>
val_cubic_fb(T a, T b, T c, T d) noexcept
{
    T l = -10;
    T r = 10;

    while(!val_cubic_rc(a, b, c, d, l, r))
    {
        l *= 10;
        r *= 10;
    }

    return {l, r};
}
} // namespace fn

MK_NIEBLOID(val_cubic_eval);
MK_NIEBLOID(val_cubic_rc);
MK_NIEBLOID(val_cubic_fb);

using cubic_eval_f = cubic_eval_program<float>;
using cubic_rc_f   = cubic_rc_program<float>;
using cubic_fb_f   = cubic_fb_program<float>;

using namespace lucid;
using namespace lucid::test;

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;
    auto          g_float = rs.make_gen<float>(-1e6f, 1e6f);
    auto          g_b     = rs.make_gen<bool>(0.5);
    auto          g_a     = rs.make_gen<float>(1.f, 1e6f);
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

    track.check(NamedTest(tm.with_ref(g_cubic, bse_cubic, val_cubic_eval),
                          "BSE cubic eval"));
    track.check(NamedTest(tm.with_ref(g_cubrc, bse_cubic_rc, val_cubic_rc),
                          "BSE cubic root condition"));
    track.check(NamedTest(tm.with_ref(g_c4, bse_cubic_fb, val_cubic_fb),
                          "BSE cubic find bounds"));

    return track.ret_code();
}
