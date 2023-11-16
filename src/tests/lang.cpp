// -*- C++ -*-
// lang.cpp

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/lang.hpp>
#include <lang/simd.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <array>
#include <initializer_list>

using namespace lucid;

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
template <typename T>
TBEGIN1(factorial, a)
    TSUB(c, a, 1),
    LABEL(loop),
    TMUL(a, a, c),
    TSUB(c, c, 1),
    TEQ(cmp, c, 0),
    PJMP2(cmp, loop, END)
TEND1(a)

int main(/*int argc, char* argv[]*/)
{
    // clang-format on
    int  ret = 0;
    auto ok  = [&](std::string_view name) noexcept
    { fmt::print("{} : {}\n", name, ret == 0 ? "OK" : "FAIL"); };

    using incr_int = increment<int>;
    bse::evaluator<incr_int> p;
    ret += p(-1);
    ok("increment bse");

    bve::evaluator<incr_int, 4> pv;
    ret += !std::ranges::all_of(pv(0),
                                [](const auto i) constexpr noexcept
                                { return i == 1; });
    ok("increment bve single input");

    auto v1 = {0, 1, 2, 3};
    auto v2 = {1, 2, 3, 4};
    ret += !std::ranges::equal(pv(v1), v2);
    ok("increment bve multi input");

    using fac_unsigned = factorial<unsigned>;
    bse::evaluator<fac_unsigned> fac;

    const unsigned fac_in  = 6;
    const unsigned fac_chk = 720;
    auto           fac_out = fac(fac_in);
    ret += fac_out != fac_chk;
    ok("fac bse");

    bve::evaluator<fac_unsigned, 4> vfac;
    const auto                      vfac_in  = {4u, 5u, 6u, 7u};
    const auto                      vfac_chk = {24u, 120u, 720u, 5040u};
    auto                            vfac_out = vfac(vfac_in);
    std::ranges::sort(vfac_out);
    ret += !std::ranges::equal(vfac_out, vfac_chk);
    ok("fac bve");

    using incf    = increment<float>;
    using incf_se = simd::evaluator<incf>;
    incf_se simd_incf_eval{};
    auto    incf_se_ret = simd_incf_eval(19999, -1.f);
    ret += !std::ranges::all_of(incf_se_ret,
                                [](const auto i) noexcept { return i == 0.f; });
    ok("simd incf");

    using facf    = factorial<float>;
    using facf_se = simd::evaluator<facf>;
    facf_se facf_se_eval{};
    auto    facf_se_ret = facf_se_eval(99, 7.f);
    ret += !std::ranges::all_of(facf_se_ret,
                                [](const float i) constexpr noexcept
                                { return fn::almost_equal(i, 5040.f, 100); });
    ok("simd facf");

    return ret;
}
