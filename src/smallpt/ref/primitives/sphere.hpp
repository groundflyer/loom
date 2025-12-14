// -*- C++ -*-
/// @file sphere.hpp
/// @brief Sphere primitive definition and auxiliary functions.

#pragma once

#include "aabb.hpp"

namespace loom
{
/// @brief Sphere primitive.
template <std::floating_point T>
struct Sphere
{
    Vec3a<T> center;
    T        radius;

    constexpr Sphere() noexcept = default;

    template <template <typename, std::size_t> typename Container1>
    constexpr Sphere(const Vec3<T, Container1>& _center,
                     const T                    _radius) noexcept :
        center(_center), radius(_radius)
    {
    }
};

template <std::floating_point T,
          template <typename, std::size_t> typename Container>
Sphere(const Vec3<T, Container>&, const T) -> Sphere<T>;

namespace fn
{
/// @brief Compute ray-sphere intersection.
template <std::floating_point T>
constexpr Intersection<T>
intersect(const Ray<T>& ray, const Sphere<T>& prim) noexcept
{
    const auto& [o, d]  = ray;
    const T        a    = dot(d, d);
    const Vec3a<T> pc   = o - prim.center;
    const T        b    = dot(d, pc * static_cast<T>(2));
    const T        c    = dot(pc, pc) - static_pow<2>(prim.radius);
    const auto [hit, t] = quadratic(a, b, c);
    return {hit, t, Vec2a<T>{}};
}

/// @brief Compute surface normal at @p pos.
template <std::floating_point T,
          template <typename, std::size_t> typename PosContainer>
constexpr Vec3a<T>
normal(const Vec3<T, PosContainer>& pos, const Sphere<T>& prim) noexcept
{
    return normalize(pos - prim.center);
}

/// @brief Sample a point on sphere surface.
template <std::floating_point T,
          template <typename, std::size_t> typename SContainer>
constexpr Vec3a<T>
sample(const Vec2<T, SContainer>& s, const Sphere<T>& prim) noexcept
{
    const auto& [c, r]   = prim;
    const auto& [t1, t2] = s;
    const T u            = 2 * t1 - 1;
    const T theta        = 2 * Pi<T> * t2;
    const T _u           = sqrt(1 - u * u);
    return Vec3a<T>(_u * cos(theta), _u * sin(theta), u) * r + c;
}

/// @brief Return sphere position.
template <std::floating_point T>
constexpr Vec3a<T>
centroid(const Sphere<T>& prim) noexcept
{
    return prim.center;
}

/// @brief Compute bounding box of a sphere.
template <std::floating_point T>
constexpr AABB<T>
bound(const Sphere<T>& prim) noexcept
{
    const auto& [c, r] = prim;
    return {c - r, c + r};
}

/// @brief Change sphere position using a transformation matrix.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer>
constexpr Sphere<T>
apply_transform(const Mat4<T, MatContainer>& t, const Sphere<T>& prim) noexcept
{
    const auto& [c, r] = prim;
    return Sphere(apply_transform_p(t, c), r);
}
} // namespace fn
} // namespace loom
