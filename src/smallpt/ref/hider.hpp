// -*- C++ -*-
// hider.hpp
//

#pragma once

#include "niebloids.hpp"
#include "ray.hpp"

namespace loom
{
namespace detail
{
template <unsigned idx, std::floating_point T, typename Prim, typename... Rest>
std::pair<unsigned, Intersection<T>>
hider_impl(const std::pair<unsigned, Intersection<T>>& prev,
           const Ray<T>&                               ray,
           const Prim&                                 prim,
           const Rest&... rest) noexcept
{
    const Intersection<T> prev_isect    = prev.second;
    const Intersection<T> current_isect = intersect(ray, prim);
    const std::pair<unsigned, Intersection<T>>  current{idx, current_isect};
    const std::pair<unsigned, Intersection<T>>& closest =
        prev_isect ? (prev_isect.t < current_isect.t ? prev : current) :
                     current;

    if constexpr(sizeof...(Rest) > 0)
        return hider_impl<idx + 1>(ray, closest, rest...);
    else
        return closest;
}
} // namespace detail

namespace fn
{
template <std::floating_point T, typename... Prims>
std::pair<unsigned, Intersection<T>>
hider(const Ray<T>& ray, const Prims&... prims) noexcept
{
    return ::loom::detail::hider_impl<0>(std::pair{-1u, Intersection<T>{}},
                                         ray,
                                         prims...);
}
} // namespace fn

MK_NIEBLOID(hider)
} // namespace loom
