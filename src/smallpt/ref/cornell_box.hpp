// -*- C++ -*-
/// @file cornell_box.hpp
/// @brief Cornell Box scene description.

#pragma once

#include "niebloids.hpp"
#include "perspective_camera.hpp"
#include "primitives/cylinder.hpp"
#include "primitives/disk.hpp"
#include "primitives/quad.hpp"
#include "primitives/sphere.hpp"
#include "primitives/triangle.hpp"
#include "sampling.hpp"

#include <common/forbid.hpp>
#include <common/math.hpp>
#include <smallpt/common/typedefs.hpp>

namespace loom
{
/// @brief Fresnel term approximation.
/// @param cd cosine theta.
/// @param eta relative index of refraction.
///
/// See @cite 10.1111:1467-8659.1320121
template <std::floating_point T>
static T
schlick(const T cd, const T eta) noexcept
{
    T r0 = (1 - eta) / (1 + eta);
    r0 *= r0;
    return r0 + (1 - r0) * static_pow<5>(1 - cd);
}

/// @brief Compute relative index of refraction.
///
/// Incident medium is vacuum (1).
template <std::floating_point T>
static T
compute_eta(const T ior, const bool enter) noexcept
{
    return enter ? 1 / ior : ior;
}

template <std::floating_point T>
static T
reflectance(const T cos_theta, const T eta) noexcept
{
    return schlick(std::abs(cos_theta), eta);
}

template <std::floating_point T>
static Vec3<T>
reflect(const Vec3<T>& n, const Vec3<T>& i, const T cos_theta) noexcept
{
    return i + n * cos_theta * static_cast<T>(2);
}

/// @brief Trowbridge-Reitz microfacet distribution.
/// @param dotWmWg dot product of normal and microfacet.
/// @param alpha surface roughness.
///
/// See @cite Trowbridge:75 and @cite 10.2312:EGWR/EGSR07/195-206
template <std::floating_point T>
static T
gtr(const T dotWmWg, const T alpha) noexcept
{
    const T D = alpha / (alpha + 1 / (dotWmWg * dotWmWg) - 1);
    return D * D;
}

/// @brief Microfacet geometry attenuation factor.
/// @param nu cosine of angle.
/// @param alpha surface rougness.
///
/// See @cite 10.2312:EGWR/EGSR07/195-206
template <std::floating_point T>
static T
gaf(const T nu, const T alpha)
{
    return 2 / (1 + std::sqrt(1 + alpha * (1 / (nu * nu) - 1)));
}

/// @brief Cornell box description.
template <std::floating_point real>
class cornell_box
{
    using Vec2r = Vec2<real>;
    using Vec3r = Vec3<real>;
    using Mat4r = Mat4<real>;
    using Rgb   = RGB<std::array>;

    // Box points coordinates
    static constexpr std::array<Vec3r, 8> room_points{Vec3r(-1, -1, 1),  // 0
                                                      Vec3r(-1, -1, -1), // 1
                                                      Vec3r(1, -1, -1),  // 2
                                                      Vec3r(1, -1, 1),   // 3
                                                      Vec3r(-1, 1, 1),   // 4
                                                      Vec3r(-1, 1, -1),  // 5
                                                      Vec3r(1, 1, -1),   // 6
                                                      Vec3r(1, 1, 1)};   // 7

    // Quad point indices for room geometry
    using room_geo_descr =
        type_sequence<std::index_sequence<0, 3, 2, 1>,  // floor
                      std::index_sequence<0, 1, 5, 4>,  // left wall
                      std::index_sequence<2, 3, 7, 6>,  // right wall
                      std::index_sequence<4, 5, 6, 7>,  // ceiling
                      std::index_sequence<1, 2, 6, 5>>; // back wall

    static constexpr real tr = smath::sqrt(static_cast<real>(3)) / 6;
    static constexpr real tR = 2 * tr;
    // Tetrahedron vertices
    static constexpr std::array<Vec3r, 4> tetrahedron_points{
        Vec3r(-0.5, 0, -tr),   // 0
        Vec3r(0, 0, tR),       // 1
        Vec3r(0.5, 0, -tr),    // 2
        Vec3r(0, tr + tR, 0)}; // 3

    // Triangle point indices for tetrahedron
    using tetrahedron_geo_descr = type_sequence<std::index_sequence<0, 1, 3>,
                                                std::index_sequence<1, 2, 3>,
                                                std::index_sequence<2, 0, 3>>;

    static constexpr Mat4r tetrahedron_transform =
        translate(Vec3r{0.1, -1, -0.35}) *
        rotate(radians(static_cast<real>(-10)), Vec3r{0, 1, 0}) *
        scale(Vec3r{0.95});

    static constexpr Vec3r cyl_pos{-0.5, -1, -0.1};
    static constexpr real  cyl_rad{0.25};
    static constexpr real  cyl_h{0.8};

  public:
    FORBID_ALL(cornell_box);

    struct Gallery
    {
        struct Lambert
        {
            const Rgb color{0};
        };

        struct Emission
        {
            static constexpr real value = 20;
        };

        struct Glass
        {
            static constexpr real ior = 1.5;
        };

        struct Plastic
        {
            static constexpr real ior = 2.5;
            static constexpr Rgb  color{0.5, 0.7, 1};
        };

        struct Metall
        {
            static constexpr real alpha = 0.1;
            static constexpr Rgb  color{1};
        };

        static constexpr std::tuple materials{
            Lambert{Rgb{1}},           // 0
            Lambert{Rgb{1, 0.4, 0.2}}, // 1
            Lambert{Rgb{0.2, 1, 0.4}}, // 2
            Plastic{},                 // 3
            Glass{},                   // 4
            Metall{},                  // 5
            Emission{}                 // 6
        };

        // Material indicies for primitives
        static constexpr unsigned mat_idxs[]{
            // room of 5 primitives
            0, // grey floor
            1, // red wall
            2, // green wall
            0, // grey ceiling
            0, // grey wall
               // objects
            3, // plastic tetrahedron of three primitives
            3,
            3,
            5, // glass sphere
            5, // metallic cylinder
            5, // metallic cylinder cap
            6  // light
        };
    };

    static constexpr Disk<real> light{
        Vec3r{0, 0.9999, 0},
        Vec3r{0,     -1, 0},
        0.35
    };

    static constexpr auto geometry = std::tuple_cat(
        // room
        typelist::maybe_collect(
            room_geo_descr{},
            []<typename idxs>() noexcept -> Quad<real>
            {
                return intlist::collect(idxs{},
                                        [](const std::size_t i) noexcept
                                        { return room_points[i]; });
            }),
        // objects
        typelist::maybe_collect(tetrahedron_geo_descr{},
                                []<typename idxs>() noexcept -> Triangle<real>
                                {
                                    return apply_transform(
                                        tetrahedron_transform,
                                        intlist::collect(
                                            idxs{},
                                            [](const std::size_t i) noexcept
                                            { return tetrahedron_points[i]; }));
                                }),
        std::tuple{
            Sphere<real>{Vec3r{0.5, -0.7, 0.4}, 0.3},
            Cylinder<real>{cyl_pos, cyl_rad, cyl_h},
            Disk<real>{cyl_pos + Vec3r{0, cyl_h, 0}, Vec3r{0, 1, 0}, cyl_rad},
            light});

    static Perspective<real>
    camera(const real fov = radians(static_cast<real>(110))) noexcept
    {
        return {convert_fov(fov), Vec3r(0, 0, 2), Vec3r(0, 0, -1)};
    }
};
} // namespace loom
