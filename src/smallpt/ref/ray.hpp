// -*- C++ -*-
/// @file ray.hpp
/// @brief Ray and intersection definitions.
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

#include "transforms.hpp"

namespace loom
{
/// @brief Ray definition.
template <std::floating_point T>
struct Ray
{
    Vec3<T> origin;
    Vec3<T> dir;

    template <template <typename, std::size_t> typename Container1,
              template <typename, std::size_t> typename Container2>
    constexpr Ray(const Vec3<T, Container1>& _origin,
                  const Vec3<T, Container2>& _dir) noexcept :
        origin(_origin), dir(normalize(_dir))
    {
    }
};

template <std::floating_point T,
          template <typename, std::size_t> typename Container>
Ray(const Vec3<T, Container>&, const Vec3<T, Container>&) -> Ray<T>;

/// @brief Intersection definition.
template <std::floating_point T>
struct Intersection
{
    bool     intersect;
    T        t;
    Vec2<T> st;

    constexpr Intersection() noexcept :
        intersect(false), t(std::numeric_limits<T>::max()), st(0)
    {
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Intersection(const bool                 _intersect,
                           const T                    _t,
                           const Vec2<T, Container2>& _st) noexcept :
        intersect(_intersect),
        t(intersect ? _t : std::numeric_limits<T>::max()), st(_st)
    {
    }

    constexpr
    operator bool() const noexcept
    {
        return intersect;
    }
};

template <std::floating_point T,
          template <typename, std::size_t> typename Container>
Intersection(const bool, const T, const Vec2<T, Container>&) -> Intersection<T>;

namespace fn
{
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer>
constexpr auto
apply_transform(const Mat4<T, MatContainer>& t, const Ray<T>& ray) noexcept
{
    return Ray(apply_transform_p(t, ray.origin), apply_transform_n(t, ray.dir));
}
} // namespace fn
} // namespace loom
