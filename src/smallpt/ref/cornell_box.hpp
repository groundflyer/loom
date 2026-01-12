// -*- C++ -*-
// cornell_box.hpp
//

/// @file
/// Cornell Box scene description.

#pragma once

#include "image.hpp"
#include "niebloids.hpp"
#include "perspective_camera.hpp"
#include "primitives/disk.hpp"
#include "primitives/quad.hpp"
#include "primitives/sphere.hpp"
#include "sampling.hpp"
#include "typedefs.hpp"

#include <utils/common.hpp>
#include <utils/math.hpp>

namespace loom
{
template <std::floating_point T>
T
schlick(const T cd, const T eta) noexcept
{
    T r0 = (1 - eta) / (1 + eta);
    r0 *= r0;
    return r0 + (1 - r0) * static_pow<5>(1 - cd);
}

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

    // Quad point indicies for room geometry
    static constexpr std::tuple room_geo_descr{
        std::index_sequence<0, 3, 2, 1>{}, // floor
        std::index_sequence<0, 1, 5, 4>{}, // left wall
        std::index_sequence<2, 3, 7, 6>{}, // right wall
        std::index_sequence<4, 5, 6, 7>{}, // ceiling
        std::index_sequence<1, 2, 6, 5>{}  // back wall
    };

    // Quad point indicies for box geometry
    static constexpr std::tuple box_geo_descr{
        std::index_sequence<1, 2, 3, 0>{},
        std::index_sequence<4, 5, 1, 0>{},
        std::index_sequence<6, 7, 3, 2>{},
        std::index_sequence<7, 6, 5, 4>{},
        std::index_sequence<5, 6, 2, 1>{},
        std::index_sequence<0, 4, 7, 3>{},
    };

    static constexpr Vec3r box_scale{0.25, 0.5, 0.25};
    static constexpr Vec3r box_pos{-0.5, get_y(box_scale) - 1, -0.25};
    static constexpr Mat4r box_tr =
        rotate(radians(static_cast<real>(20)), Vec3r{0, 1, 0}) *
        translate(box_pos) * scale(box_scale);

    // Box point positions.
    static constexpr std::array<Vec3r, 8> box_points = std::apply(
        [](const auto&... pns) noexcept
        { return std::array<Vec3r, 8>{apply_transform_p(box_tr, pns)...}; },
        room_points);

    using QuadR   = Quad<real>;

    template <std::size_t N, std::size_t... Pns>
    static constexpr QuadR
    make_wall_geo(const std::array<Vec3r, N>& points,
                  std::index_sequence<Pns...>) noexcept
    {
        return QuadR{std::get<Pns>(points)...};
    }

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
            Rgb color{0};
        };

        struct Glass
        {
            static constexpr real ior = 1.5;

            real
            compute_eta(const bool enter) const noexcept
            {
                return enter ? 1 / ior : ior;
            }

            real
            reflectance(const real cos_theta) const noexcept
            {
                return schlick(std::abs(cos_theta), compute_eta(cos_theta));
            }

            Vec3r
            reflect(const Vec3r& n,
                    const Vec3r& i,
                    real         cos_theta) const noexcept
            {
                return i + n * cos_theta * static_cast<real>(2);
            }
        };

        static constexpr std::tuple materials{
            Lambert{Rgb{0.8}},           // 0
            Lambert{Rgb{0.8, 0.1, 0.1}}, // 1
            Lambert{Rgb{0.1, 0.8, 0.1}}, // 2
            Lambert{Rgb{0.1, 0.1, 0.8}}, // 3
            Glass{},                     // 4
            Emission{Rgb{10}}            // 5
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
            3, // blue box of six primitives
            3,
            3,
            3,
            3,
            3,
            4, // sphere
            5 // light
        };
    };

    static constexpr auto shadow_set = std::tuple_cat(
        std::apply(
            [](const auto&... pns) noexcept
            {
                return std::tuple{make_wall_geo(box_points, pns)...}
                 ;
    },
            box_geo_descr), // box
        std::tuple{Sphere<real>{Vec3r{0.5, -0.7, 0.4}, 0.3}});

    static constexpr Disk<real> light{
        Vec3r{0, 0.9999, 0},
        Vec3r{0,     -1, 0},
        0.35
    };

    static constexpr real light_intensity = 1;

    static constexpr auto geometry = std::tuple_cat(
        // room
        std::apply([](const auto&... pns) noexcept
                   { return std::tuple{make_wall_geo(room_points, pns)...}; },
                   room_geo_descr),
        // objects
        shadow_set,
        std::tuple{light});

    static Perspective<real>
    camera(const real fov = radians(static_cast<real>(60))) noexcept
    {
        return {convert_fov(fov),
                look_at(Vec3r(0, 0, 4), Vec3r(0, 0, 0), Vec3r(0, 1, 0))};
    }
};
} // namespace loom
