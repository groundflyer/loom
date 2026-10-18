// -*- C++ -*-
/// @file operators.hpp
/// @brief Additional computation operations and their default implementations.
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

///
/// Operators described here are used in vm::Compute.
///
/// For basic operations reusing STL:
///
/// Arithmetic operations:
/// * std::plus
/// * std::minus
/// * std::multiplies
/// * std::divides
/// * std::modulus
/// * std::negate
///
/// Comparison operations:
/// * std::equal_to
/// * std::not_equal_to
/// * std::greater
/// * std::less
/// * std::greater_equal
/// * std::less_equal
///
/// Logical operations:
/// * std::ligical_and
/// * std::logical_or
/// * std::logical_not
///
/// Bitwise operations
/// * std::bit_and
/// * std::bit_or
/// * std::bit_xor
/// * std::bit_not
///
/// Undocumented operators generated with preprocessor:
///
/// Binary operations:
/// * loom::vm::ops::min
/// * loom::vm::ops::max
///
/// Floating point unary operations:
/// * loom::vm::ops::round
/// * loom::vm::ops::ceil
/// * loom::vm::ops::floor
/// * loom::vm::ops::trunc
/// * loom::vm::ops::sin
/// * loom::vm::ops::cos
/// * loom::vm::ops::sqrt
///
/// Floating point binary operations:
/// * loom::vm::ops::copysign

#pragma once

#include <common/string.hpp>
#include <common/tmp.hpp>
#include <common/tuple.hpp>

#include <algorithm>
#include <array>
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
        return loom::fma(x, y, z);
    }
};

/// @brief Fused multiply-subtract.
///
/// Implements x * y - z.
struct fms
{
    template <std::floating_point T>
    T
    operator()(const T x, const T y, const T z) const noexcept
    {
        return loom::fma(x, y, -z);
    }
};

/// @brief Reciprocal square root @f$f(x) = \frac{1}{\sqrt{x}}@f$.
template <std::floating_point T>
struct rsqrt
{
    T
    operator()(const T x) const noexcept
    {
        return static_cast<T>(1) / std::sqrt(x);
    }
};

/// @brief Select constant.
/// @tparam T value type.
/// @tparam List list of constants.
///
/// This special instruction selects a constant from @p L using index value.
///
/// When size of @p List is two just use cmov.
template <typename T, std::integral I, typelist::List L>
    requires(typelist::length<L> >= 3)
struct sct
{
    static constexpr std::array<T, typelist::length<L>> values = tuple2array(
        typelist::maybe_collect(L{},
                                []<CharSequence S>() constexpr noexcept -> T
                                {
                                    constexpr std::optional<T> pr =
                                        s2n<T>(cs2sv<S>);
                                    static_assert(pr.has_value());
                                    return pr.value();
                                }));

    T
    operator()(const I i) const noexcept
    {
        return values[i];
    }
};

/// @brief Select value using index. Generalization of ops::cmov.
/// @tparam T value type.
/// @tparam I index type.
///
/// Unlike ops::sct works with registers.
template <typename T, std::integral I>
struct slt
{
    template <std::same_as<T>... Ts>
        requires(sizeof...(Ts) >= 2)
    T
    operator()(const I i, const Ts... args) const noexcept
    {
        return std::array<T, sizeof...(Ts)>{args...}[i];
    }
};

/// @brief Predicate accumulation.
/// @tparam T type of comparing operands.
/// @tparam I type of accumulating boolean values.
/// @tparam Acc boolean accumulator. Possible values:
/// * std::logical_and
/// * std::logical_or
/// @tparam Cmp comparison operator. Possible values:
/// * std::equal_to
/// * std::not_equal_to
/// * std::greater
/// * std::less
/// * std::greater_equal
/// * std::less_equal
///
/// Performs comparison @f$\circ@f$ and accumulates
/// the result with boolean operator @f$\star@f$:
///
/// @f$p \star (x \circ y)@f$
template <typename T,
          std::integral I,
          template <typename> typename Acc,
          template <typename> typename Cmp>
class pacc
{
    static constexpr Acc<I> acc{};
    static constexpr Cmp<T> cmp{};

  public:
    I
    operator()(const I p, const T x, const T y) const noexcept
    {
        return acc(p, cmp(x, y));
    }
};

struct assert_fp
{
    template <std::floating_point T>
    T
    operator()(T v) const noexcept
    {
        if(std::isnan(v))
        {
            std::fprintf(stderr, "isnan\n");
            std::abort();
        }

        if(!std::isfinite(v))
        {
            std::fprintf(stderr, "infinity\n");
            std::abort();
        }

        return v;
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

/// @brief Macro template for unary floating-point operators available in STL.
#define MK_STD_BINOP_FLOAT(NAME)                        \
    template <std::floating_point T>                    \
    struct NAME                                         \
    {                                                   \
        T                                               \
        operator()(const T a, const T b) const noexcept \
        {                                               \
            return std::NAME(a, b);                     \
        }                                               \
    };

MK_STD_BINOP(max)
MK_STD_BINOP(min)

MK_STD_UNOP_FLOAT(round)
MK_STD_UNOP_FLOAT(ceil)
MK_STD_UNOP_FLOAT(floor)
MK_STD_UNOP_FLOAT(trunc)
MK_STD_UNOP_FLOAT(sin)
MK_STD_UNOP_FLOAT(cos)
MK_STD_UNOP_FLOAT(sqrt)

MK_STD_BINOP_FLOAT(copysign)

#undef MK_STD_BINOP
#undef MK_STD_UNOP_FLOAT
#undef MK_STD_BINOP_FLOAT
} // namespace loom::vm::ops
