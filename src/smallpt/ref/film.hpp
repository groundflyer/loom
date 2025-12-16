// -*- C++ -*-
// film.hpp
//

#pragma once

#include "typedefs.hpp"

namespace loom
{
template <std::floating_point T>
constexpr T
ratio(const Vec2a<T>& res) noexcept
{
    const auto& [w, h] = res;
    return w / h;
}

// pixel index to film sample space
template <std::floating_point T>
constexpr Vec2a<T>
sample_space(const Vec2a<T>& _res, const T _ratio, const Vec2u& _idx) noexcept
{
    const auto& [c, r] = _idx;
    const auto [w, h]  = _res - static_cast<T>(1);
    const T sx = static_cast<T>(c) / w * _ratio - _ratio * static_cast<T>(0.5);
    const T sy = static_cast<T>(r) / h - static_cast<T>(0.5);
    return {sx, sy};
}

template <std::floating_point T>
constexpr Vec2a<T>
sample_space(const Vec2a<T>& _res, const Vec2u& _idx) noexcept
{
    return sample_space(_res, ratio(_res), _idx);
}

template <std::floating_point T>
constexpr Vec2u
pixel_index(const Vec2a<T>& _res,
            const T         _ratio,
            const Vec2a<T>& sample_pos) noexcept
{
    const auto& [sx, sy] = sample_pos;
    const auto [w, h]    = _res - static_cast<T>(1);
    // normalize sample coordinates
    const T nsx = (sx + _ratio * static_cast<T>(0.5)) / _ratio;
    const T nsy = sy + static_cast<T>(0.5);
    return Vec2u{static_cast<unsigned>(std::round(nsx * w)),
                 static_cast<unsigned>(std::round(nsy * h))};
}

template <std::floating_point T>
constexpr Vec2u
pixel_index(const Vec2a<T>& _res, const Vec2a<T>& sample_pos) noexcept
{
    return pixel_index(_res, ratio(_res), sample_pos);
}

template <std::floating_point T>
constexpr T
pixel_width(const Vec2a<T>& _res) noexcept
{
    const T w = _res.template get<0>();
    return static_cast<T>(1) / w;
}

template <typename Image, std::floating_point T>
struct Film
{
    Image    img;
    Vec2a<T> res;
    T        _ratio;
    T        _pixel_width;
    // radius of pixel bounding circle
    T _pixel_radius;

    explicit Film(const Vec2u& res_u) noexcept :
        img(res_u), res(res_u), _ratio(ratio(res)),
        _pixel_width(pixel_width(res)),
        _pixel_radius(
            std::sqrt(static_cast<T>(2) * _pixel_width * _pixel_width))
    {
    }

    Vec2a<T>
    sample_space(const Vec2u& _pidx) const noexcept
    {
        CHECK_INDEX(get_x(_pidx), get_x(img.res()));
        CHECK_INDEX(get_y(_pidx), get_y(img.res()));
        return loom::sample_space(res, _ratio, _pidx);
    }

    Vec2u
    pixel_index(const Vec2a<T>& pos) const noexcept
    {
        // we require posx to be in range [-ratio/2, ratio/2]
        // and posy in range [-0.5, 0.5]
        ASSERT(inclusive_range(-_ratio * static_cast<T>(0.5), _ratio * static_cast<T>(0.5))(get_x(pos)),
               "Invalid X sample position");
        ASSERT(inclusive_range(static_cast<T>(-0.5), static_cast<T>(0.5))(get_y(pos)),
               "Invalid Y sample position");

        return loom::pixel_index(res, _ratio, pos);
    }
};
} // namespace loom
