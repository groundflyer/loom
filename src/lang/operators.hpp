// -*- C++ -*-
// operators.hpp
//

#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

namespace lucid::ops
{
struct choice
{
    template <typename C, typename T>
    constexpr T
    operator()(const C c, const T a, const T b) const noexcept
    {
        return c ? a : b;
    }
};

template <std::integral T>
struct lshift
{
    constexpr T
    operator()(const T v, const T s) const noexcept
    {
        return v << s;
    }
};

template <std::integral T>
struct rshift
{
    constexpr T
    operator()(const T v, const T s) const noexcept
    {
        return v >> s;
    }
};

template <typename T>
struct abs
{
    T
    operator()(const T v) const noexcept
    {
        return std::abs(v);
    }
};

#define MK_STD_BINOP(NAME)                              \
    template <typename T>                               \
    struct NAME                                         \
    {                                                   \
        T                                               \
        operator()(const T a, const T b) const noexcept \
        {                                               \
            return std::NAME(a, b);                     \
        }                                               \
    };

#define MK_STD_UNOP_FLOAT(NAME)              \
    template <std::floating_point T>         \
    struct NAME                              \
    {                                        \
        T                                    \
        operator()(const T v) const noexcept \
        {                                    \
            return std::NAME(v);             \
        }                                    \
    };

MK_STD_BINOP(max)
MK_STD_BINOP(min)

MK_STD_UNOP_FLOAT(round)
MK_STD_UNOP_FLOAT(ceil)
MK_STD_UNOP_FLOAT(floor)
MK_STD_UNOP_FLOAT(trunc)
MK_STD_UNOP_FLOAT(copysign)
MK_STD_UNOP_FLOAT(sin)
MK_STD_UNOP_FLOAT(cos)

#undef MK_STD_BINOP
#undef MK_STD_UNOP_FLOAT
} // namespace lucid::ops
