// -*- C++ -*-
// lang.cpp

#pragma GCC diagnostic   ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Walign-mismatch"

#include "programs.hpp"

#ifdef USE_CUDA
#include "cuda.hpp"
#include <bit>
#endif

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/lang.hpp>
#include <lang/simd.hpp>
#include <utils/test.hpp>

using namespace lucid;
using namespace lucid::test;

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;

    auto g_int   = rs.make_gen<int>(2, 10);
    auto g_ui    = rs.make_gen<unsigned>(2, 10);
    auto g_float = rs.make_gen<float>();
    auto g_u3    = rs.make_gen<unsigned>(0, 3);
    auto g_j4    = [&]() noexcept { return std::pair{g_u3(), g_float()}; };
    auto g_shift = [&]() noexcept { return std::pair(g_ui(), g_ui()); };

    bse::evaluator<incr_int>     bse_ii;
    bse::evaluator<fac_unsigned> bse_fu;
    bse::evaluator<fac_int>      bse_fi;
    bse::evaluator<sut>          bse_sut;
    bse::evaluator<cut>          bse_cut;
    bse::evaluator<jmp4_float>   bse_jmp4;
    bse::evaluator<choice_float> bse_choice;
    bse::evaluator<lshift_u>     bse_lshift;
    bse::evaluator<rshift_u>     bse_rshift;

    bve::evaluator<incr_int>     bve_ii;
    bve::evaluator<fac_unsigned> bve_fu;
    bve::evaluator<fac_int>      bve_fi;
    bve::evaluator<sut>          bve_sut;
    bve::evaluator<cut>          bve_cut;
    bve::evaluator<jmp4_float>   bve_jmp4;
    bve::evaluator<choice_float> bve_choice;
    bve::evaluator<lshift_u>     bve_lshift;
    bve::evaluator<rshift_u>     bve_rshift;

    simd::evaluator<incr_int>     simd_ii;
    simd::evaluator<fac_unsigned> simd_fu;
    simd::evaluator<fac_int>      simd_fi;
    simd::evaluator<sut>          simd_sut;
    simd::evaluator<cut>          simd_cut;
    simd::evaluator<jmp4_float>   simd_jmp4;
    simd::evaluator<choice_float> simd_choice;
    simd::evaluator<lshift_u>     simd_lshift;
    simd::evaluator<rshift_u>     simd_rshift;

#define TEST_RESET_T(G, T, V)            \
    [&]() noexcept                       \
    {                                    \
        bool r = tm.rng_sval(G, T, V)(); \
        T.reset();                       \
        return r;                        \
    }

#ifdef USE_CUDA
#define WRAP(NAME) \
    [](unsigned n, const auto&... in) noexcept { return NAME(n, in...); }

#define WRAP_UNION(NAME)                                             \
    [](unsigned n, const auto& in) noexcept -> std::vector<unsigned> \
    { return NAME<unsigned, float>(n, in); }

#define TEST_RESET_V(G, T, V)           \
    [&]() noexcept                      \
    {                                   \
        bool r = tm.rng_cmp(G, T, V)(); \
        V.reset();                      \
        return r;                       \
    }

    auto cuda_ii = WRAP(cuda_inc);
    auto cuda_fu = WRAP(cuda_fac);
    auto cuda_su = WRAP_UNION(cuda_sut);
    auto cuda_cu = WRAP_UNION(cuda_cut);
    auto cuda_j4 = WRAP(cuda_jmp4);
    auto cuda_ch = WRAP(cuda_choice);
    auto cuda_ls = WRAP(cuda_lshift);
    auto cuda_rs = WRAP(cuda_rshift);

    // CUDA backend supports only 2^n size
    tm.m_num = std::bit_floor(tm.m_num);

    auto ft_ii = std::pair{
        NamedTest(tm.rng_cmp(g_int, simd_ii, bve_ii), "SIMD increment int"),
        NamedTest(tm.rng_cmp(g_int, cuda_ii, bve_ii), "CUDA increment int")};
    auto ft_fu = std::pair{NamedTest(TEST_RESET_V(g_ui, simd_fu, bve_fu),
                                     "SIMD factorial unsigned"),
                           NamedTest(tm.rng_cmp(g_ui, cuda_fu, bve_fu),
                                     "CUDA factorial unsigned")};
    auto ft_su = std::pair{
        NamedTest(tm.rng_cmp(g_float, simd_sut, bve_sut), "SIMD simple union"),
        NamedTest(tm.rng_cmp(g_float, cuda_su, bve_sut), "CUDA simple union")};
    auto ft_cu = std::pair{
        NamedTest(TEST_RESET_V(g_float, simd_cut, bve_cut),
                  "SIMD complex union"),
        NamedTest(tm.rng_cmp(g_float, cuda_cu, bve_cut), "CUDA complex union")};
    auto ft_j4 = std::pair{
        NamedTest(TEST_RESET_V(g_j4, simd_jmp4, bve_jmp4), "SIMD jump4"),
        NamedTest(tm.rng_cmp(g_j4, cuda_j4, bve_jmp4), "CUDA jump4")};
    auto ft_ch = std::pair{
        NamedTest(tm.rng_cmp(g_j4, simd_choice, bve_choice), "SIMD choice"),
        NamedTest(tm.rng_cmp(g_j4, cuda_ch, bve_choice), "CUDA choice")};
    auto ft_ls = std::pair{
        NamedTest(tm.rng_cmp(g_shift, simd_lshift, bve_lshift),
                  "SIMD left shift"),
        NamedTest(tm.rng_cmp(g_shift, cuda_ls, bve_lshift), "CUDA left shift")};
    auto ft_rs =
        std::pair{NamedTest(tm.rng_cmp(g_shift, simd_rshift, bve_rshift),
                            "SIMD right shift"),
                  NamedTest(tm.rng_cmp(g_shift, cuda_rs, bve_rshift),
                            "CUDA right shift")};
