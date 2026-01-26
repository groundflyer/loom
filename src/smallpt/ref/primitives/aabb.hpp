// -*- C++ -*-
/// @file aabb.hpp
/// @brief Definition and functions for axis-aligned bounding box.

#pragma once

#include "../ray.hpp"

#include <utils/math.hpp>

namespace loom
{
/// @brief Axis-aligned bounding box.
template <std::floating_point T>
struct AABB
{
    Vec3<T> vmin;
    Vec3<T> vmax;

    constexpr AABB() noexcept = default;

    template <template <typename, std::size_t> typename Container1,
              template <typename, std::size_t> typename Container2>
    constexpr AABB(const Vec3<T, Container1>& _vmin,
                   const Vec3<T, Container2>& _vmax) noexcept :
        vmin(_vmin), vmax(_vmax)
    {
    }

    constexpr const auto&
    operator[](const bool i) const noexcept
    {
        return i ? vmax : vmin;
    }

    constexpr auto&
    operator[](const bool i) noexcept
    {
        return i ? vmax : vmin;
    }

    /// @brief Select vertex components individually.
    /// @param idxs Vector of indicies (0, 1) to select.
    ///
    /// Creates vector containing selected components
    /// of @p vmin (0) and @p vmax (1).
    ///
    /// This used in intersect function.
    template <template <typename, std::size_t> typename Container1>
    constexpr Vec3<T>
    operator[](const Vector<bool, 3, Container1>& idxs) const noexcept
    {
        Vec3<T> ret{};
        for(std::size_t i = 0; i < 3; ++i) ret[i] = (*this)[bool{idxs[i]}][i];
        return ret;
    }
};

template <std::floating_point T,
          template <typename, std::size_t> typename Container>
AABB(const Vec3<T, Container>&, const Vec3<T, Container>&) -> AABB<T>;

namespace fn
{
/// @brief Axis-aligned bounding box intersection function.
///
/// Implements algorithm described in @cite 10.1145/1198555.1198748
template <std::floating_point T>
constexpr Intersection<T>
intersect(const Ray<T>& ray, const AABB<T>& prim) noexcept
{
    const auto& [o, d]             = ray;
    const Vec3<T> inv_d           = Vec3<T>{1} / d;
    const auto     sign            = inv_d < static_cast<T>(0);
    const Vec3<T> vmin            = inv_d * (prim[sign] - o);
    const Vec3<T> vmax            = inv_d * (prim[!sign] - o);
    const auto& [xmin, ymin, zmin] = vmin;
    const auto& [xmax, ymax, zmax] = vmax;
    const T    tmin1               = std::max(ymin, xmin);
    const T    tmax1               = std::min(ymax, xmax);
    const T    tmin2               = std::max(zmin, tmin1);
    const bool intersected =
        xmin <= ymax && ymin <= xmax && tmin1 <= zmax && zmin <= tmax1;
    return {intersected && !std::signbit(tmin2), tmin2, Vec2<T>{}};
}

/// @brief Compute normal of AABB at @p pos.
template <std::floating_point T>
constexpr Vec3<T>
normal(const Vec3<T>& pos, const AABB<T>& prim) noexcept
{
    const T  nsign[2]{-1, 1};
    Vec3<T> vd[2]{};
    T        md[2]{};
    for(unsigned i = 0; i < 2; ++i)
    {
        const Vec3 ad = loom::abs(pos - prim[bool(i)]);
        md[i]         = loom::min(ad);
        vd[i]         = Vec3(ad == md[i]);
    }
    const auto pp = md[0] > md[1];
    return normalize(vd[pp] * nsign[pp]);
}

namespace detail
{
template <std::floating_point T>
constexpr std::pair<Vec3<T>, Vec3<T>>
diag(const AABB<T>& prim, const unsigned shift, const bool side) noexcept
{
    const auto& [vmin, vmax]   = prim;
    const Vec3<T>& startpoint = side ? vmin : vmax;
    const Vec3<T>  diag       = side ? vmax - vmin : vmin - vmax;
    return std::pair{startpoint,
                     startpoint + diag * roll(Vec3<T>(1, 1, 0), shift)};
}
} // namespace detail

/// @brief Uniformly sample position on AABB surface.
template <std::floating_point T,
          template <typename, std::size_t> typename SContainer>
constexpr Vec3<T>
sample(const Vec2<T, SContainer>& s, const AABB<T>& prim) noexcept
{
    const auto& [s1, s2] = s;
    const T        s3 = fmod(static_cast<T>(2) * (s1 + s2), static_cast<T>(1));
    const unsigned shift = static_cast<unsigned>(
        static_cast<T>(300) * fmod(s1 + s2 + s3, static_cast<T>(1)));
    const auto [a, b] = detail::diag(prim, shift, s3 > static_cast<T>(0.5));
    return loom::transform(
        [](const auto&... args) noexcept { return loom::lerp(args...); },
        a,
        b,
        roll(Vec3<T>(resample(s1), resample(s2), static_cast<T>(0)), shift));
}

/// @brief Compute AABB centroid.
template <std::floating_point T>
constexpr Vec3<T>
centroid(const AABB<T>& prim) noexcept
{
    const auto& [vmin, vmax] = prim;
    return (vmin + vmax) * static_cast<T>(0.5);
}

/// @brief Returns copy of itself.
template <std::floating_point T>
constexpr AABB<T>
bound(const AABB<T>& prim) noexcept
{
    return prim;
}

/// @brief Apply transformation matrix to AABB.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer>
constexpr AABB<T>
apply_transform(const Mat4<T, MatContainer>& t, const AABB<T>& prim) noexcept
{
    const auto& [vmin, vmax] = prim;
    return AABB(apply_transform_p(t, vmin), apply_transform_p(t, vmax));
}
} // namespace fn
} // namespace loom
