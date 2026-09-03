// -*- C++ -*-
// vm_smallpt.cpp

#include "util.hpp"

#include <common/linal.hpp>
#include <common/math.hpp>
#include <smallpt/ref/pt.hpp>
#include <smallpt/vm/pt.hpp>
#include <vm/bse.hpp>

#include <fmt/ranges.h>

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

template <std::floating_point T>
KERNEL(gen_canon_test, VAR(std::uint32_t, x))
    INSERT_TMACRO(vm::gen_canonical, CS(x))
TKERNEL_END1(x);

KERNEL(bump_test, VAR(std::uint32_t, c0), VAR(std::uint32_t, c1), VAR(std::uint32_t, i))
    INSERT_MACRO(vm::bump_ctr, CS(c0), CS(c1), CS(i), CS(tmp1), CS(tmp2), CS(tmp3))
KERNEL_END(VAR(std::uint32_t, c0), VAR(std::uint32_t, c1));

template <std::floating_point T>
TKERNEL3(onb_test, n0, n1, n2)
    INSERT_TMACRO(vm::onb, CS(b0), CS(b1), CS(b2), CS(t0), CS(t1), CS(t2), CS(n0), CS(n1), CS(n2)),
    INSERT_TMACRO(vm::vec3_norm, CS(b), CS(tmp)),
    INSERT_TMACRO(vm::vec3_norm, CS(t), CS(tmp))
TKERNEL_END6(b0, b1, b2, t0, t1, t2);

template <std::floating_point T>
TKERNEL5(sample_hemi_test, n0, n1, n2, u0, u1)
    INSERT_TMACRO(vm::onb, CS(b0), CS(b1), CS(b2), CS(t0), CS(t1), CS(t2), CS(n0), CS(n1), CS(n2)),
    INSERT_TMACRO(vm::vec3_norm, CS(b), CS(tmp0)),
    INSERT_TMACRO(vm::vec3_norm, CS(t), CS(tmp0)),
    INSERT_TMACRO(vm::sample_hemisphere, CS(d), CS(n), CS(b), CS(t), CS(u0), CS(u1), CS(tmp0), CS(tmp1)),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp0))
TKERNEL_END3(d0, d1, d2);

template <std::floating_point T>
KERNEL(sample_space_test, IVAR(x), IVAR(y), IVAR(w), IVAR(h))
    MOV(TVAR(w), IVAR(w)),
    MOV(TVAR(h), IVAR(h)),
    MOV(TVAR(x), IVAR(x)),
    MOV(TVAR(y), IVAR(y)),
    TDIV(r, w, h),
    INSERT_TMACRO(vm::to_sample_space, CS(x), CS(y), CS(w), CS(h), CS(r))
TKERNEL_END2(x, y);

template <std::floating_point T>
TKERNEL13(disk_isect_test,
          o0, o1, o2,
          d0, d1, d2,
          p0, p1, p2,
          n0, n1, n2,
          r)
    INSERT_TMACRO(vm::isect_disk, CS(hit), CS(t), CS(o), CS(d), CS(p), CS(n), CS(r), CS(vtmp), CS(tmp))
KERNEL_END(IVAR(hit), TVAR(t));

template <std::floating_point T>
TKERNEL3(quadratic_test, a, b, c)
    INSERT_TMACRO(vm::solve_quadratic, CS(s), CS(x), CS(a), CS(b), CS(c), CS(t), CS(t2))
KERNEL_END(IVAR(s), TVAR(x));

template <std::floating_point T>
TKERNEL10(sphere_isect_test,
          o0, o1, o2,
          d0, d1, d2,
          p0, p1, p2,
          r)
    INSERT_TMACRO(vm::prep_isect_sphere, CS(a), CS(b), CS(c), CS(o), CS(d), CS(p), CS(r), CS(tmp0), CS(tmp1), CS(vtmp1)),
    INSERT_TMACRO(vm::solve_quadratic, CS(hit), CS(t), CS(a), CS(b), CS(c), CS(tmp0), CS(tmp1))
