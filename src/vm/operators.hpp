// -*- C++ -*-
/// @file operators.hpp
/// @brief Extra operations and their default implementations

#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

namespace loom::vm::ops
{
/// @brief Conditional move.
///
/// Implements ternary operator @code c ? a : b @endcode
struct cmov
{
    /// @param c condition.
    /// @return @p a if @p c otherwise @p b
    template <std::integral C, typename T>
    constexpr T
    operator()(const C c, const T a, const T b) const noexcept
    {
        return c ? a : b;
    }
};

/// @brief Left bitwise shift.
///
/// Implements left shift operator <<
template <std::integral T>
struct lshift
{
    /// @param v value.
    /// @param s number of bits to shift by.
    constexpr T
    operator()(const T v, const T s) const noexcept
    {
        return v << s;
    }
};

/// @brief Right bitwise shift.
///
/// Implements right shift operator
template <std::integral T>
struct rshift
{
    /// @param v value.
    /// @param s number of bits to shift by.
    constexpr T
    operator()(const T v, const T s) const noexcept
    {
        return v >> s;
    }
};

/// @brief Absolute value.
template <typename T>
struct abs
{
    T
    operator()(const T v) const noexcept
    {
        return std::abs(v);
    }
};

/// @brief Fused multiply-add.
///
/// Implements x * y + z.
struct fma
{
    template <std::floating_point T>
    T
    operator()(const T x, const T y, const T z) const noexcept
    {
        return std::fma(x, y, z);
    }
};

/// @brief Macro template for binary operators available in STL.
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

/// @brief Macro template for unary floating-point operators available in STL.
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
} // namespace loom::vm::ops
