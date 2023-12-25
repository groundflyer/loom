// -*- C++ -*-
// lang.cpp

#pragma GCC diagnostic   ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Walign-mismatch"

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/lang.hpp>
#include <lang/simd.hpp>
#include <utils/math.hpp>
#include <utils/test.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

// clang-format off
template <typename T>
TBEGIN1(increment, val)
    TADD(val, val, 1)
TEND1(val)

// T factorial(T i):
//     T c = i - 1
//     while c != 0:
//         i *= c
//         c -= 1
//     return i
//
// input: a
// output: a
//
// c = a - 1
// loop:
// a = a * c
// c = c - 1
// cmp = c == 0
// jump: cmp: 0 -> loop, 1 -> end
template <std::integral T>
TBEGIN1(factorial_int, a)
    TSUB(c, a, 1),
    LABEL(loop),
    TMUL(a, a, c),
    TSUB(c, c, 1),
    TEQ(cmp, c, 0),
    PJMP2(cmp, loop, END)
TEND1(a)

template <std::floating_point T>
TBEGIN1(simple_union_test, i)
    TMOV(j, i),
    MOV(IVAR(j), TVAR(j))
END(IVAR(j))

// i < 0.5 ? 200 : 100
template <std::floating_point T>
TBEGIN1(complex_union_test, i)
    TLT(cmp, i, 0.5),
    PJMP2(cmp, l0, l1),

    LABEL(l0),
    MOV(IVAR(i), IVAR(100)),
    JMP(END),

    LABEL(l1),
    MOV(IVAR(i), IVAR(200))
END(IVAR(i))

// case i:
// 0: a + 10
// 1: a - 10
// 2: a * 10
// 3: a / 10
template <typename T>
BEGIN(jmp4_test, IVAR(i), TVAR(a))
    PJMP4(i, l0, l1, l2, l3),

    LABEL(l0),
    TADD(a, a, 100),
    JMP(END),

    LABEL(l1),
    TSUB(a, a, 50),
    JMP(END),

    LABEL(l2),
    TMUL(a, a, 5),
    JMP(END),

    LABEL(l3),
    TDIV(a, a, 2)
TEND1(a)

using namespace lucid;
using namespace lucid::test;

int main(int argc, char* argv[])
{
    // clang-format on
    Track       track(argc, argv);
    RandomState rs;

    auto g_int   = rs.make_gen<int>(2, 10);
    auto g_ui    = rs.make_gen<unsigned>(2, 10);
    auto g_float = rs.make_gen<float>();
    auto g_u3    = rs.make_gen<unsigned>(0, 3);

    using incr_int     = increment<int>;
    using fac_int      = factorial_int<int>;
    using fac_unsigned = factorial_int<unsigned>;
    using sut          = simple_union_test<float>;
    using cut          = complex_union_test<float>;
    using jmp4_t       = jmp4_test<float>;

    bse::evaluator<incr_int>     bse_ii;
    bse::evaluator<fac_unsigned> bse_fu;
    bse::evaluator<fac_int>      bse_fi;
    bse::evaluator<sut>          bse_sut;
    bse::evaluator<cut>          bse_cut;
    bse::evaluator<jmp4_t>       bse_jmp4;

    bve::evaluator<incr_int>     bve_ii;
    bve::evaluator<fac_unsigned> bve_fu;
    bve::evaluator<fac_int>      bve_fi;
    bve::evaluator<sut>          bve_sut;
    bve::evaluator<cut>          bve_cut;
    bve::evaluator<jmp4_t>       bve_jmp4;

    simd::evaluator<incr_int>     simd_ii;
    simd::evaluator<fac_unsigned> simd_fu;
    simd::evaluator<fac_int>      simd_fi;
    simd::evaluator<sut>          simd_sut;
    simd::evaluator<cut>          simd_cut;
    simd::evaluator<jmp4_t>       simd_jmp4;

    auto val_incr = [](auto i) noexcept { return i + 1; };
    auto val_fac  = [](auto i) noexcept { return fac(i); };
    auto val_sut  = [](float i) noexcept -> unsigned
    { return static_cast<unsigned>(i); };
    auto val_cut = [](float i) noexcept -> unsigned
    { return i < 0.5 ? 200 : 100; };
    auto val_jmp4 = [](unsigned i, float a) noexcept
    {
        switch(i)
        {
        case 0: return a + 100;
        case 1: return a - 50;
        case 2: return a * 5;
        case 3: return a / 2;
        }
        return a;
    };
    auto g_j4 = [&]() noexcept { return std::pair{g_u3(), g_float()}; };

    track.parm_test_val("BSE increment int", g_int, bse_ii, val_incr);
    track.parm_test_val("BSE factorial unsigned", g_ui, bse_fu, val_fac);
    track.parm_test_val("BSE factorial int", g_int, bse_fi, val_fac);
    track.parm_test_val("BSE simple union", g_float, bse_sut, val_sut);
    track.parm_test_val("BSE complex union", g_float, bse_cut, val_cut);
    track.parm_test_val("BSE jump4", g_j4, bse_jmp4, val_jmp4);

    track.parm_test_rng_sval("BVE increment", g_int, bve_ii, bse_ii);
    track.parm_test_rng_sval("BVE factorial unsigned", g_ui, bve_fu, bse_fu);
    track.parm_test_rng_sval("BVE complex union", g_float, bve_cut, bse_cut);
    track.parm_test_rng_sval("BVE simple union", g_float, bve_sut, bse_sut);
    track.parm_test_rng_sval("BVE jump4", g_j4, bve_jmp4, bse_jmp4);

    bve_fu.reset();
    bve_sut.reset();
    bve_cut.reset();
    bve_jmp4.reset();
    track.parm_test_rng("SIMD increment int", g_int, simd_ii, bve_ii);
    track.parm_test_rng("SIMD factorial unsigned", g_ui, simd_fu, bve_fu);
    track.parm_test_rng("SIMD simple union", g_float, simd_sut, bve_sut);
    track.parm_test_rng("SIMD complex union", g_float, simd_cut, bve_cut);
    track.parm_test_rng("SIMD jump4", g_j4, simd_jmp4, bve_jmp4);

    return track.code();
}