KERNEL_END(IVAR(hit), TVAR(t));

template <std::floating_point T>
TKERNEL11(cylinder_isect_test, o0, o1, o2, d0, d1, d2, p0, p1, p2, r, h)
    INSERT_TMACRO(vm::vec_vec3_sub, CS(o), CS(o), CS(p)),
    INSERT_TMACRO(vm::cyl_prep_abc, CS(a), CS(b), CS(c), CS(o0), CS(o1), CS(o2), CS(d0), CS(d1), CS(d2), CS(r)),
    INSERT_TMACRO(vm::solve_quadratic, CS(hit), CS(t), CS(a), CS(b), CS(c), CS(tmp0), CS(tmp1)),
    INSERT_TMACRO(vm::cyl_fin, CS(hit), CS(t), CS(o1), CS(d1), CS(tmp0), CS(h))
KERNEL_END(IVAR(hit), TVAR(t));

template <std::floating_point T>
TKERNEL15(triangle_isect_test,
          o0, o1, o2,
          d0, d1, d2,
          vert00, vert01, vert02,
          vert10, vert11, vert12,
          vert20, vert21, vert22)
    INSERT_TMACRO(vm::isect_triangle, CS(hit), CS(t), CS(o), CS(d), CS(vert0), CS(vert1), CS(vert2), CS(tmp1), CS(tmp2), CS(vtmp))
KERNEL_END(IVAR(hit), TVAR(t));

template <std::floating_point T>
TKERNEL18(quad_isect_test,
          o0, o1, o2,
          d0, d1, d2,
          v000, v001, v002,
          v010, v011, v012,
          v110, v111, v112,
          v100, v101, v102)
    INSERT_TMACRO(vm::isect_quad, CS(hit), CS(t), CS(o), CS(d),
                  CS(v00), CS(v01), CS(v11), CS(v10),
                  CS(vt1), CS(vt2), CS(vt3), CS(tmp1), CS(tmp2), CS(tmp3))
KERNEL_END(IVAR(hit), TVAR(t));

template <std::floating_point T>
TKERNEL2(ray_dir_test, d0, d1)
    INSERT_TMACRO(vm::gen_ray_dir, CS(d0), CS(d1)),
    TMOV_CS(CS(d2), vm::scene::CAM_Dz),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp))
TKERNEL_END3(d0, d1, d2);

template <std::floating_point T>
TKERNEL2(sample_light_test, u0, u1)
    INSERT_TMACRO(vm::sample_light_disk, CS(dst), CS(u0), CS(u1), CS(tmp0), CS(vtmp0))
TKERNEL_END3(dst0, dst1, dst2);

template <std::floating_point T>
TKERNEL2(schlick_test, cd, eta)
    INSERT_TMACRO(vm::schlick, CS(r), CS(cd), CS(eta), CS(tmp0), CS(tmp1))
TKERNEL_END1(r);

template <std::floating_point T>
TKERNEL6(reflect_test, n0, n1, n2, i0, i1, i2)
    INSERT_TMACRO(vm::vec3_negate, CS(wi), CS(i)),
    INSERT_TMACRO(vm::vec3_dot, CS(ct), CS(n), CS(wi)),
    INSERT_TMACRO(vm::reflect, CS(r), CS(n), CS(i), CS(ct))
TKERNEL_END3(r0, r1, r2);

template <std::floating_point T>
TKERNEL6(refract_test, n0, n1, n2, d0, d1, d2)
    INSERT_TMACRO(vm::comp_f, CS(f), CS(wi), CS(ct), CS(into), CS(eta), CS(1.5), CS(tmp0), CS(tmp1)),
    INSERT_TMACRO(vm::vec3_negate, CS(ng), CS(n)),
    TCMOV(n0, into, n0, ng0),
    TCMOV(n1, into, n1, ng1),
    TCMOV(n2, into, n2, ng2),
    INSERT_TMACRO(vm::vec3_dot, CS(ddn), CS(d), CS(n)),
    TMUL(tmp0, ddn, ddn),
    TSUB(tmp0, 1, tmp0),
    TMUL(tmp0, eta, tmp0),
    TMUL(tmp0, eta, tmp0),
    TSUB(cos2t, 1, tmp0),
    TABS(cos2t, cos2t),
    INSERT_TMACRO(vm::refract, CS(d), CS(eta), CS(ddn), CS(cos2t), CS(tmp0), CS(vtmp))
