// -*- C++ -*-
/// @file quad.hpp
/// @brief Difinition of quadrilateral primitive and auxiliary functions.

#pragma once

#include "triangle.hpp"

namespace loom
{
/// @brief We define quadrilateral as an array of vertices.
///
/// Vertices set in this order:
/// v00, v01, v11, v10.
template <std::floating_point T>
using Quad = std::array<Vec3a<T>, 4>;

namespace fn
{
/// @brief Compute ray-quadrilateral intersection.
///
/// Implements @cite LD2004AERIT
template <std::floating_point R>
constexpr Intersection<R>
intersect(const Ray<R>& ray, const Quad<R>& prim) noexcept
{
    using V3 = Vec3a<R>;

    const auto& [o, d]               = ray;
    const auto& [v00, v01, v11, v10] = prim;

    const V3 e01 = v10 - v00;
    const V3 e03 = v01 - v00;
    const V3 p   = cross(d, e03);
    const R  D   = dot(e01, p);

    if(abs(D) < std::numeric_limits<R>::min()) return {};

    const V3 T     = o - v00;
    const R  alpha = dot(T, p) / D;

    constexpr range<R> range01{0, 1};

    if(!range01(alpha)) return {};

    const V3 Q    = cross(T, e01);
    const R  beta = dot(d, Q) / D;

    if(!range01(beta)) return {};

    if((alpha + beta) > 1)
    {
        const V3 e23 = v01 - v11;
        const V3 e21 = v10 - v11;
        const V3 p_  = cross(d, e21);
        const R  D_  = dot(e23, p_);

        if(abs(D_) < std::numeric_limits<R>::min()) return {};

        const V3 T_     = o - v11;
        const R  alpha_ = dot(T_, p_) / D_;

        if(alpha_ < 0) return {};

        const V3 Q_    = cross(T_, e23);
        const R  beta_ = dot(d, Q_) / D_;
        if(beta_ < 0) return {};
    }

    const R t = dot(e03, Q) / D;

    if(t < 0) return {};

    const V3 e02                = v11 - v00;
    const V3 N                  = cross(e01, e03);
    const V3 aN                 = abs(N);
    const auto& [Nx, Ny, Nz]    = N;
    const auto& [aNx, aNy, aNz] = aN;

    R a11{}, b11{};
    if((aNx >= aNy) && (aNx >= aNz))
    {
        a11 = (e02[1] * e03[2] - e02[2] * e03[1]) / Nx;
        b11 = (e01[1] * e02[2] - e01[2] * e02[1]) / Nx;
    }
    else if((aNy >= aNx) && (aNy >= aNz))
    {
        a11 = (e02[2] * e03[0] - e02[0] * e03[2]) / Ny;
        b11 = (e01[2] * e02[0] - e01[0] * e02[2]) / Ny;
    }
    else
    {
        a11 = (e02[0] * e03[1] - e02[1] * e03[0]) / Nz;
        b11 = (e01[0] * e02[1] - e01[1] * e02[0]) / Nz;
    }

    R u{}, v{};
    if(abs(a11 - 1) < std::numeric_limits<R>::min())
    {
        u = alpha;
        if(abs(b11 - 1) < std::numeric_limits<R>::min())
            v = beta;
        else
            v = beta / (u * (b11 - 1) + 1);
    }
    else if(abs(b11 - 1) < std::numeric_limits<R>::min())
    {
        v = beta;
        u = alpha / (v * (a11 - 1) + 1);
    }
    else
    {
        const R A     = -(b11 - 1);
        const R B     = alpha * (b11 - 1) - beta * (a11 - 1) - 1;
        const R C     = alpha;
        const R Delta = B * B - 4 * A * C;
        const R QQ = static_cast<R>(-0.5) * (B + std::copysign(sqrt(Delta), B));
        u          = QQ / A;

        if(!range01(u)) u = C / QQ;

        v = beta / (u * (b11 - 1) + 1);
    }

    return {true, t, Vec2a<R>(u, v)};
}

/// @brief Compute normal of a quadrilateral.
template <std::floating_point T,
          template <typename, std::size_t> typename PosContainer>
constexpr Vec3a<T>
normal(const Vec3<T, PosContainer>&, const Quad<T>& prim) noexcept
{
    const auto e01 = std::get<3>(prim) - std::get<0>(prim);
    const auto e03 = std::get<1>(prim) - std::get<0>(prim);
    return normalize(cross(e03, e01));
}

/// @brief Sample a point on a quadrilateral surface.
template <std::floating_point T,
          template <typename, size_t> typename SContainer>
constexpr Vec3a<T>
sample(const Vec2<T, SContainer>& s, const Quad<T>& prim) noexcept
{
    const auto& [a, c1, b, c2] = prim;
    const auto& [t1, t2]       = s;
    const auto& c              = t1 > static_cast<T>(0.5) ? c1 : c2;
    return Vec3a<T>(
        detail::triangle_sample(Vec2a<T>(resample(t1), t2), a, b, c));
}

/// @brief Compute centroid of a quadrilateral.
template <std::floating_point T>
constexpr Vec3a<T>
centroid(const Quad<T>& prim) noexcept
{
    const auto& [a, b, c, d] = prim;
    return a + b + c + d / static_cast<T>(4);
}

/// @brief Compute bounding box of quadrilateral.
template <std::floating_point T>
constexpr AABB<T>
bound(const Quad<T>& prim) noexcept
{
    return detail::bound(prim);
}

/// @brief Transform a quadrilateral using a transformation matrix.
template <std::floating_point T,
          template <typename, size_t> typename MatContainer>
constexpr Quad<T>
apply_transform(const Mat4<T, MatContainer>& t, const Quad<T>& prim) noexcept
{
    return std::apply([&](const auto&... points) noexcept
                      { return Quad{apply_transform_p(t, points)...}; },
                      prim);
}
} // namespace fn
} // namespace loom
