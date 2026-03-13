// -*- C++ -*-
// primitives.cpp
#include "property_test.hpp"

#include <smallpt/ref/niebloids.hpp>

#include <common/tuple.hpp>

#include <random>

using namespace std;
using namespace loom;

template <std::floating_point real>
constexpr real
radius(const Sphere<real>& prim) noexcept
{
    return prim.radius;
}

template <std::floating_point real>
constexpr real
radius(const AABB<real>& prim) noexcept
{
    return loom::distance(centroid(prim), prim.vmax);
}

int
main(int argc, char* argv[])
{
    const size_t num_tests = argc == 2 ? stoul(argv[1]) : 1000000;

    random_device         rd;
    default_random_engine g(rd());

    int ret = 0;

    using R  = float;
    using V2 = Vec2<R>;
    using V3 = Vec3<R>;
    using M4 = Mat4<R>;
    using Ir = Intersection<R>;
    using Rr = Ray<R>;

    constexpr R                    val_range = 1000000;
    std::uniform_real_distribution big_dist(-val_range, val_range);
    std::uniform_real_distribution rad_dist(0.1f, val_range);
    std::bernoulli_distribution    bern_dist(0.5);
    auto randr = static_cast<R (*)(std::default_random_engine&)>(
        std::generate_canonical<R, 8>);
    auto sgen    = [&]() noexcept { return V2{generate<2>(randr, g)}; };
    auto posgen  = [&]() noexcept { return V3(generate<3>(big_dist, g)); };
    auto normgen = [&]() noexcept
    { return std::apply(make_normal, generate<3>(big_dist, g)); };

    const auto sample_intersect_property =
        [](const auto& sampled, const auto& prim) noexcept
    {
        const auto& [target, origin] = sampled;
        const Rr tohit{origin, target - origin};
        const Rr tomiss{origin, origin - target};
        const Ir hit  = intersect(tohit, prim);
        const Ir miss = intersect(tomiss, prim);
        return !hit || miss ||
               !almost_equal(hit.t, loom::distance(target, origin), 10);
    };

    const auto bound_property =
        [](const auto& testing, const auto& prim) noexcept
    {
        const auto& [bound, ray] = testing;
        const Ir hit_prim        = intersect(ray, prim);
        const Ir hit_bound       = intersect(ray, bound);
        return !hit_bound || hit_prim.t < hit_bound.t;
    };

    const auto test_prim =
        [&](auto&& name, auto&& gen, auto&& sampler, auto&& property) noexcept
    { return test_property(num_tests, 0.05, name, gen, sampler, property); };

    const auto intersect_test_prim =
        [&](auto&& name, auto&& gen, auto&& sampler) noexcept
    { return test_prim(name, gen, sampler, sample_intersect_property); };

    const auto sample_prim = [&](const auto& prim) noexcept
    { return pair(loom::sample(sgen(), prim), posgen()); };

    const auto sample_selfoccluded_prim = [&](const auto& prim) noexcept
    {
        const V3   sampled_point = loom::sample(sgen(), prim);
        const V3   offset        = sampled_point - centroid(prim);
        const auto sign          = transform([](const R val) noexcept
                                    { return std::copysign(1.f, val); },
                                    offset);
        const V3   origin =
            sampled_point + Vec3(generate<3>(rad_dist, g)) * sign + offset;
        return pair(sampled_point, Vec3(origin));
    };

    const auto bound_gen = [&](const auto& prim) noexcept
    {
        const AABB<R> bbox = bound(prim);
        const V3      sp   = loom::sample(sgen(), prim);
        const auto    tb   = Vector(generate<3>(bern_dist, g));
        const auto offset  = transform([](const auto& a, const auto& b) noexcept
                                      { return a ? b : -b; },
                                      tb,
                                      Vec3(generate<3>(rad_dist, g)));
        const V3   origin(bbox[tb] + offset);
        return pair{bbox, Ray(origin, sp - origin)};
    };

    const auto bound_test_prim = [&](auto&& name, auto&& gen) noexcept
    { return test_prim(name, gen, bound_gen, bound_property); };

    const auto aabb_gen = [&]() noexcept
    { return tuple_maker<AABB<R>>(loom::minmax(posgen(), posgen())); };
    const auto sphere_gen = [&]() noexcept
    { return Sphere(posgen(), rad_dist(g)); };
    const auto disk_gen = [&]() noexcept
    { return Disk(posgen(), normgen(), rad_dist(g)); };
    const auto triangle_gen = [&]() noexcept
    { return Triangle<R>{posgen(), posgen(), posgen()}; };
    const auto quad_gen = [&]() noexcept
    {
        const V3   v00{0, 0, 0};
        const V3   v01{0, rad_dist(g), 0};
        const V3   v10{rad_dist(g), 0, 0};
        const V3   v11 = v00 + v01 + v10;
        const auto t =
            dot(loom::rotate(big_dist(g), normgen()), translate(posgen()));
        return apply_transform(t, Quad<R>{v00, v01, v11, v10});
    };

    const auto aabb_gen_l = [&](const R max_rad, const V3&) noexcept
    {
        return AABB(-Vec3(generate<3>(randr, g)) * max_rad * 0.5f,
                    Vec3(generate<3>(randr, g)) * max_rad * 0.5f);
    };
    const auto sphere_gen_l = [&](const R max_rad, const V3&) noexcept
    { return Sphere(V3(0), randr(g) * max_rad); };
    const auto disk_gen_l = [&](const R, const V3& n) noexcept
    { return Disk(V3(0), n, rad_dist(g)); };
    const auto triangle_gen_l = [&](const R, const V3& n) noexcept
    {
        const V3 v0{-rad_dist(g), -rad_dist(g), 0};
        const V3 v1{0, rad_dist(g), 0};
        const V3 v2{rad_dist(g), -rad_dist(g), 0};
        M4       t(basis_matrix(n));
        t[3][3] = 1;
        return apply_transform(t, Triangle<R>{v0, v1, v2});
    };
    const auto quad_gen_l = [&](const R, const V3& n) noexcept
    {
        const R    x = rad_dist(g);
        const R    y = rad_dist(g);
        const V3   v00{-x, -y, 0};
        const V3   v01{0, y * 2, 0};
        const V3   v10{x * 2, 0, 0};
        const auto v11 = v00 + v01 + v10;
        M4         t(basis_matrix(n));
        t[3][3] = 1;
        return apply_transform(t, Quad<R>{v00, v01, v11, v10});
    };

    const auto gp_gen = tuple{sphere_gen, triangle_gen, quad_gen, disk_gen};
    const auto gp_gen_l =
        tuple{sphere_gen_l, triangle_gen_l, quad_gen_l, disk_gen_l, aabb_gen_l};

    ret += intersect_test_prim("Disk: sample/trace", disk_gen, sample_prim);
    ret += intersect_test_prim("Sphere: sample/trace",
                               sphere_gen,
                               sample_selfoccluded_prim);
    ret += intersect_test_prim("AABB: sample/trace",
                               aabb_gen,
                               sample_selfoccluded_prim);
    ret += intersect_test_prim("Triangle: sample/trace",
                               triangle_gen,
                               sample_prim);
    ret += intersect_test_prim("Quad: sample/trace", quad_gen, sample_prim);

    ret += bound_test_prim("Sphere: bound", sphere_gen);
    ret += bound_test_prim("Disk: bound", disk_gen);
    ret += bound_test_prim("Triangle: bound", triangle_gen);
    ret += bound_test_prim("Quad: bound", quad_gen);

    if(ret)
        fmt::print("{} tests failed\n", ret);
    else
        fmt::print("All tests passed\n");

    return ret;
}