TKERNEL_END3(d0, d1, d2);

template <std::floating_point T>
TKERNEL2(gtr_test, dotWmWg, alpha)
    INSERT_TMACRO(vm::gtr, CS(r), CS(dotWmWg), CS(alpha))
TKERNEL_END1(r);

template <std::floating_point T>
TKERNEL2(gaf_test, ct, alpha)
    INSERT_TMACRO(vm::gaf, CS(r), CS(ct), CS(alpha))
TKERNEL_END1(r);

template <std::floating_point T>
TKERNEL3(get_slope_test, ct, u0, u1)
    INSERT_TMACRO(vm::get_slope, CS(r0), CS(r1), CS(ct), CS(u0), CS(u1), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3), CS(tmp4), CS(tmp5), CS(tmp6))
TKERNEL_END2(r0, r1);

template <std::floating_point T>
TKERNEL7(microfacet_test, wi0, wi1, wi2, alpha0, alpha1, u0, u1)
    INSERT_TMACRO(vm::microfacet,
                  CS(r), CS(r0), CS(r1), CS(r2),
                  CS(wi), CS(wi0), CS(wi1), CS(wi2),
                  CS(alpha0), CS(alpha1), CS(u0), CS(u1), CS(s0), CS(s1),
                  CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3), CS(tmp4), CS(tmp5), CS(tmp6))
TKERNEL_END3(r0, r1, r2);

template <std::floating_point T>
TKERNEL11(anisorough_i_test, wi0, wi1, wi2, tu0, tu1, tu2, tv0, tv1, tv2, alpha0, alpha1)
    INSERT_TMACRO(vm::anisorough_i, CS(r), CS(wi), CS(tu), CS(tv), CS(alpha0), CS(alpha1), CS(tmp))
TKERNEL_END1(r);

template <std::floating_point T>
TKERNEL14(anisorough_n_test, wm0, wm1, wm2, n0, n1, n2, tu0, tu1, tu2, tv0, tv1, tv2, alpha0, alpha1)
    INSERT_TMACRO(vm::vec3_dot, CS(dotWmWg), CS(wm), CS(n)),
    INSERT_TMACRO(vm::anisorough_n, CS(r), CS(wm), CS(n), CS(tu), CS(tv), CS(dotWmWg), CS(alpha0), CS(alpha1), CS(tmp), CS(vtmp))
TKERNEL_END1(r);

// clang-format on

using Vec3f = Vector<float, 3, std::array>;
using Vec2f = Vector<float, 2, std::array>;
using Vec2u = Vector<unsigned, 2, std::array>;

static constexpr int     ulp = 100;
constexpr inline ulp_cmp fcmp{ulp};

struct fcmp_tuple
{
    template <typename T>
    bool
    operator()(const T& t1, const T& t2) const noexcept
    {
        return fold_tuple(map_tuple(zip_tuples(t1, t2),
                                    [](const auto& p) noexcept -> bool
                                    {
                                        const auto [a, b] = p;
                                        return fcmp(a, b);
                                    }),
                          std::logical_and<bool>{});
    }
};

constexpr inline fcmp_tuple fcmpt{};

template <typename T, std::size_t N>
static auto
to_tuple(const std::array<T, N>& a) noexcept
{
    return array2tuple(a);
}

template <typename T, std::size_t N>
static auto
to_tuple(const Vector<T, N, std::array>& v) noexcept
{
    return to_tuple(v.data());
}

template <typename T>
static auto
to_tuple(const Disk<T>& prim) noexcept
{
    const auto& [p, n, r] = prim;
    return array2tuple(array_cat(p.data(), n.data(), std::array<T, 1>{r}));
}

