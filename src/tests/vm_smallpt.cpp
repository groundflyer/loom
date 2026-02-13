// -*- C++ -*-
// vm_smallpt.cpp
#include <common/linal.hpp>
#include <common/test.hpp>
#include <smallpt/ref/rng.hpp>
#include <smallpt/vm/linal.hpp>
#include <smallpt/vm/rng.hpp>
#include <vm/bse.hpp>

using namespace loom;
using namespace test;

constexpr std::uint32_t k0 = 0xdead;
constexpr std::uint32_t k1 = 0xbeef;
// constexpr std::uint32_t k2 = 0x1bd11bda ^ k0 ^ k1; // 0x1bd17b98

// clang-format off
KERNEL(rotl32_test, VAR(std::uint32_t, x), VAR(std::uint32_t, n))
    INSERT_MACRO(vm::rotl32, CS(x), CS(n), CS(dst), CS(tmp))
KERNEL_END(VAR(std::uint32_t, dst));

KERNEL(threefry2x32_test, VAR(std::uint32_t, c0), VAR(std::uint32_t, c1))
    INSERT_MACRO(vm::threefry2x32r32, CS(c0), CS(c1), CS(0xdead), CS(0xbeef), CS(0x1bd17b98), CS(tmp1), CS(tmp2))
KERNEL_END(VAR(std::uint32_t, c0), VAR(std::uint32_t, c1));

KERNEL(gen_canon_test, VAR(std::uint32_t, x))
    INSERT_MACRO(vm::gen_canonical, CS(x))
KERNEL_END(VAR(float, x));

KERNEL(bump_test, VAR(std::uint32_t, c0), VAR(std::uint32_t, c1), VAR(std::uint32_t, i))
    INSERT_MACRO(vm::bump_ctr, CS(c0), CS(c1), CS(i), CS(tmp1), CS(tmp2))
KERNEL_END(VAR(std::uint32_t, c0), VAR(std::uint32_t, c1));

// cross product of two vectors is vector perpendicular to both of them
template <std::floating_point T>
TKERNEL6(cross_dot_test, x0, x1, x2, y0, y1, y2)
    INSERT_TMACRO(vm::vec3_cross, CS(z), CS(x), CS(y), CS(t1)),
    INSERT_TMACRO(vm::vec3_dot, CS(t1), CS(x), CS(z)),
    INSERT_TMACRO(vm::vec3_dot, CS(t2), CS(y), CS(z)),
    TABS(t1, t1),
    TABS(t2, t2),
    TLE(t1, t1, 1e-6),
    TLE(t2, t2, 1e-6),
    ILAND(t1, t1, t2)
KERNEL_END(IVAR(t1));

template <std::floating_point T>
KERNEL(mat_vec_dot_test,
       TVAR(m00), TVAR(m01), TVAR(m02),
       TVAR(m10), TVAR(m11), TVAR(m12),
       TVAR(m20), TVAR(m21), TVAR(m22),
       TVAR(x0), TVAR(x1), TVAR(x2))
    INSERT_TMACRO(vm::mat_vec3_dot, CS(y), CS(m), CS(x))
KERNEL_END(TVAR(y0), TVAR(y1), TVAR(y2));

// clang-format on

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns<100000>(argc, argv)};
    RandomState   rs;

    auto       gu32          = rs.make_gen<std::uint32_t>(0, -1u);
    auto       gf            = rs.make_gen<float>(-1e6, 1e6);
    const auto cmp_tuple_arr = [](const auto& a, const auto& b) noexcept
    { return std::get<0>(a) == b[0] && std::get<1>(a) == b[1]; };

    vm::bse::function<rotl32_test>             bse_rotl32;
    vm::bse::function<threefry2x32_test>       bse_threefry2x32;
    vm::bse::function<gen_canon_test>          bse_gen_canon;
    vm::bse::function<bump_test>               bse_bump;
    vm::bse::function<cross_dot_test<float>>   bse_cross_dot;
    vm::bse::function<mat_vec_dot_test<float>> bse_mat_vec_dot;

    track.dep_check(
        NamedTest(
            tm.with_ref(
                [&, gn = rs.make_gen<std::uint32_t>(0, 31)]() mutable noexcept
                { return std::tuple{gu32(), gn()}; },
                bse_rotl32,
                rotl32),
            "rotl32"),
        NamedTest(tm.with_ref([&]() noexcept -> std::array<std::uint32_t, 2>
                              { return {gu32(), gu32()}; },
                              bse_threefry2x32,
                              threefry2x32r32<k0, k1>,
                              cmp_tuple_arr),
                  "Threefry-2x32"));
    track.check(NamedTest(tm.with_ref(gu32, bse_gen_canon, gen_canonical),
                          "gen_canonical"));
    track.check(NamedTest(
        tm.with_ref(
            [&]() noexcept -> std::array<std::uint32_t, 3>
            { return {gu32(), gu32(), gu32()}; },
            bse_bump,
            [](const std::uint32_t c0,
               const std::uint32_t c1,
               const std::uint32_t i) noexcept
            { return bump_ctr(std::array<std::uint32_t, 2>{c0, c1}, i); },
            cmp_tuple_arr),
        "bump counter"));

    track.check(NamedTest(
        tm.invariant([&]() noexcept
                     { return std::tuple{gf(), gf(), gf(), gf(), gf(), gf()}; },
                     bse_cross_dot,
                     std::identity{}),
        "cross dot test"));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(
            [&]() noexcept
            {
                std::array<float, 12> ret;
                for(std::size_t i = 0; i < 12; ++i) ret[i] = gf();
                return ret;
            },
            bse_mat_vec_dot,
            [](const auto& arr) noexcept
            {
                Matrix<float, 3, 3, std::array> m{arr};
                Vector<float, 3, std::array>    v{arr[9], arr[10], arr[11]};
                return dot(m, v);
            },
            [](const auto& t, const auto& r) noexcept
            {
                return almost_equal(std::get<0>(t), r[0], 10) &&
                       almost_equal(std::get<1>(t), r[1], 10) &&
                       almost_equal(std::get<2>(t), r[2], 10);
            }),
        "matrix-vector product",
        0.01));

    return track.ret_code();
}
