// -*- C++ -*-
// lang.cpp

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/lang.hpp>
#include <lang/simd.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <range/v3/view/zip.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <random>
#include <ranges>
#include <vector>

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
TBEGIN1(factorial_float, a)
    MOV(IVAR(c), TVAR(a)),
    ISUB(c, c, 1),
    LABEL(loop),
    MOV(TVAR(ca), IVAR(c)),
    TMUL(a, a, ca),
    ISUB(c, c, 1),
    IEQ(cmp, c, 0),
    PJMP2(cmp, loop, END)
TEND1(a)

using namespace lucid;

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

    using fac_unsigned = factorial_int<unsigned>;
    bse::evaluator<fac_unsigned> facu;
    const unsigned               facu_in  = 6;
    const unsigned               facu_chk = 720;
    auto                         facu_out = facu(facu_in);
    ret += facu_out != facu_chk;
    ok("facu bse");

    using facf = factorial_float<float>;
    bse::evaluator<facf> facf_bse;
    const float          facf_bse_in  = 6;
    const float          facf_bse_chk = 720;
    const float          facf_bse_out = facf_bse(facf_bse_in);
    ret += !almost_equal(facf_bse_out, facf_bse_chk, 100);
    ok("facf bse");

    bve::evaluator<fac_unsigned, 4> vfacu;
    const auto                      vfacu_in  = {4u, 5u, 6u, 7u};
    const auto                      vfacu_chk = {24u, 120u, 720u, 5040u};
    auto                            vfacu_out = vfacu(vfacu_in);
    std::ranges::sort(vfacu_out);
    ret += !std::ranges::equal(vfacu_out, vfacu_chk);
    ok("fac bve");

    using incf    = increment<float>;
    using incf_se = simd::evaluator<incf>;
    incf_se simd_incf_eval{};
    auto    incf_se_ret = simd_incf_eval(19999, -1.f);
    ret += !std::ranges::all_of(incf_se_ret,
                                [](const auto i) noexcept { return i == 0.f; });
    ok("simd incf");

    using incd    = increment<double>;
    using incd_se = simd::evaluator<incd>;
    incd_se simd_incd_eval{};
    auto    incd_se_ret = simd_incd_eval(19999, -1.f);
    ret += !std::ranges::all_of(incd_se_ret,
                                [](const auto i) noexcept { return i == 0.; });
    ok("simd incd");

    using facf_simd = simd::evaluator<facf>;
    facf_simd facf_simd_eval{};
    auto      facf_simd_ret = facf_simd_eval(99, 7.f);
    ret += !std::ranges::all_of(facf_simd_ret,
                                [](const float i) constexpr noexcept
                                { return almost_equal(i, 5040.f, 100); });
    ok("simd facf1");

    using facd    = factorial_float<double>;
    using facd_se = simd::evaluator<facd>;
    facd_se facd_se_eval{};
    auto    facd_se_ret = facd_se_eval(99, 7.f);
    ret += !std::ranges::all_of(facd_se_ret,
                                [](const double i) constexpr noexcept
                                { return almost_equal(i, 5040., 100); });
    ok("simd facd1");

    std::random_device                      r;
    std::default_random_engine              e{r()};
    std::uniform_int_distribution<unsigned> dist(10, 100);

    auto generator = [&]() noexcept { return static_cast<float>(dist(e)); };
    std::vector<float> facf_in;
    std::vector<float> facf_check;
    std::size_t        n = 19999;
    std::ranges::generate_n(std::back_inserter(facf_in), n, generator);
    std::ranges::transform(facf_in, std::back_inserter(facf_check), facf_bse);
    std::sort(facf_check.begin(), facf_check.end());
    auto facf_simd_out = facf_simd_eval(n, facf_in);
    std::sort(facf_simd_out.begin(), facf_simd_out.end());
    ret += !std::ranges::all_of(ranges::views::zip(facf_simd_out, facf_check) |
                                    std::views::transform(
                                        [](const auto& ab) noexcept
                                        {
                                            const auto [a, b] = ab;
                                            return almost_equal(a, b, 100);
                                        }),
                                std::identity{});
    ok("simd facf2");

    return ret;
}
