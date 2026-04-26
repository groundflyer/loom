// -*- C++ -*-
// utils.hpp
//

#pragma once

#include "../common/typedefs.hpp"

#include <common/functional.hpp>
#include <vm/simd.hpp>

namespace loom
{
namespace fn
{
static std::uint32_t
encode_pp(const Vec2u& p) noexcept
{
    const auto [x, y] = p;
    return x << 16 | y;
} // namespace std::uint32_t

static Vec2u
decode_pp(const std::uint32_t p) noexcept
{
    return Vec2u(p >> 16, p & 0xffff);
}

static std::uint32_t
encode_res(const Vec2u& res) noexcept
{
    const auto [w, h] = res;
    return w << 16 | h;
}
} // namespace fn

MK_NIEBLOID(encode_pp);
MK_NIEBLOID(decode_pp);
MK_NIEBLOID(encode_res);

static int _FORCE_INLINE
masked_ray_count(const vm::simd::native_vector<std::uint32_t> vrc,
                 const std::size_t tail_size) noexcept
{
#if defined(__AVX512F__)
    return _mm512_mask_reduce_add_epi32(static_cast<std::uint16_t>(-1)
                                            << tail_size,
                                        vrc);
#else // AVX or AVX2
    static constexpr std::size_t   N = simd::native_vector_extent_v<float>;
    static constexpr std::uint32_t l = static_cast<std::uint32_t>(-1);
    static constexpr vm::simd::native_vector<std::uint32_t> masks[8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {l, 0, 0, 0, 0, 0, 0, 0},
        {l, l, 0, 0, 0, 0, 0, 0},
        {l, l, l, 0, 0, 0, 0, 0},
        {l, l, l, l, 0, 0, 0, 0},
        {l, l, l, l, l, 0, 0, 0},
        {l, l, l, l, l, l, l, 0},
        {l, l, l, l, l, l, l, l}
    };

    return __builtin_reduce_add(vrc & masks[N - tail_size]);
#endif
}

template <typename K>
class OutputPixelConverter
{
    std::size_t& r_rc;

  public:
    OutputPixelConverter(std::size_t& rc) noexcept : r_rc(rc) {}

    RGBA<>
    operator()(const vm::output_type<K>& o) const noexcept
    {
        const auto [r, g, b, rc, _] = o;
        r_rc += rc;
        return RGBA{r, g, b, 1};
    }
};
} // namespace loom
