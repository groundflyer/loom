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
BEGIN(simple_union_test, TVAR(i))
    TMOV(j, i),
    MOV(IVAR(j), TVAR(j))
END(IVAR(j))

using namespace lucid;
using namespace lucid::test;

int main(int argc, char* argv[])
{
    // clang-format on
    Track       track(argc, argv);
    RandomState rs;

    auto int_g   = rs.make_gen<int>(2, 10);
    auto u_g     = rs.make_gen<unsigned>(2, 10);
    auto float_g = rs.make_gen<float>(0, 1000);

    using incr_int     = increment<int>;
    using fac_int      = factorial_int<int>;
    using fac_unsigned = factorial_int<unsigned>;
    using sut          = simple_union_test<float>;

    bse::evaluator<incr_int>     bse_ii;
    bse::evaluator<fac_unsigned> bse_fu;
    bse::evaluator<fac_int>      bse_fi;
    bse::evaluator<sut>          bse_sut;

    bve::evaluator<incr_int>     bve_ii;
    bve::evaluator<fac_unsigned> bve_fu;
    bve::evaluator<fac_int>      bve_fi;
    bve::evaluator<sut>          bve_sut;

    simd::evaluator<incr_int>     simd_ii;
    simd::evaluator<fac_unsigned> simd_fu;
    simd::evaluator<fac_int>      simd_fi;
    simd::evaluator<sut>          simd_sut;

    auto val_incr = [](auto i) noexcept { return i + 1; };
    auto val_fac  = [](auto i) noexcept { return fac(i); };
    auto val_sut  = [](float i) noexcept -> unsigned
    { return static_cast<unsigned>(i); };

    track.parm_test_val_cmp("BSE increment int", int_g, bse_ii, val_incr);
    track.parm_test_val_cmp("BSE factorial unsigned int", u_g, bse_fu, val_fac);
    track.parm_test_val_cmp("BSE factorial int", int_g, bse_fi, val_fac);
    track.parm_test_val_cmp("BSE union", float_g, bse_sut, val_sut);

    track.parm_range_test_gen("BVE increment", int_g, bve_ii, bse_ii);
    track.parm_range_test_gen("BVE factorial unsigned", u_g, bve_fu, val_fac);
    track.parm_range_test_gen("BVE union", float_g, bve_sut, bse_sut);

    bve_fu.reset();
    bve_sut.reset();
    track.parm_range_test("SIMD increment int", int_g, simd_ii, bve_ii);
    track.parm_range_test("SIMD factorial unsigned", u_g, simd_fu, bve_fu);
    track.parm_range_test("SIMD union", float_g, simd_sut, bve_sut);

    return track.code();
}
