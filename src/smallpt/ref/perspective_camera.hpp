// -*- C++ -*-
// perspective.hpp --

#pragma once

#include "ray.hpp"

namespace loom
{
template <std::floating_point real>
constexpr real
convert_fov(const real fov) noexcept
{
    return std::tan(fov * real(0.5));
}

template <std::floating_point real>
constexpr real
invert_fov(const real f) noexcept
{
    return std::atan(f) * 2;
}

template <std::floating_point real>
struct Perspective
{
    real       tan_fov_half{1};
    Vec3<real> pos;
    Vec3<real> dir;

    template <template <typename, std::size_t> typename Container>
    constexpr Ray<real>
    operator()(const Vec2<real, Container>& ndc) const noexcept
    {
        return {pos, dir - Vec3<real>(ndc * tan_fov_half)};
    }
};
} // namespace loom
