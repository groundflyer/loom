// -*- C++ -*-
// rng.hpp
//

#pragma once

#include <array>
#include <cstdint>

#include <utils/functional.hpp>

namespace loom
{
namespace fn
{
std::uint32_t
rotl32(std::uint32_t x, std::uint32_t N) noexcept
{
    return (x << (N & 31)) | (x >> ((32 - N) & 31));
}

#define THREEFRY_ROUND(R) \
    c[0] += c[1];         \
    c[1] = rotl32(c[1], R) ^ c[0]

#define THREEFRY_KEY_INJECTION(C, I1, I2) \
    c[0] += ks[I1];                       \
    c[1] += ks[I2] + C

#define THREEFRY_8ROUNDS(C1, C2, I1, I2, I3, I4) \
    THREEFRY_ROUND(13);                          \
    THREEFRY_ROUND(15);                          \
    THREEFRY_ROUND(26);                          \
    THREEFRY_ROUND(6);                           \
    THREEFRY_KEY_INJECTION(C1, I1, I2);          \
    THREEFRY_ROUND(17);                          \
    THREEFRY_ROUND(29);                          \
    THREEFRY_ROUND(16);                          \
    THREEFRY_ROUND(24);                          \
    THREEFRY_KEY_INJECTION(C2, I3, I4)

using ar_32_2 = std::array<std::uint32_t, 2>;

ar_32_2
threefry2x32r32(ar_32_2 c, const ar_32_2& k) noexcept
{
    std::uint32_t ks[3] = {k[0], k[1], 0x1bd11bda ^ k[0] ^ k[1]};
    c[0] += ks[0];
    c[1] += ks[1];

    THREEFRY_8ROUNDS(1, 2, 1, 2, 2, 0);
    THREEFRY_8ROUNDS(3, 4, 0, 1, 1, 2);
    THREEFRY_8ROUNDS(5, 6, 2, 0, 0, 1);
    THREEFRY_8ROUNDS(7, 8, 1, 2, 2, 0);

    return c;
}
#undef THREEFRY_ROUND
#undef THREEFRY_8ROUNDS
#undef THREEFRY_KEY_INJECTION

float
gen_canonical(std::uint32_t x) noexcept
{
    std::uint32_t bits = (x >> 9) | 0x3f800000;
    return reinterpret_cast<float&>(bits) - 1.f;
}

ar_32_2
bump_ctr(const ar_32_2& c, std::uint32_t i = 1) noexcept
{
    ar_32_2 ret = c;
    ret[1] += i;
    if(ret[1] < c[1]) ret[0] += i;
    if(ret[0] < c[0]) ret[1] += i;
    return ret;
}
} // namespace fn

MK_NIEBLOID(rotl32)
MK_NIEBLOID(threefry2x32r32)
MK_NIEBLOID(gen_canonical)
MK_NIEBLOID(bump_ctr)
} // namespace loom
