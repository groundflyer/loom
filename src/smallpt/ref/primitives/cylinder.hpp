// -*- C++ -*-
/// @file cylinder.hpp
/// @brief Reference cylinder primitive implementation.
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
/// @brief Simple Y-oriented cylinder.
template <std::floating_point T>
struct Cylinder
{
    Vec3<T> position;
    T       radius;
    T       height;
};

namespace fn
{
template <std::floating_point T>
constexpr Intersection<T>
intersect(const Ray<T>& ray, const Cylinder<T>& prim) noexcept
{
    const auto& [o, d]    = ray;
    const auto& [p, r, h] = prim;

    const auto [ox, oy, oz] = o - p;
    const auto [dx, dy, dz] = d;

    const T a = dx * dx + dz * dz;
    const T b = 2 * (ox * dx + oz * dz);
    const T c = ox * ox + oz * oz - r * r;

    const auto [hit, t] = quadratic(a, b, c);

    const T y = oy + dy * t;

    return {hit && y >= 0 && y <= h, t, Vec2<T>{}};
}

/// @brief Return cylinder normal.
template <std::floating_point T>
constexpr Vec3<T>
normal(Vec3<T> pos, const Cylinder<T>& prim) noexcept
{
    Vec3<T> p = prim.position;
    p[1]      = 0;
    pos[1]    = 0;
    return normalize(pos - p);
}

template <std::floating_point T,
          template <typename, std::size_t> typename SContainer>
constexpr Vec3<T>
sample(const Vec2<T, SContainer>& s, const Cylinder<T>& prim) noexcept
{
    const auto [p, r, h] = prim;
    const auto [t1, t2]  = s;

    const T theta = static_cast<T>(2) * Pi<T> * t2;
    return p + Vec3<T>{std::sin(theta), 0, std::cos(theta)} * r +
           Vec3<T>{0, t2 * h, 0};
}

template <std::floating_point T>
constexpr Vec3<T>
centroid(const Cylinder<T>& prim) noexcept
{
    Vec3<T> p = prim.position;
    p[1] += prim.height * static_cast<T>(0.5);
    return p;
}

template <std::floating_point T>
constexpr AABB<T>
bound(const Cylinder<T>&) noexcept
{
    return {}; // mock
}

/// @brief Transform disk with a transformation matrix.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer>
constexpr Cylinder<T>
apply_transform(const Mat4<T, MatContainer>& t,
                const Cylinder<T>&           prim) noexcept
{
    const auto& [p, r, h] = prim;
    return Cylinder(apply_transform_p(t, p), r, h);
}
} // namespace fn
} // namespace loom
