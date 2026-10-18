// -*- C++ -*-
/// @file typeinfo.hpp
/// @brief Simple primitive type reflection.
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

#include <cstdint>

namespace loom
{
/// @brief Has static constexpr member chars with type name.
template <typename T>
struct TypeInfo;

#define MK_NAMED_TYPEINFO(TYPE, NAME)          \
    template <>                                \
    struct TypeInfo<TYPE>                      \
    {                                          \
        static constexpr char chars[] = #NAME; \
    };

#define MK_TYPEINFO(TYPE) MK_NAMED_TYPEINFO(TYPE, TYPE)

MK_NAMED_TYPEINFO(uint8_t, u8)
MK_NAMED_TYPEINFO(uint16_t, u16)
MK_NAMED_TYPEINFO(uint32_t, u32)
MK_NAMED_TYPEINFO(uint64_t, u64)
MK_NAMED_TYPEINFO(int8_t, i8)
MK_NAMED_TYPEINFO(int16_t, i16)
MK_NAMED_TYPEINFO(int32_t, i32)
MK_NAMED_TYPEINFO(int64_t, i64)
MK_NAMED_TYPEINFO(float, f32)
MK_NAMED_TYPEINFO(double, f64)
MK_NAMED_TYPEINFO(long double, f80)
MK_TYPEINFO(bool)
MK_TYPEINFO(long long)

#define MK_TYPEINFO_STRING(TYPE) \
    constexpr auto get_typeinfo_string(TYPE) noexcept { return #TYPE; }

MK_TYPEINFO_STRING(bool)
MK_TYPEINFO_STRING(int)
MK_TYPEINFO_STRING(char)
MK_TYPEINFO_STRING(short)
MK_TYPEINFO_STRING(long)
MK_TYPEINFO_STRING(long long)
MK_TYPEINFO_STRING(float)
MK_TYPEINFO_STRING(double)
MK_TYPEINFO_STRING(long double)
MK_TYPEINFO_STRING(unsigned long long)
MK_TYPEINFO_STRING(unsigned int)
MK_TYPEINFO_STRING(unsigned char)
MK_TYPEINFO_STRING(unsigned short)

#undef MK_NAMED_TYPEINFO
#undef MK_TYPEINFO
#undef MK_TYPEINFO_STRING
} // namespace loom
