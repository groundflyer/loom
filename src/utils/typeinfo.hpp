// -*- C++ -*-
// typeinfo.hpp
//

#pragma once

#include <fmt/core.h>

namespace lucid
{
#define MAKE_TYPEINFO_STRING(TYPE) \
    constexpr auto get_typeinfo_string(TYPE) noexcept { return #TYPE; }

MAKE_TYPEINFO_STRING(int)
MAKE_TYPEINFO_STRING(bool)
MAKE_TYPEINFO_STRING(char)
MAKE_TYPEINFO_STRING(short)
MAKE_TYPEINFO_STRING(long)
MAKE_TYPEINFO_STRING(long long)
MAKE_TYPEINFO_STRING(float)
MAKE_TYPEINFO_STRING(double)
MAKE_TYPEINFO_STRING(size_t)
MAKE_TYPEINFO_STRING(long double)
MAKE_TYPEINFO_STRING(unsigned int)
MAKE_TYPEINFO_STRING(unsigned char)
MAKE_TYPEINFO_STRING(unsigned short)

} // namespace lucid
