// -*- C++ -*-
// primitives.hpp
//

#pragma once

#include "aabb.hpp"
#include "disk.hpp"
#include "generic.hpp"
#include "quad.hpp"
#include "sphere.hpp"
#include "triangle.hpp"

namespace lucid
{
MK_NIEBLOID(intersect)
MK_NIEBLOID(normal)
MK_NIEBLOID(sample)
MK_NIEBLOID(centroid)
MK_NIEBLOID(bound)

namespace fn
{
template <typename Intersections>
constexpr std::pair<std::size_t, Intersection>
closest(const Intersections&) noexcept;

namespace detail
{
static constexpr Intersection miss{};

template <template <typename, std::size_t> typename Container,
          typename PrimsTuple,
          std::size_t... Ids>
constexpr std::pair<std::size_t, Intersection>
hider_impl(const Ray_<Container>& ray,
           const PrimsTuple&      prims,
           std::index_sequence<Ids...>) noexcept
{
    return closest(std::tuple{intersect(ray, std::get<Ids>(prims))...});
}
} // namespace detail

template <typename Intersections>
constexpr std::pair<std::size_t, Intersection>
closest(const Intersections& isects) noexcept
{
    return fold_tuple(
        enumerate(isects),
        [](const auto& a, const auto& b) noexcept
        { return a.second.t < b.second.t ? a : b; },
        std::pair{0ul, detail::miss});
}

template <template <typename, std::size_t> typename Container,
          typename PrimsTuple>
constexpr std::pair<std::size_t, Intersection>
hider(const Ray_<Container>& ray, const PrimsTuple& prims) noexcept
{
    return detail::hider_impl(
        ray,
        prims,
        std::make_index_sequence<std::tuple_size_v<PrimsTuple>>{});
}

template <template <typename, std::size_t> typename RayContainer,
          template <typename, std::size_t> typename IsectContainer>
constexpr Vec3
hit_pos(const Ray_<RayContainer>&            ray,
        const Intersection_<IsectContainer>& isect) noexcept
{
    const auto& [o, d] = ray;
    return o + d * isect.t;
}
} // namespace fn

MK_NIEBLOID(closest)
MK_NIEBLOID(hider)
MK_NIEBLOID(hit_pos)
} // namespace lucid
