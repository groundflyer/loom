// -*- C++ -*-
// cylinder.hpp
//

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

    const auto [xo, yo, zo] = o - p;
    const auto [xd, yd, zd] = d;

    const T a = xd * xd + zd * zd;
    const T b = 2 * xo * xd + 2 * zo * zd;
    const T c = xo * xo + zo * zo - r * r;

    auto [hit, t] = quadratic(a, b, c);

    if(!hit) return {hit, t, Vec2<T>{}};

    const Vec3<T> hitp = o + d * t;
    // const T       y    = yo + yd * t;
    const T y = hitp[1];

    hit = y <= (p[1] + h) && y >= p[1];

    return {hit, t, Vec2<T>{}};
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
sample(const Vec2<T, SContainer>&, const Cylinder<T>& prim) noexcept
{
    return prim.position; // mock
}

template <std::floating_point T>
constexpr Vec3<T>
centroid(const Cylinder<T>& prim) noexcept
{
    return prim.position; // mock
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