#else
    auto ft_ii =
        NamedTest(tm.rng_cmp(g_int, simd_ii, bve_ii), "SIMD increment int");
    auto ft_fu =
        NamedTest(tm.rng_cmp(g_ui, simd_fu, bve_fu), "SIMD factorial unsigned");
    auto ft_su =
        NamedTest(tm.rng_cmp(g_float, simd_sut, bve_sut), "SIMD simple union");
    auto ft_cu =
        NamedTest(tm.rng_cmp(g_float, simd_cut, bve_cut), "SIMD complex union");
    auto ft_j4 = NamedTest(tm.rng_cmp(g_j4, simd_jmp4, bve_jmp4), "SIMD jump4");
    auto ft_ch =
        NamedTest(tm.rng_cmp(g_j4, simd_choice, bve_choice), "SIMD choice");
    auto ft_ls = NamedTest(tm.rng_cmp(g_shift, simd_lshift, bve_lshift),
                           "SIMD left shift");
    auto ft_rs = NamedTest(tm.rng_cmp(g_shift, simd_rshift, bve_rshift),
                           "SIMD right shift");
#endif

    track.dep_check(
        NamedTest(tm.test_cmp(g_int, bse_ii, val_inc), "BSE increment int"),
        NamedTest(tm.rng_sval(g_int, bve_ii, bse_ii), "BVE increment int"),
        ft_ii);

    track.dep_check(NamedTest(tm.test_cmp(g_shift, bse_lshift, val_lshift),
                              "BSE left shift"),
                    NamedTest(tm.rng_sval(g_shift, bve_lshift, bse_lshift),
                              "BVE left shift"),
                    ft_ls);

    track.dep_check(NamedTest(tm.test_cmp(g_shift, bse_rshift, val_rshift),
                              "BSE right shift"),
                    NamedTest(tm.rng_sval(g_shift, bve_rshift, bse_rshift),
                              "BVE right shift"),
                    ft_rs);

    track.dep_check(
        std::pair{
            NamedTest(tm.test_cmp(g_int, bse_fi, val_fac), "BSE factorial int"),
            NamedTest(tm.test_cmp(g_ui, bse_fu, val_fac),
                      "BSE factorial unsigned")},
        NamedTest(TEST_RESET_T(g_ui, bve_fu, bse_fu), "BVE factorial unsigned"),
        ft_fu);

    track.dep_check(
        NamedTest(tm.test_cmp(g_float, bse_sut, val_sut), "BSE simple union"),
        NamedTest(tm.rng_sval(g_float, bve_sut, bse_sut), "BVE simple union"),
        ft_su);

    track.dep_check(
        NamedTest(tm.test_cmp(g_float, bse_cut, val_cut), "BSE complex union"),
        NamedTest(TEST_RESET_T(g_float, bve_cut, bse_cut), "BVE complex union"),
        ft_cu);

    track.dep_check(
        NamedTest(tm.test_cmp(g_j4, bse_jmp4, val_jmp4), "BSE jump4"),
        NamedTest(TEST_RESET_T(g_j4, bve_jmp4, bse_jmp4), "BVE jump4"),
        ft_j4);

    track.dep_check(
        NamedTest(tm.test_cmp(g_j4, bse_choice, val_choice), "BSE choice"),
        NamedTest(tm.rng_sval(g_j4, bve_choice, bse_choice), "BVE choice"),
        ft_ch);

    return track.ret_code();
}
