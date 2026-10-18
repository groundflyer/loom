// -*- C++ -*-
/// @file disk.hpp
/// @brief Disk primitive definition and auxiliary functions.
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include "aabb.hpp"

namespace loom
{
/// @brief Disk definition.
template <std::floating_point T>
struct Disk
{
    Vec3<T> position;
    Vec3<T> normal;
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
    return {!std::signbit(t) && distance(o + d * t, p) < r, t, Vec2<T>{}};
}

/// @brief Return disk normal.
template <std::floating_point T,
          template <typename, std::size_t> typename PosContainer>
constexpr Vec3<T>
normal(const Vec3<T, PosContainer>&, const Disk<T>& prim) noexcept
{
    return prim.normal;
}

/// @brief Sample a point on disk surface.
template <std::floating_point T,
          template <typename, std::size_t> typename SContainer>
constexpr Vec3<T>
sample(const Vec2<T, SContainer>& s, const Disk<T>& prim) noexcept
{
    const auto& [p, n, r] = prim;
    const auto& [u0, u1]  = s;
    const T sr            = r * sqrt(u0);
    const T theta         = static_cast<T>(2) * Pi<T> * (u1 - T{0.5});
    const auto [x, y]     = basis(n);
    return x * sr * std::sin(theta) + y * sr * std::cos(theta) + p;
}

/// @brief Return disk position.
template <std::floating_point T>
constexpr Vec3<T>
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
        sqrt(static_cast<T>(1) - static_pow<2>(dot(n, Vec3<T>(0, 1, 0)))) * r);
    const Vec3<T> offset(r, h, r);
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
