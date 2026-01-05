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
    Vec2<real> tan_fov_half{1};
    Mat4<real> transform{m4id<real>};

    template <template <typename, std::size_t> typename Container>
    constexpr Perspective(const real                   cfovy,
                          const Mat4<real, Container>& _transform) noexcept :
        tan_fov_half(cfovy), transform(_transform)
    {
    }

    template <template <typename, std::size_t> typename Container>
    constexpr Ray<real>
    operator()(const Vec2<real, Container>& ndc) const noexcept
    {
        return apply_transform(
            transform,
            Ray<real>(Vec3<real>(0),
                      normalize(Vec3<real>(ndc * tan_fov_half, 1))));
    }
};
} // namespace loom
