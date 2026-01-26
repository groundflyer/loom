// -*- C++ -*-
/// @file triangle.hpp
/// @brief Triangle primitive definition and auxiliary functions.

#pragma once

#include "aabb.hpp"

namespace loom
{
/// @brief Defining triangle as array of vertices.
template <std::floating_point T,
          template <typename, std::size_t> typename Container = std::array>
using Triangle = std::array<Vec3<T, Container>, 3>;

namespace fn
{
/// @brief Compute ray-triangle intersection.
///
/// Implements classic algorithm described in
/// @cite 10.1080/10867651.1997.10487468
template <std::floating_point T>
constexpr Intersection<T>
intersect(const Ray<T>& ray, const Triangle<T>& prim) noexcept
{
    using V3 = Vec3<T>;

    const auto& [o, d]         = ray;
    const auto& [v0, v1, v2]   = prim;
    const V3   edge1           = v1 - v0;
    const V3   edge2           = v2 - v0;
    const V3   pvec            = cross(d, edge2);
    const T    D               = dot(edge1, pvec);
    const T    invD            = static_cast<T>(1) / D;
    const V3   tvec            = o - v0;
    const T    u               = dot(tvec, pvec) * invD;
    const V3   qvec            = cross(tvec, edge1);
    const T    v               = dot(d, qvec) * invD;
    const T    t               = dot(edge2, qvec) * invD;
    const bool not_intersected = almost_equal(D, static_cast<T>(0), 10) ||
                                 std::signbit(v) || std::signbit(t) ||
                                 u + v > 1;

    const bool intersected =
        range(static_cast<T>(0), static_cast<T>(1))(u) && !not_intersected;

    return {
        intersected,
        t,
        Vec2<T>{u, v}
    };
}

/// @brief Compute triangle normal.
template <std::floating_point T,
          template <typename, std::size_t> typename PosContainer>
constexpr Vec3<T>
normal(const Vec3<T, PosContainer>&, const Triangle<T>& prim) noexcept
{
    const auto& [v0, v1, v2] = prim;
    const Vec3<T> edge1      = v1 - v0;
    const Vec3<T> edge2      = v2 - v0;
    return normalize(cross(edge1, edge2));
}

namespace detail
{
template <std::floating_point T>
constexpr void
shift(T& x, T& y) noexcept
{
    x *= static_cast<T>(0.5);
    y -= x;
}

template <std::floating_point T>
constexpr Vec2<T>&
s2t(Vec2<T>& s) noexcept
{
    auto& [x, y] = s;

    if(y > x)
        shift(x, y);
    else
        shift(y, x);

    return s;
}

template <std::floating_point T,
          template <typename, std::size_t> typename Container2>
constexpr Vec3<T>
triangle_sample(Vec2<T>                    s,
                const Vec3<T, Container2>& v1,
                const Vec3<T, Container2>& v2,
                const Vec3<T, Container2>& v3) noexcept
{
    auto& [t1, t2] = s2t(s);
    return v1 * t1 + v2 * t2 + v3 * (static_cast<T>(1) - t1 - t2);
}

/// @brief Compute bounding box of primitive defined as array.
template <typename Prim>
constexpr auto
bound(const Prim& prim) noexcept
{
    const auto vmin = fold_tuple(prim, loom::min);
    const auto vmax = fold_tuple(prim, loom::max);
    return AABB{vmin, vmax};
}
} // namespace detail

/// @brief Sample a point on triangle surface.
///
/// Implements optimized sampling algorithm from @cite heitz:hal-02073696
template <std::floating_point T,
          template <typename, std::size_t> typename SContainer>
constexpr Vec3<T>
sample(const Vec2<T, SContainer>& s, const Triangle<T>& prim) noexcept
{
    return Vec3<T>(std::apply([&](const auto&... verts) noexcept
                              { return detail::triangle_sample(s, verts...); },
                              prim));
}

/// @brief Compute triangle centroid.
template <std::floating_point T>
constexpr Vec3<T>
centroid(const Triangle<T>& prim) noexcept
{
    const auto& [a, b, c] = prim;
    return (a + b + c) / static_cast<T>(3);
}

/// @brief Compute triangle bounding box.
template <std::floating_point T>
constexpr AABB<T>
bound(const Triangle<T>& prim) noexcept
{
    return detail::bound(prim);
}

/// @brief Transform triangle with a transformation matrix.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer>
constexpr Triangle<T>
apply_transform(const Mat4<T, MatContainer>& t,
                const Triangle<T>&           prim) noexcept
{
    return std::apply([&](const auto&... points) noexcept
                      { return Triangle{apply_transform_p(t, points)...}; },
                      prim);
}
} // namespace fn
} // namespace loom
