// -*- C++ -*-
// enc.hpp
//
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


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
} // namespace loom
