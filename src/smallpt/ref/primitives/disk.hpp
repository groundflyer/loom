// -*- C++ -*-
/// @file disk.hpp
/// @brief Disk primitive definition and auxiliary functions.

#pragma once

#include "aabb.hpp"

namespace loom
{
/// @brief Disk definition.
template <std::floating_point T>
struct Disk
{
    Vec3a<T> position;
    Vec3a<T> normal;
    T        radius;

    constexpr Disk() noexcept = default;

    template <template <typename, std::size_t> typename Container1,
              template <typename, std::size_t> typename Container2>
    constexpr Disk(const Vec3<T, Container1>& _position,
                   const Vec3<T, Container2>& _normal,
                   const T                    _radius) noexcept :
        position(_position), normal(normalize(_normal)), radius(_radius)
    {
    }
};

template <std::floating_point T,
          template <typename, std::size_t> typename Container>
Disk(const Vec3<T, Container>&, const Vec3<T, Container>&, const T) -> Disk<T>;

namespace fn
{
/// @brief Compute ray-disk intersection.
template <std::floating_point T>
constexpr Intersection<T>
intersect(const Ray<T>& ray, const Disk<T>& prim) noexcept
{
    const auto& [o, d]    = ray;
    const auto& [p, n, r] = prim;
    const T t             = dot(p - o, n) / dot(d, n);
    return {!std::signbit(t) && distance(o + d * t, p) < r, t, Vec2a<T>{}};
}

/// @brief Return disk normal.
template <std::floating_point T,
          template <typename, std::size_t> typename PosContainer>
constexpr Vec3a<T>
normal(const Vec3<T, PosContainer>&, const Disk<T>& prim) noexcept
{
    return prim.normal;
}

/// @brief Sample a point on disk surface.
template <std::floating_point T,
          template <typename, std::size_t> typename SContainer>
constexpr Vec3a<T>
sample(const Vec2<T, SContainer>& s, const Disk<T>& prim) noexcept
{
    const auto& [p, zaxis, r] = prim;
    const auto& [t1, t2]      = s;
    const T        sr         = r * sqrt(t1);
    const T        theta      = static_cast<T>(2) * Pi<T> * t2;
    const Vec3a<T> point{sr * sin(theta), sr * cos(theta), 0};
    return dot(basis_matrix(zaxis), point) + p;
}

/// @brief Return disk position.
template <std::floating_point T>
constexpr Vec3a<T>
centroid(const Disk<T>& prim) noexcept
{
    return prim.position;
}

/// @brief Compute axis-aligned box bounding a disk.
template <std::floating_point T>
constexpr AABB<T>
bound(const Disk<T>& prim) noexcept
{
    const auto& [p, n, r] = prim;
    const T h             = abs(
        sqrt(static_cast<T>(1) - static_pow<2>(dot(n, Vec3a<T>(0, 1, 0)))) * r);
    const Vec3a<T> offset(r, h, r);
    return {p - offset, p + offset};
}

/// @brief Transform disk with a transformation matrix.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer>
constexpr Disk<T>
apply_transform(const Mat4<T, MatContainer>& t, const Disk<T>& prim) noexcept
{
    const auto& [p, n, r] = prim;
    return Disk(apply_transform_p(t, p), apply_transform_n(t, n), r);
}
} // namespace fn
} // namespace loom
