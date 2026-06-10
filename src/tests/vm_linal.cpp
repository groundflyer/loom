// -*- C++ -*-
// vm_linal.cpp
#include <common/linal.hpp>
#include <common/math.hpp>
#include <common/test.hpp>
#include <vm/bse.hpp>
#include <vm/lib/linal.hpp>

using namespace loom;
using namespace test;

// clang-format off

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
TKERNEL6(cross_test, x0, x1, x2, y0, y1, y2)
    INSERT_TMACRO(vm::vec3_cross, CS(z), CS(x), CS(y), CS(t1))
TKERNEL_END3(z0, z1, z2);

template <std::floating_point T>
TKERNEL12(mat_vec_dot_test,
          m00, m01, m02,
          m10, m11, m12,
          m20, m21, m22,
          x0, x1, x2)
    INSERT_TMACRO(vm::mat_vec3, CS(y), CS(m), CS(x))
KERNEL_END(TVAR(y0), TVAR(y1), TVAR(y2));

using l = CS(1);
using o = CS(0);
using id_mat = type_sequence<type_sequence<l, o, o>, type_sequence<o, l, o>, type_sequence<o, o, l>>;

template <std::floating_point T>
TKERNEL3(const_mat_vec, v0, v1, v2)
    INSERT_TMACRO(vm::mat_vec3, CS(y), id_mat, CS(v))
TKERNEL_END3(y0, y1, y2);

template <std::floating_point T>
TKERNEL3(vec3_norm_test, v0, v1, v2)
    INSERT_TMACRO(vm::vec3_norm, CS(v), CS(l))
TKERNEL_END3(v0, v1, v2);

// clang-format on

using Vec3f = Vector<float, 3, std::array>;
using Mat3f = Matrix<float, 3, 3, std::array>;

static constexpr int ulp = 30;

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns<100000>(argc, argv)};
    RandomState   rs;

    vm::bse::function<cross_dot_test<float>>   bse_cross_dot;
    vm::bse::function<cross_test<float>>       bse_cross;
    vm::bse::function<mat_vec_dot_test<float>> bse_mat_vec_dot;
    vm::bse::function<const_mat_vec<float>>    bse_cmat;
    vm::bse::function<vec3_norm_test<float>>   bse_vec3_norm;

    auto       gf   = rs.make_gen<float>(-1e6, 1e6);
    auto       gf3  = [&]() noexcept { return std::tuple{gf(), gf(), gf()}; };
    const auto g2v3 = [&]() noexcept
    { return std::tuple{gf(), gf(), gf(), gf(), gf(), gf()}; };
    const auto vec3_ulp_cmp = [](const auto& t, const auto& r) noexcept
    {
        return almost_equal(std::get<0>(t), r[0], ulp) &&
               almost_equal(std::get<1>(t), r[1], ulp) &&
               almost_equal(std::get<2>(t), r[2], ulp);
    };

    track.check(NamedTest(tm.invariant(g2v3, bse_cross_dot, std::identity{}),
                          "cross dot test"));

    track.check(NamedFuzzyTest(tm.fuzzy_with_ref(
                                   g2v3,
                                   bse_cross,
                                   [](float x0,
                                      float x1,
                                      float x2,
                                      float y0,
                                      float y1,
                                      float y2) noexcept
                                   {
                                       const auto [z0, z1, z2] =
                                           cross(Vec3f{x0, x1, x2},
                                                 Vec3f{y0, y1, y2});
                                       return std::tuple{z0, z1, z2};
                                   },
                                   test::uni_tuple_cmp{ulp_cmp{ulp}}),
                               "cross test",
                               0.01));

    track.check(NamedFuzzyTest(tm.fuzzy_with_ref(
                                   [&]() noexcept
                                   {
                                       std::array<float, 12> ret;
                                       for(std::size_t i = 0; i < 12; ++i)
                                           ret[i] = gf();
                                       return ret;
                                   },
                                   bse_mat_vec_dot,
                                   [](const auto& arr) noexcept
                                   {
                                       const Mat3f m{arr};
                                       const Vec3f v{arr[9], arr[10], arr[11]};
                                       return dot(m, v);
                                   },
                                   vec3_ulp_cmp),
                               "matrix-vector product",
                               0.01));

    track.check(NamedTest(tm.test(gf3, bse_cmat, std::equal_to<>{}),
                          "identity matrix multiplication"));

    track.check(NamedFuzzyTest(tm.fuzzy_with_ref(
                                   gf3,
                                   bse_vec3_norm,
                                   [](float v0, float v1, float v2) noexcept
                                   {
                                       const auto [r0, r1, r2] =
                                           normalize(Vec3f{v0, v1, v2});
                                       return std::tuple{r0, r1, r2};
                                   },
                                   test::uni_tuple_cmp{ulp_cmp{ulp}}),
                               "vec3 normalize test",
                               0.01));

    return track.ret_code();
}