template <typename T>
static auto
to_tuple(const Sphere<T>& prim) noexcept
{
    return std::tuple_cat(to_tuple(prim.center), std::tuple{prim.radius});
}

template <typename T>
static auto
to_tuple(const Cylinder<T>& prim) noexcept
{
    const auto [p, r, h] = prim;
    return std::tuple_cat(to_tuple(p), std::tuple{r, h});
}

template <typename T, std::size_t N, std::size_t M>
static auto
to_tuple(const std::array<Vector<T, M, std::array>, N>& prim) noexcept
{
    return std::apply([](const auto&... vs) noexcept
                      { return std::tuple_cat(array2tuple(vs.data())...); },
                      prim);
}

template <typename T>
static auto
to_tuple(const Ray<T>& ray) noexcept
{
    const auto& [o, d] = ray;
    return array2tuple(array_cat(o.data(), d.data()));
}

int
main(int argc, char* argv[])
{
    auto [track, tm, rs] = test_init<100000>(argc, argv);

    fmt::print("Floating-point equality ULP: {}\n", ulp);

    auto gu32    = rs.make_gen<std::uint32_t>(0, -1u);
    auto gur     = rs.make_gen<unsigned>(30, 1000);
    auto gf      = rs.make_gen<float>(-1e6, 1e6);
    auto gfp     = rs.make_gen<float>(0.1, 1e6);
    auto gf11    = rs.make_gen<float>(-1, 1);
    auto g_canon = rs.make_gen<float>();
    auto gb      = rs.make_gen<bool>(0.5);
    auto gf1_10  = rs.make_gen<float>(1, 10);
    auto gv2s    = [&]() noexcept { return Vec2f(generate<2>(g_canon)); };
    auto gv3     = [&]() noexcept { return Vec3f(generate<3>(gf)); };
    auto geta    = [&]() noexcept { return gf1_10() / 1.f; };

    const auto cmp_tuple_arr = [](const auto& a, const auto& b) noexcept
    { return std::get<0>(a) == b[0] && std::get<1>(a) == b[1]; };
    const auto cmp_isects = [](const auto& a, const auto& b) noexcept
    {
        const bool ai = a.intersect;
        const bool bi = b.intersect;
        if(ai && bi)
            return fcmp(a.t, b.t);
        else
            return ai == bi;
    };

    vm::bse::function<rotl32_test>                bse_rotl32;
    vm::bse::function<threefry2x32_test>          bse_threefry2x32;
    vm::bse::function<gen_canon_test<float>>      bse_gen_canon;
    vm::bse::function<bump_test>                  bse_bump;
    vm::bse::function<sample_hemi_test<float>>    bse_sample_hemi;
    vm::bse::function<sample_space_test<float>>   bse_sample_space;
    vm::bse::function<disk_isect_test<float>>     bse_disk_isect;
    vm::bse::function<quadratic_test<float>>      bse_quadratic;
    vm::bse::function<sphere_isect_test<float>>   bse_sphere_isect;
    vm::bse::function<cylinder_isect_test<float>> bse_cylinder_isect;
    vm::bse::function<triangle_isect_test<float>> bse_triangle_isect;
    vm::bse::function<quad_isect_test<float>>     bse_quad_isect;
    vm::bse::function<ray_dir_test<float>>        bse_ray_dir_test;
    vm::bse::function<onb_test<float>>            bse_onb;
    vm::bse::function<sample_light_test<float>>   bse_sample_light;
    vm::bse::function<schlick_test<float>>        bse_schlick;
    vm::bse::function<reflect_test<float>>        bse_reflect;
    vm::bse::function<refract_test<float>>        bse_refract;
    vm::bse::function<gtr_test<float>>            bse_gtr;
    vm::bse::function<gaf_test<float>>            bse_gaf;
    vm::bse::function<get_slope_test<float>>      bse_get_slope;
    vm::bse::function<microfacet_test<float>>     bse_microfacet;
    vm::bse::function<anisorough_i_test<float>>   bse_anisorough_i;
    vm::bse::function<anisorough_n_test<float>>   bse_anisorough_n;

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

    track.dep_check(
        NamedFuzzyTest(tm.fuzzy_with_ref(
                           [&]() noexcept { return normalize(gv3()); },
                           [&bse_onb](const Vec3f& n) noexcept
                           { return bse_onb(n[0], n[1], n[2]); },
                           [](const Vec3f& n) noexcept
                           {
                               const auto [b, t] = basis(n);
                               return std::tuple_cat(to_tuple(b), to_tuple(t));
                           },
                           fcmpt),
                       "orthonormal basis",
                       0.01),
        NamedFuzzyTest(
            tm.fuzzy_with_ref(
                [&]() noexcept
                {
                    return std::pair{normalize(gv3()),
                                     Vec2f{generate<2>(g_canon)}};
                },
                [&bse_sample_hemi](const Vec3f& n, const Vec2f& u) noexcept
                { return bse_sample_hemi(n[0], n[1], n[2], u[0], u[1]); },
                [](const Vec3f& n, const Vec2f& u) noexcept
                { return to_tuple(sample_hemisphere(n, u)); },
                fcmpt),
            "sample hemisphere",
            0.01));

    track.check(
        NamedFuzzyTest(tm.fuzzy_with_ref(
                           gv2s,
                           [&bse_sample_light](const Vec2f& u) noexcept
                           { return bse_sample_light(u[0], u[1]); },
                           [](const Vec2f& u) noexcept
                           { return to_tuple(sample(u, Scene::light)); },
                           fcmpt),
                       "sample light",
                       0.01));

    track.check(NamedFuzzyTest(tm.fuzzy_with_ref(
                                   [&]() noexcept
                                   {
                                       const unsigned w = gur();
                                       const unsigned h = gur();
                                       const unsigned x = g_canon() * w;
                                       const unsigned y = g_canon() * h;
                                       return std::tuple{x, y, w, h};
                                   },
                                   bse_sample_space,
                                   [](const unsigned x,
                                      const unsigned y,
                                      const unsigned w,
                                      const unsigned h) noexcept
                                   {
                                       ToSampleSpace<float> f{
                                           Vec2u{w, h}
                                       };
                                       return to_tuple(f(Vec2u{x, y}));
                                   },
                                   fcmpt),
                               "sample film space",
                               0.01));

    auto mk_isect_g = [&](auto&& pg) noexcept
    {
        return [&, pg = std::move(pg)]() noexcept
        {
            const auto prim = pg();
            const Vec3 p    = gv3();
            const Ray  ray{p,
                          gb() ? normalize(sample(gv2s(), prim) - p) :
                                  normalize(gv3())};
            return std::pair{ray, prim};
        };
    };
    auto mk_isect_t = [&](auto& bse) noexcept
    {
        return [&](const auto& ray,
                   const auto& prim) noexcept -> Intersection<float>
        {
            const auto [hit, t] =
                std::apply(bse, std::tuple_cat(to_tuple(ray), to_tuple(prim)));
            return {static_cast<bool>(hit), t, Vec2f{}};
        };
    };

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(
            mk_isect_g([&]() noexcept
                       { return Disk{gv3(), normalize(gv3()), gf()}; }),
            mk_isect_t(bse_disk_isect),
            intersect,
            cmp_isects),
        "ray-disk intersection",
        0.01));

    track.dep_check(
        NamedFuzzyTest(
            tm.fuzzy_with_ref(
                [&]() noexcept { return std::tuple{gf(), gf(), gf()}; },
                bse_quadratic,
                [](const auto... ks) noexcept { return quadratic(ks...); },
                [&](const auto& t, const auto& v) noexcept
                {
                    const auto [utr, tx] = t;
                    const bool tr        = static_cast<bool>(utr);
                    const auto [vr, vx]  = v;

                    if(tr && vr)
                        return fcmp(tx, vx);
                    else
                        return tr == vr;
                }),
            "quadratic",
            0.01),
        std::pair{
            NamedFuzzyTest(
                tm.fuzzy_with_ref(
                    mk_isect_g([&]() noexcept { return Sphere{gv3(), gfp()}; }),
                    mk_isect_t(bse_sphere_isect),
                    intersect,
                    cmp_isects),
                "ray-sphere intersection",
                0.01),
            NamedFuzzyTest(
                tm.fuzzy_with_ref(
                    mk_isect_g([&]() noexcept
                               { return Cylinder{gv3(), gfp(), gfp()}; }),
                    mk_isect_t(bse_cylinder_isect),
                    intersect,
                    cmp_isects),
                "ray-cylinder intersection",
                0.01)});

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(mk_isect_g([&]() noexcept -> Triangle<float>
                                     { return {gv3(), gv3(), gv3()}; }),
                          mk_isect_t(bse_triangle_isect),
                          intersect,
                          cmp_isects),
        "ray-triangle intersection",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(mk_isect_g(
                              [&]() noexcept -> Quad<float>
                              {
                                  const Vec3f v00 = gv3();
                                  const Vec3f v01 = gv3();
                                  const Vec3f v11 = gv3();
                                  const Vec3f v10 =
                                      v00 + ((v01 + v11) * 0.5f - v00) * 2.f;
                                  return {v00, v01, v11, v10};
                              }),
                          mk_isect_t(bse_quad_isect),
                          intersect,
                          cmp_isects),
        "ray-quad intersection",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(
            [&]() noexcept { return std::tuple{gf11(), gf11()}; },
            bse_ray_dir_test,
            [](const float ndc0, const float ndc1) noexcept
            {
                return to_tuple(
                    cornell_box<float>::camera()(Vec2f{ndc0, ndc1}).dir);
            },
            test::uni_tuple_cmp{ulp_cmp{ulp}}),
        "cam ray dir",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref([&]() noexcept { return std::tuple{gf11(), geta()}; },
                          bse_schlick,
                          [](const auto cd, const auto eta) noexcept
                          { return schlick(cd, eta); },
                          ulp_cmp{ulp}),
        "schlick",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(
            [&]() noexcept
            { return std::tuple{normalize(gv3()), normalize(gv3())}; },
            [&](const Vec3f& n, const Vec3f& i) noexcept
            { return bse_reflect(n[0], n[1], n[2], i[0], i[1], i[2]); },
            [](const Vec3f& n, const Vec3f& i) noexcept
            { return to_tuple(reflect(n, i, dot(n, -i))); },
            test::uni_tuple_cmp{ulp_cmp{ulp}}),
        "reflect",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(
            [&]() noexcept
            { return std::tuple{normalize(gv3()), normalize(gv3())}; },
            [&](const Vec3f& n, const Vec3f& i) noexcept
            { return bse_refract(n[0], n[1], n[2], i[0], i[1], i[2]); },
            [](const Vec3f& n, const Vec3f& i) noexcept
            {
                const float cos_theta = dot(n, -i);
                const bool  into      = cos_theta > 0;
                const Vec3f nfn       = into ? n : -n;
                const float eta       = compute_eta(1.5f, into);
                const float ddn       = dot(i, nfn);
                const float cos2t     = 1 - eta * eta * (1 - ddn * ddn);

                return to_tuple(refract(nfn, i, eta, ddn, std::abs(cos2t)));
            },
            test::uni_tuple_cmp{ulp_cmp{ulp}}),
        "refract",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref([&]() noexcept
                          { return std::tuple{g_canon(), g_canon()}; },
                          bse_gtr,
                          [](const float dotWmWg, const float alpha) noexcept
                          { return gtr(dotWmWg, alpha); },
                          ulp_cmp{ulp}),
        "GTR",
        0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref([&]() noexcept
                          { return std::tuple{g_canon(), g_canon()}; },
                          bse_gaf,
                          [](const float ct, const float alpha) noexcept
                          { return gaf(ct, alpha); },
                          ulp_cmp{ulp}),
        "GAF",
        0.01));

    track.dep_check(
        NamedFuzzyTest(
            tm.fuzzy_with_ref(
                [&]() noexcept
                { return std::tuple{g_canon(), g_canon(), g_canon()}; },
                bse_get_slope,
                [](const float ct, const float u0, const float u1) noexcept
                { return to_tuple(get_slope(ct, Vec2f{u0, u1})); },
                test::uni_tuple_cmp{ulp_cmp{ulp}}),
            "get slope",
            0.01),
        NamedFuzzyTest(tm.fuzzy_with_ref(
                           [&]() noexcept
                           {
                               const auto wi = to_tuple(normalize(gv3()));
                               const auto a  = to_tuple(gv2s());
                               return std::tuple_cat(
                                   wi,
                                   a,
                                   std::tuple{g_canon(), g_canon()});
                           },
                           bse_microfacet,
                           [](float wi0,
                              float wi1,
                              float wi2,
                              float a0,
                              float a1,
                              float u0,
                              float u1) noexcept
                           {
                               return to_tuple(microfacet(Vec3f{wi0, wi1, wi2},
                                                          Vec2f{a0, a1},
                                                          Vec2f{u0, u1}));
                           },
                           test::uni_tuple_cmp{ulp_cmp{ulp}}),
                       "microfacet",
                       0.02));

    track.check(NamedFuzzyTest(tm.fuzzy_with_ref(
                                   [&]() noexcept
                                   {
                                       const auto wi =
                                           to_tuple(normalize(gv3()));
                                       const Vec3 n = normalize(gv3());
                                       const Vec3 tu =
                                           cross(n, normalize(gv3()));
                                       const Vec3 tv = cross(n, tu);
                                       return std::tuple_cat(wi,
                                                             to_tuple(tu),
                                                             to_tuple(tv),
                                                             to_tuple(gv2s()));
                                   },
                                   bse_anisorough_i,
                                   [](float wi0,
                                      float wi1,
                                      float wi2,
                                      float tu0,
                                      float tu1,
                                      float tu2,
                                      float tv0,
                                      float tv1,
                                      float tv2,
                                      float a0,
                                      float a1) noexcept
                                   {
                                       return anisorough_i(Vec3f{wi0, wi1, wi2},
                                                           Vec3f{tu0, tu1, tu2},
                                                           Vec3f{tv0, tv1, tv2},
                                                           Vec2f(a0, a1));
                                   },
                                   ulp_cmp{ulp}),
                               "anisorough_i",
                               0.01));

    track.check(NamedFuzzyTest(tm.fuzzy_with_ref(
                                   [&]() noexcept
                                   {
                                       const Vec3f wi = normalize(gv3());
                                       Vec3        n  = normalize(gv3());
                                       if(dot(n, wi) < 0) n = -n;
                                       const Vec3 wm = normalize(wi + n);
                                       const Vec3 tu =
                                           cross(n, normalize(gv3()));
                                       const Vec3 tv = cross(n, tu);
                                       return std::tuple_cat(to_tuple(wm),
                                                             to_tuple(n),
                                                             to_tuple(tu),
                                                             to_tuple(tv),
                                                             to_tuple(gv2s()));
                                   },
                                   bse_anisorough_n,
                                   [](float wm0,
                                      float wm1,
                                      float wm2,
                                      float n0,
                                      float n1,
                                      float n2,
                                      float tu0,
                                      float tu1,
                                      float tu2,
                                      float tv0,
                                      float tv1,
                                      float tv2,
                                      float a0,
                                      float a1) noexcept
                                   {
                                       const Vec3f wm{wm0, wm1, wm2};
                                       const Vec3f wg{n0, n1, n2};
                                       return anisorough_n(wm,
                                                           wg,
                                                           Vec3f{tu0, tu1, tu2},
                                                           Vec3f{tv0, tv1, tv2},
                                                           dot(wm, wg),
                                                           Vec2f(a0, a1));
                                   },
                                   ulp_cmp{ulp}),
                               "anisorough_n",
                               0.01));

    return track.ret_code();
}
