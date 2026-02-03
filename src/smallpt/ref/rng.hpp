// -*- C++ -*-
/// @file rng.hpp
/// @brief Reference implemenation of PRNG.
///
/// Contains implementation of Threefry-2x32 @cite 10.1145/2063384.2063405

#pragma once

#include <common/debug.hpp>
#include <common/functional.hpp>

#include <array>
#include <cstdint>

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

#define THREEFRY_KEY_INJECTION(C, KS1, KS2) \
    c[0] += KS1;                            \
    c[1] += KS2 + C

#define THREEFRY_8ROUNDS(C1, C2, KS1, KS2, KS3, KS4) \
    THREEFRY_ROUND(13);                              \
    THREEFRY_ROUND(15);                              \
    THREEFRY_ROUND(26);                              \
    THREEFRY_ROUND(6);                               \
    THREEFRY_KEY_INJECTION(C1, KS1, KS2);            \
    THREEFRY_ROUND(17);                              \
    THREEFRY_ROUND(29);                              \
    THREEFRY_ROUND(16);                              \
    THREEFRY_ROUND(24);                              \
    THREEFRY_KEY_INJECTION(C2, KS3, KS4)

using ar_32_2 = std::array<std::uint32_t, 2>;

/// @brief Reference Threefry-2x32 implementation with 32 rounds.
/// @tparam k0 first key number.
/// @tparam k1 second key number.
template <std::uint32_t k0, std::uint32_t k1>
struct threefry2x32r32
{
    static constexpr std::uint32_t k2 = 0x1bd11bda ^ k0 ^ k1;

    ar_32_2
    operator()(ar_32_2 c) const noexcept
    {
        c[0] += k0;
        c[1] += k1;

        THREEFRY_8ROUNDS(1, 2, k1, k2, k2, k0);
        THREEFRY_8ROUNDS(3, 4, k0, k1, k1, k2);
        THREEFRY_8ROUNDS(5, 6, k2, k0, k0, k1);
        THREEFRY_8ROUNDS(7, 8, k1, k2, k2, k0);

        return c;
    }
};
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
    ret[0] += i;
    if(ret[0] < c[0]) ret[1] += i;
    if(ret[1] < c[1]) ret[0] += i;
    return ret;
}
} // namespace fn

template <std::uint32_t k0, std::uint32_t k1>
inline constexpr fn::threefry2x32r32<k0, k1> threefry2x32r32{};

MK_NIEBLOID(rotl32)
MK_NIEBLOID(gen_canonical)
MK_NIEBLOID(bump_ctr)
} // namespace loom
