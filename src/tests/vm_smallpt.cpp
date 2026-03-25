// -*- C++ -*-
// vm_smallpt.cpp
#include <common/linal.hpp>
#include <common/math.hpp>
#include <common/test.hpp>
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
TKERNEL5(sample_hemi_test, n0, n1, n2, u0, u1)
    INSERT_TMACRO(vm::sample_hemisphere, CS(d0), CS(d1), CS(d2), CS(n0), CS(n1), CS(n2), CS(u0), CS(u1), CS(x), CS(t0), CS(t1))
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
    INSERT_TMACRO(vm::prep_isect_sphere, CS(a), CS(b), CS(c), CS(o), CS(d), CS(p), CS(r), CS(tmp1), CS(tmp2), CS(vtmp1)),
    INSERT_TMACRO(vm::solve_quadratic, CS(hit), CS(t), CS(a), CS(b), CS(c), CS(tmp1), CS(tmp2))
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
    TMOV_CS(CS(d2), vm::default_cam::DIR2),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp))
TKERNEL_END3(d0, d1, d2);

// clang-format on

using Vec3f = Vector<float, 3, std::array>;
using Vec2f = Vector<float, 2, std::array>;
using Vec2u = Vector<unsigned, 2, std::array>;

static constexpr int ulp = 30;

template <typename T>
auto
to_tuple(const Disk<T>& prim) noexcept
{
    const auto& [p, n, r] = prim;
    return array2tuple(array_cat(p.data(), n.data(), std::array<T, 1>{r}));
}

template <typename T>
auto
to_tuple(const Sphere<T>& prim) noexcept
{
    return std::tuple_cat(array2tuple(prim.center.data()),
                          std::tuple{prim.radius});
}

template <typename T, std::size_t N>
auto
to_tuple(const std::array<T, N>& prim) noexcept
{
    return std::apply([](const auto&... vs) noexcept
                      { return std::tuple_cat(array2tuple(vs.data())...); },
                      prim);
}

template <typename T>
auto
to_tuple(const Ray<T>& ray) noexcept
{
    const auto& [o, d] = ray;
    return array2tuple(array_cat(o.data(), d.data()));
}

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns<100000>(argc, argv)};
    RandomState   rs;

    auto gu32    = rs.make_gen<std::uint32_t>(0, -1u);
    auto gur     = rs.make_gen<unsigned>(30, 1000);
    auto gf      = rs.make_gen<float>(-1e6, 1e6);
    auto gf11    = rs.make_gen<float>(-1, 1);
    auto g_canon = rs.make_gen<float>();
    auto gb      = rs.make_gen<bool>(0.5);
    auto gv2s    = [&]() noexcept { return Vec2f(generate<2>(g_canon)); };
    auto gv3     = [&]() noexcept { return Vec3f(generate<3>(gf)); };

    const auto cmp_tuple_arr = [](const auto& a, const auto& b) noexcept
    { return std::get<0>(a) == b[0] && std::get<1>(a) == b[1]; };
    const auto vec2_ulp_cmp = [](const auto& t, const auto& r) noexcept
    {
        return almost_equal(std::get<0>(t), r[0], ulp) &&
               almost_equal(std::get<1>(t), r[1], ulp);
    };
    const auto vec3_ulp_cmp = [](const auto& t, const auto& r) noexcept
    {
        return almost_equal(std::get<0>(t), r[0], ulp) &&
               almost_equal(std::get<1>(t), r[1], ulp) &&
               almost_equal(std::get<2>(t), r[2], ulp);
    };
    const auto cmp_isects = [](const auto& a, const auto& b) noexcept
    {
        const bool ai = a.intersect;
        const bool bi = b.intersect;
        if(ai && bi)
            return almost_equal(a.t, b.t, ulp);
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
    vm::bse::function<triangle_isect_test<float>> bse_triangle_isect;
    vm::bse::function<quad_isect_test<float>>     bse_quad_isect;
    vm::bse::function<ray_dir_test<float>>        bse_ray_dir_test;

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

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(
            [&]() noexcept
            {
                return std::pair{normalize(Vec3f{generate<3>(gf)}),
                                 Vec2f{generate<2>(g_canon)}};
            },
            [&bse_sample_hemi](const Vec3f& n, const Vec2f& u) noexcept
            { return bse_sample_hemi(n[0], n[1], n[2], u[0], u[1]); },
            [](const Vec3f& n, const Vec2f& u) noexcept
            { return sample_hemisphere(n, u); },
            vec3_ulp_cmp),
        "sample hemisphere",
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
                                       return f(Vec2u{x, y});
                                   },
                                   vec2_ulp_cmp),
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
                    if(tr == vr)
                    {
                        if(tr && vr) return almost_equal(tx, vx, ulp);
                        return true;
                    }

                    return false;
                }),
            "quadratic",
            0.01),
        NamedFuzzyTest(
            tm.fuzzy_with_ref(
                mk_isect_g([&]() noexcept { return Sphere{gv3(), gf()}; }),
                mk_isect_t(bse_sphere_isect),
                intersect,
                cmp_isects),
            "ray-sphere intersection",
            0.01));

    track.check(NamedFuzzyTest(
        tm.fuzzy_with_ref(mk_isect_g([&]() noexcept -> Triangle<float>
                                     { return {gv3(), gv3(), gv3()}; }),
                          mk_isect_t(bse_triangle_isect),
                          intersect,
                          cmp_isects),
        "ray-triangle intersection",
        0.025));

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
        tm.fuzzy_with_ref([&]() noexcept { return std::tuple{gf11(), gf11()}; },
                          bse_ray_dir_test,
                          [](const float ndc0, const float ndc1) noexcept
                          {
                              const auto cam = cornell_box<float>::camera();
                              const auto [d0, d1, d2] =
                                  cam(Vec2f{ndc0, ndc1}).dir;
                              return std::tuple{d0, d1, d2};
                          },
                          test::uni_tuple_cmp{ulp_cmp{ulp}}),
        "cam ray dir",
        0.01));

    return track.ret_code();
}
