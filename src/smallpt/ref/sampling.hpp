// -*- C++ -*-
// sampling.hpp
//

#pragma once

#include "rng.hpp"
#include "typedefs.hpp"

#include <common/tuple.hpp>

#include <random>

namespace loom
{
/// @brief Generate vector of two random numbers [0, 1).
Vec2<float>
sample_u(const std::array<std::uint32_t, 2>& u) noexcept
{
    return transform(gen_canonical, Vec2<std::uint32_t>(u));
}

template <std::floating_point T>
Vec2<T>
jitter_sample_pos(const T            width,
                  const Vec2<T>&     pos,
                  const Vec2<float>& u) noexcept
{
    return (pos - width * static_cast<T>(0.5)) + u * width;
}

template <std::floating_point T>
Vec3<T>
sample_hemisphere(const Vec3<T>& n, const Vec2<T>& u) noexcept
{
    const auto& [u1, u2] = u;
    const T       r1     = static_cast<T>(2) * Pi<T> * (u1 - 1);
    const T       r2s    = std::sqrt(u2);
    const Vec3<T> x =
        normalize(cross(std::abs(n[0]) > static_cast<T>(.1) ? Vec3<T>(0, 1, 0) :
                                                              Vec3<T>(1, 0, 0),
                        n));
    const Vec3<T> y = cross(n, x);
    return normalize(x * std::cos(r1) * r2s + y * std::sin(r1) * r2s +
                     n * std::sqrt(1 - u2));
}

/// @brief Compute ratio.
template <std::floating_point T>
T
ratio(const Vec2<T>& res) noexcept
{
    const auto& [w, h] = res;
    return w / h;
}

/// @brief Pixel position to film sample space.
template <std::floating_point T>
class ToSampleSpace
{
    Vec2<T> m_res;
    T       m_ratio;

  public:
    ToSampleSpace(const Vec2u& res) noexcept : m_res(res), m_ratio(ratio(m_res))
    {
    }

    Vec2<T>
    operator()(const Vec2u& pos) const noexcept
    {
        const auto& [x, y] = pos;
        const auto [w, h]  = m_res - static_cast<T>(1);
        const T sx = m_ratio * (static_cast<T>(x) / w - static_cast<T>(0.5));
        const T sy = static_cast<T>(y) / h - static_cast<T>(0.5);
        return Vec2<T>{sx, sy};
    }
};
} // namespace loom
