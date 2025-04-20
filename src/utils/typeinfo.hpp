// -*- C++ -*-
// typeinfo.hpp
//

#pragma once

#include <cstdint>

namespace lucid
{
template <typename T>
struct TypeInfo;

#define MAKE_TYPEINFO(TYPE)                    \
    template <>                                \
    struct TypeInfo<TYPE>                      \
    {                                          \
        static constexpr char chars[] = #TYPE; \
    };

MAKE_TYPEINFO(uint8_t)
MAKE_TYPEINFO(uint16_t)
MAKE_TYPEINFO(uint32_t)
MAKE_TYPEINFO(uint64_t)
MAKE_TYPEINFO(int8_t)
MAKE_TYPEINFO(int16_t)
MAKE_TYPEINFO(int32_t)
MAKE_TYPEINFO(int64_t)
MAKE_TYPEINFO(bool)
MAKE_TYPEINFO(float)
MAKE_TYPEINFO(double)
MAKE_TYPEINFO(long double)

#define MAKE_TYPEINFO_STRING(TYPE) \
    constexpr auto get_typeinfo_string(TYPE) noexcept { return #TYPE; }

MAKE_TYPEINFO_STRING(bool)
MAKE_TYPEINFO_STRING(int)
MAKE_TYPEINFO_STRING(char)
MAKE_TYPEINFO_STRING(short)
MAKE_TYPEINFO_STRING(long)
MAKE_TYPEINFO_STRING(long long)
MAKE_TYPEINFO_STRING(float)
MAKE_TYPEINFO_STRING(double)
MAKE_TYPEINFO_STRING(long double)
MAKE_TYPEINFO_STRING(unsigned long long)
MAKE_TYPEINFO_STRING(unsigned int)
MAKE_TYPEINFO_STRING(unsigned char)
MAKE_TYPEINFO_STRING(unsigned short)

} // namespace lucid
