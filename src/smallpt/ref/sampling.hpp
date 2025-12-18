// -*- C++ -*-
// sampling.hpp
//

#pragma once

#include "typedefs.hpp"

namespace loom
{
template <typename Generator, std::floating_point T>
Vec2a<T>
sample_pixel(Generator& g, const T _pixel_width, const Vec2a<T>& pos) noexcept
{
    return (pos - _pixel_width * static_cast<T>(0.5)) +
           Vec2a<T>(generate<2>(g)) * _pixel_width;
}

template <std::floating_point T>
constexpr Vec3a<T>
sample_hemisphere(const Vec3a<T>& n, const Vec2a<T>& u) noexcept
{
    const auto& [u1, u2] = u;
    const T r            = static_cast<T>(2) * Pi<T> * u2;
    const T phi          = sqrt(static_cast<T>(1) - u1 * u1);
    return basis_matrix(n) * Vec3a<T>(std::cos(r) * phi, std::sin(r) * phi, u1);
}
} // namespace loom
