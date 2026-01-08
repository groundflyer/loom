// -*- C++ -*-
// sampling.hpp
//

#pragma once

#include "typedefs.hpp"

#include <utils/tuple.hpp>

#include <random>

namespace loom
{
template <typename Generator, std::floating_point T>
Vec2<T>
sample_pixel(Generator& g, const T _pixel_width, const Vec2<T>& pos) noexcept
{
    return (pos - _pixel_width * static_cast<T>(0.5)) +
           Vec2<T>(generate<2>(g)) * _pixel_width;
}

template <std::floating_point T>
Vec3<T>
sample_hemisphere(const Vec3<T>& n, const Vec2<T>& u) noexcept
{
    const auto& [u1, u2] = u;
    const T r            = static_cast<T>(2) * Pi<T> * u2;
    const T phi          = sqrt(static_cast<T>(1) - u1 * u1);
    return basis_matrix(n) * Vec3<T>(std::cos(r) * phi, std::sin(r) * phi, u1);
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

/// @brief Generate vector of two random numbers [0, 1).
template <std::floating_point T, typename G>
Vec2<T>
sample_u(G& g) noexcept
{
    return Vec2<T>{generate<2>(
        [&g]() noexcept { return std::generate_canonical<float, 10>(g); })};
}
} // namespace loom
