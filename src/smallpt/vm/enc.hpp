// -*- C++ -*-
// enc.hpp
//

#pragma once

#include "../common/typedefs.hpp"

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
} // namespace fn

MK_NIEBLOID(encode_pp);
MK_NIEBLOID(decode_pp);
} // namespace loom
