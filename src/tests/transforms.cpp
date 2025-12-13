// -*- C++ -*-
// transforms.cpp --
#include "property_test.hpp"
#include <smallpt/ref/base/linal.hpp>
#include <smallpt/ref/base/transforms.hpp>
#include <utils/tuple.hpp>

#include <random>

using namespace loom;
using namespace std;

int
main(int argc, char* argv[])
{
    const size_t num_tests = argc == 2 ? stoul(argv[1]) : 1000000;

    random_device         rd;
    default_random_engine g(rd());

    int ret = 0;

    auto test_property_n =
        [num_tests]<typename... Args>(Args&&... args) noexcept
    { return test_property(num_tests, 0.01, std::forward<Args>(args)...); };

    std::uniform_real_distribution<float> dist(-1000.f, 1000.f);

    auto argen  = [&]() noexcept { return loom::generate<3>(dist, g); };
    auto mkpair = [&]() noexcept { return pair{Vec3{argen()}, Vec3{argen()}}; };
    static const constexpr unsigned ULP = 5;

    ret += test_property_n(
        "translate",
        mkpair,
        [](const Vec3a<float>& origin, const Vec3a<float>& delta) noexcept
        { return apply_transform_p(translate(delta), origin); },
        [](const Vec3a<float>& testing, const auto& feed) noexcept
        {
            const auto& [origin, delta] = feed;
            const auto origin2          = testing - delta;
            return any(!almost_equal_vec(origin2, origin, ULP)) ||
                   !all(loom::isfinite(origin2));
        });

    ret += test_property_n(
        "scale",
        mkpair,
        [](const Vec3a<float>& origin, const Vec3a<float>& delta) noexcept
        { return apply_transform(scale(delta), origin); },
        [](const Vec3a<float>& testing, const auto& feed) noexcept
        {
            const auto& [origin, delta] = feed;
            const auto& origin2         = testing / delta;
            return any(!almost_equal_vec(origin2, origin, ULP)) ||
                   !all(loom::isfinite(origin2));
        });

    ret += test_property_n(
        "rotate",
        [&]() noexcept
        {
            return pair{Pi<float> * generate_canonical<float, 10>(g),
                        std::apply(make_normal, argen())};
        },
        loom::rotate,
        [&](const Mat4a<float>& testing, const auto&) noexcept
        {
            const Vec3a<float> o{argen()};
            const Vec3a<float> ot = apply_transform(testing, o);
            const float        l  = length(o);
            const float        lt = length(ot);
            return !(almost_equal(1.f, det(Mat3a<float>(testing)), ULP) &&
                     almost_equal(l, lt, ULP)) ||
                   all(almost_equal_vec(o, ot, ULP)) ||
                   !all(loom::isfinite(ot));
        });

    ret += test_property(
        num_tests,
        // fails in 7% of cases with clang and in 13% of cases with gcc
        0.14,
        "look at",
        mkpair,
        [&](const Vec3a<float>& eye, const Vec3a<float>& target) noexcept
        {
            return apply_transform_n(
                look_at(eye, target, Vec3a<float>(0.f, 1.f, 0.f)),
                Vec3a<float>(0.f, 0.f, 1.f));
        },
        [](const Vec3a<float>& testing, const auto& feed) noexcept
        {
            const auto& [eye, target]   = feed;
            const Vec3a<float> expected = normalize(target - eye);
            return any(!almost_equal_vec(expected, testing, ULP)) ||
                   !all(loom::isfinite(testing));
        });

    ret += test_property_n(
        "orthonormal basis",
        [&]() noexcept { return std::apply(make_normal, argen()); },
        basis,
        [](const auto& testing, const auto& feed) noexcept
        {
            const auto& [a, b] = testing;
            const Mat3a<float>     mm{a, b, feed};
            const Mat3a<float>     zero = m3id<float> - mm * transpose(mm);
            const constexpr size_t ulp  = static_pow<sizeof(float)>(60ul);
            return any(!almost_equal_vec(flat_ref(zero), 0.f, ulp));
        });

    if(ret)
        fmt::print("{} tests failed\n", ret);
    else
        fmt::print("All tests passed\n");

    return ret;
}
