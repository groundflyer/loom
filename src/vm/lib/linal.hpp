// -*- C++ -*-
/// @file linal.hpp
/// @brief Linear aglebra port.
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
/// This module uses lists of strings to describe vectors or arrays.
/// They are typelists loom::type_sequence of strings loom::char_sequence.
/// A string can be a literal or a register identifier.

#pragma once

#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

namespace loom::vm
{
/// @brief Convert number to character.
template <std::uint8_t I>
constexpr inline char i2c = '0' + I;

/// @brief Append number to register identifier prefix.
/// @tparam P prefix.
/// @tparam I suffix.
template <CharSequence P, std::uint8_t I>
using append_idx = intlist::append<P, i2c<I>>;

/// @brief Remove last symbol.
template <CharSequence V>
using remove_idx_t = intlist::reverse<intlist::tail<intlist::reverse<V>>>;

/// @brief Make array of registers.
/// @tparam P register identifer prefix.
/// @tparam N array size.
///
/// Creates type_sequence of size @p N with registers identifiers.
template <CharSequence P, std::size_t N>
class make_reg_array
{
    template <std::uint8_t I>
    using f = append_idx<P, I>;

  public:
    using type = intlist::map_t<f, std::make_index_sequence<N>>;
};

/// @brief Helper alias for vm::make_reg_array.
template <CharSequence P, std::size_t N>
using make_reg_array_t = make_reg_array<P, N>::type;

/// @brief Make square matrix of registers.
/// @tparam P register identifer prefix.
/// @tparam N matrix order.
///
/// Creates type_sequence of size @p N of nested lists with registers
/// identifiers.
template <CharSequence P, std::size_t N>
class make_reg_mat
{
    template <typename P_I>
    using f = make_reg_array_t<P_I, N>;

  public:
    using type = typelist::map<f, make_reg_array_t<P, N>>;
};

/// @brief Helper alias for vm::make_reg_mat.
template <CharSequence P, std::size_t N>
using make_reg_mat_t = make_reg_mat<P, N>::type;

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename F>
struct maybe_make : std::type_identity<T>
{
};

template <CharSequence P,
          std::size_t  N,
          template <typename, std::size_t> typename F>
struct maybe_make<P, N, F> : std::type_identity<F<P, N>>
{
};

template <typename T, std::size_t N>
using maybe_make_reg_array_t = maybe_make<T, N, make_reg_array_t>::type;

template <typename T, std::size_t N>
using maybe_make_reg_mat_t = maybe_make<T, N, make_reg_mat_t>::type;

/// @brief Generate macro of binary map with two vectors.
/// @tparam Op operator. See available operators in vm/operators.hpp
/// @tparam T variables type.
/// @tparam Dst vector of destination registers.
/// @tparam A vector or scalar for the first operand.
/// @tparam B vector or scalar for the second operand.
///
/// At least one of @p A and @p B should be vector.
template <typename Op, typename T, typelist::List Dst, typename A, typename B>
    requires((typelist::List<A> || typelist::List<B>) &&
             !(CharSequence<A> && CharSequence<B>))
class make_bin_map
{
    template <std::uint8_t I>
    using f = Compute<Op,
                      Variable<T, typelist::at<I, Dst>>,
                      Variable<T, typelist::proj<I, A>>,
                      Variable<T, typelist::proj<I, B>>>;

  public:
    using type =
        intlist::map_t<f, std::make_index_sequence<typelist::length<Dst>>>;
};

/// @brief Generate macro for unary map.
/// @tparam Op operator. See available operators in vm/operators.hpp
/// @tparam T variables type.
/// @tparam Dst vector of destination registers.
/// @tparam Src vector of source registers.
template <typename Op, typename T, typelist::List Dst, typelist::List Src>
    requires(typelist::length<Dst> == typelist::length<Src>)
class make_un_map
{
    template <std::uint8_t I>
    using f = Compute<Op,
                      Variable<T, typelist::at<I, Dst>>,
                      Variable<T, typelist::at<I, Src>>>;

  public:
    using type =
        intlist::map_t<f, std::make_index_sequence<typelist::length<Src>>>;
};

/// @brief Generate macro of ternary map with three vectors.
/// @tparam Op operator. See available operators in vm/operators.hpp
/// @tparam T variables type.
/// @tparam Dst vector of destination registers.
/// @tparam A vector or scalar for the first operand.
/// @tparam B vector or scalar for the second operand.
/// @tparam C vector or scalar for the second operand.
///
/// At least one of @p A and @p B should be vector.
template <typename Op,
          typename T,
          typelist::List Dst,
          typename A,
          typename B,
          typename C>
    requires((typelist::List<A> || typelist::List<B>) &&
             !(CharSequence<A> && CharSequence<B>))
class make_ternary_map
{
    template <std::uint8_t I>
    using f = Compute<Op,
                      Variable<T, typelist::at<I, Dst>>,
                      Variable<T, typelist::proj<I, A>>,
                      Variable<T, typelist::proj<I, B>>,
                      Variable<T, typelist::proj<I, C>>>;

  public:
    using type =
        intlist::map_t<f, std::make_index_sequence<typelist::length<Dst>>>;
};

/// @brief Generate macro for dot product of two vectors.
/// @tparam Dst destination register identifier.
template <typename MAJOR_TYPE,
          CharSequence   Dst,
          typelist::List A,
          typelist::List B>
    requires(typelist::length<A> == typelist::length<B>)
class make_dot
{
    using dst_var = Variable<MAJOR_TYPE, Dst>;

    template <std::uint8_t I>
    using f = Compute<ops::fma,
                      dst_var,
                      Variable<MAJOR_TYPE, typelist::at<I, A>>,
                      Variable<MAJOR_TYPE, typelist::at<I, B>>,
                      dst_var>;

  public:
    using type = typelist::cons<
        TMOV_CS(Dst, CS(0)),
        intlist::map_t<f, std::make_index_sequence<typelist::length<A>>>>;
};

/// @brief Generate macro for cross product of two vectors[3].
template <typename MAJOR_TYPE,
          typelist::List Dst,
          typelist::List A,
          typelist::List B,
          CharSequence   TMP>
    requires(typelist::length<A> == 3 && typelist::length<B> == 3 &&
             typelist::length<Dst> == 3)
class make_cross
{
    template <typename R, std::uint8_t I>
    using tvar_idx = Variable<MAJOR_TYPE, typelist::at<I, R>>;

    template <std::uint8_t K, std::uint8_t I, std::uint8_t J>
    using s = type_sequence<Compute<std::multiplies<MAJOR_TYPE>,
                                    Variable<MAJOR_TYPE, TMP>,
                                    tvar_idx<A, J>,
                                    tvar_idx<B, I>>,
                            Compute<ops::fms,
                                    tvar_idx<Dst, K>,
                                    tvar_idx<A, I>,
                                    tvar_idx<B, J>,
                                    Variable<MAJOR_TYPE, TMP>>>;

  public:
    using type =
        typelist::concat<type_sequence<s<0, 1, 2>, s<1, 2, 0>, s<2, 0, 1>>>;
};

/// @brief Matrix-vector product.
///
/// @tparam Mat list of lists.
template <typename MAJOR_TYPE,
          typelist::List Dst,
          typelist::List Mat,
          typelist::List Vec>
    requires(typelist::length<Dst> == typelist::length<Vec> &&
             typelist::length<Mat> == typelist::length<Vec>)
class make_mat_vec
{
    template <typename R, std::uint8_t I>
    using tvar_idx = Variable<MAJOR_TYPE, typelist::at<I, R>>;

    template <typename R, std::uint8_t I, std::uint8_t J>
    using tvar_ij = Variable<MAJOR_TYPE, typelist::at<J, typelist::at<I, R>>>;

    template <std::uint8_t I, std::uint8_t J>
    using f = Compute<ops::fma,
                      tvar_idx<Dst, I>,
                      tvar_ij<Mat, I, J>,
                      tvar_idx<Vec, J>,
                      tvar_idx<Dst, I>>;

    template <std::uint8_t I>
    using init_dst = Compute<std::identity, tvar_idx<Dst, I>, TVAR(0)>;

    using idxs = std::make_index_sequence<typelist::length<Vec>>;

    template <std::uint8_t I>
    using make_ij = type_pair<std::integral_constant<std::uint8_t, I>, idxs>;

    template <std::uint8_t I>
    class fi
    {
        template <std::uint8_t J>
        using fij = f<I, J>;

      public:
        using type = intlist::map_t<fij, idxs>;
    };

    template <std::uint8_t I>
    using fi_t = fi<I>::type;

  public:
    using type = typelist::join<intlist::map_t<init_dst, idxs>,
                                typelist::concat<intlist::map_t<fi_t, idxs>>>;
};

namespace detail
{
template <std::uint8_t I>
using sq_br = char_sequence<'[', i2c<I>, ']'>;
}

#define ARR_PARM_DECL(NAME, N) CS_JOIN(CS(NAME), detail::sq_br<N>)
#define MAT_PARM_DECL(NAME, N) \
    CS_JOIN(CS_JOIN(CS(NAME), detail::sq_br<N>), detail::sq_br<N>)

#define MAKE_STD_BINOP_VEC_VEC_MACRO(N, NAME, OP)                      \
    using __macro_decl_vec_vec##N##_##NAME =                           \
        macro_decl<CS(vec_vec##N##_##NAME),                            \
                   ARR_PARM_DECL(dst, N),                              \
                   ARR_PARM_DECL(a, N),                                \
                   ARR_PARM_DECL(b, N)>;                               \
    template <typename T, typename DST, typename A, typename B>        \
    struct vec_vec##N##_##NAME :                                       \
        std::type_identity<                                            \
            typename make_bin_map<std::OP<T>,                          \
                                  T,                                   \
                                  maybe_make_reg_array_t<DST, N>,      \
                                  maybe_make_reg_array_t<A, N>,        \
                                  maybe_make_reg_array_t<B, N>>::type> \
    {                                                                  \
    };

#define MAKE_STD_BINOP_VEC_SCALAR_MACRO(N, NAME, OP)              \
    using __macro_decl_vec##N##_scalar_##NAME =                   \
        macro_decl<CS(vec##N##_scalar_##NAME),                    \
                   ARR_PARM_DECL(dst, N),                         \
                   ARR_PARM_DECL(a, N),                           \
                   CS(b)>;                                        \
    template <typename T, typename DST, typename A, typename B>   \
    struct vec##N##_scalar_##NAME :                               \
        std::type_identity<                                       \
            typename make_bin_map<std::OP<T>,                     \
                                  T,                              \
                                  maybe_make_reg_array_t<DST, N>, \
                                  maybe_make_reg_array_t<A, N>,   \
                                  B>::type>                       \
    {                                                             \
    };

#define MAKE_UNOP_VEC_MACRO(N, OP, NS)                                   \
    using __macro_decl_vec##N##_##OP = macro_decl<CS(vec##N##_##OP),     \
                                                  ARR_PARM_DECL(dst, N), \
                                                  ARR_PARM_DECL(a, N)>;  \
    template <typename T, typename DST, typename A>                      \
    struct vec##N##_##OP :                                               \
        std::type_identity<                                              \
            typename make_un_map<NS::OP<T>,                              \
                                 T,                                      \
                                 maybe_make_reg_array_t<DST, N>,         \
                                 maybe_make_reg_array_t<A, N>>::type>    \
    {                                                                    \
    };

#define MAKE_TERNARY_OP_VEC_VEC_MACRO(N, NAME, OP)                          \
    using __macro_decl_vec_vec##N##_##NAME =                                \
        macro_decl<CS(vec_vec##N##_##NAME),                                 \
                   ARR_PARM_DECL(dst, N),                                   \
                   ARR_PARM_DECL(a, N),                                     \
                   ARR_PARM_DECL(b, N),                                     \
                   ARR_PARM_DECL(c, N)>;                                    \
    template <typename T, typename DST, typename A, typename B, typename C> \
    struct vec_vec##N##_##NAME :                                            \
        std::type_identity<                                                 \
            typename make_ternary_map<ops::OP,                              \
                                      T,                                    \
                                      maybe_make_reg_array_t<DST, N>,       \
                                      maybe_make_reg_array_t<A, N>,         \
                                      maybe_make_reg_array_t<B, N>,         \
                                      maybe_make_reg_array_t<C, N>>::type>  \
    {                                                                       \
    };

#define MAKE_MAT_VEC_MACRO(N)                                            \
    using __macro_decl_mat_vec##N = macro_decl<CS(mat_vec##N),           \
                                               ARR_PARM_DECL(dst, N),    \
                                               MAT_PARM_DECL(mat, N),    \
                                               ARR_PARM_DECL(vec, N)>;   \
    template <typename T, typename DST, typename MAT, typename VEC>      \
    struct mat_vec##N :                                                  \
        std::type_identity<                                              \
            typename make_mat_vec<T,                                     \
                                  maybe_make_reg_array_t<DST, N>,        \
                                  maybe_make_reg_mat_t<MAT, N>,          \
                                  maybe_make_reg_array_t<VEC, N>>::type> \
    {                                                                    \
    };

MAKE_STD_BINOP_VEC_VEC_MACRO(2, add, plus)
MAKE_STD_BINOP_VEC_VEC_MACRO(2, sub, minus)
MAKE_STD_BINOP_VEC_VEC_MACRO(2, mul, multiplies)
MAKE_STD_BINOP_VEC_VEC_MACRO(2, div, divides)

MAKE_STD_BINOP_VEC_VEC_MACRO(3, add, plus)
MAKE_STD_BINOP_VEC_VEC_MACRO(3, sub, minus)
MAKE_STD_BINOP_VEC_VEC_MACRO(3, mul, multiplies)
MAKE_STD_BINOP_VEC_VEC_MACRO(3, div, divides)

MAKE_STD_BINOP_VEC_SCALAR_MACRO(2, add, plus)
MAKE_STD_BINOP_VEC_SCALAR_MACRO(2, sub, minus)
MAKE_STD_BINOP_VEC_SCALAR_MACRO(2, mul, multiplies)
MAKE_STD_BINOP_VEC_SCALAR_MACRO(2, div, divides)

MAKE_STD_BINOP_VEC_SCALAR_MACRO(3, add, plus)
MAKE_STD_BINOP_VEC_SCALAR_MACRO(3, sub, minus)
MAKE_STD_BINOP_VEC_SCALAR_MACRO(3, mul, multiplies)
MAKE_STD_BINOP_VEC_SCALAR_MACRO(3, div, divides)

MAKE_UNOP_VEC_MACRO(3, abs, ops)
MAKE_UNOP_VEC_MACRO(3, negate, std)

MAKE_TERNARY_OP_VEC_VEC_MACRO(3, fma, fma)
MAKE_TERNARY_OP_VEC_VEC_MACRO(3, fms, fms)

MAKE_MAT_VEC_MACRO(2)
MAKE_MAT_VEC_MACRO(3)

#undef MAKE_STD_BINOP_VEC_VEC_MACRO
#undef MAKE_STD_BINOP_VEC_SCALAR_MACRO
#undef MAKE_UNOP_VEC_MACRO
#undef MAKE_MAT_VEC_MACRO

using __macro_decl_vec3_dot =
    macro_decl<CS(vec3_dot), CS(dst), CS(a[3]), CS(b[3])>;

/// @brief Dot product port for 3D vectors.
/// @param[out] DST destination
/// @param[in] A vec[3] a
/// @param[in] B vec[3] b
template <typename T, CharSequence DST, typename A, typename B>
struct vec3_dot :
    std::type_identity<typename make_dot<T,
                                         DST,
                                         maybe_make_reg_array_t<A, 3>,
                                         maybe_make_reg_array_t<B, 3>>::type>
{
};

using __macro_decl_vec3_cross =
    macro_decl<CS(vec3_cross), CS(dst[3]), CS(a[3]), CS(b[3]), CS(tmp)>;

/// @brief Cross product port for 3D vectors.
/// @param[out] DST vec[3] destination
/// @param[in] A vec[3] a
/// @param[in] B vec[3] b
/// @param tmp storage for temporary values
template <typename T, typename DST, typename A, typename B, CharSequence Tmp>
struct vec3_cross :
    std::type_identity<typename make_cross<T,
                                           maybe_make_reg_array_t<DST, 3>,
                                           maybe_make_reg_array_t<A, 3>,
                                           maybe_make_reg_array_t<B, 3>,
                                           Tmp>::type>
{
};

// clang-format off
TMACRO2(vec3_length2, dst, v)
    INSERT_TMACRO(vec3_dot, dst, v, v)
MACRO_END;

TMACRO2(vec3_length, dst, v)
    INSERT_TMACRO(vec3_length2, dst, v),
    TSQRT_CS(dst, dst)
MACRO_END;

TMACRO2(vec3_norm, v, l)
    INSERT_TMACRO(vec3_length2, l, v),
    TRSQRT_CS(l, l),
    INSERT_TMACRO(vec3_scalar_mul, v, v, l)
MACRO_END;

// a invalidated
TMACRO3(vec3_dist, dst, a, b)
    INSERT_TMACRO(vec_vec3_sub, a, a, b),
    INSERT_TMACRO(vec3_length, dst, a)
MACRO_END;

#define CS_UNIDX(X) remove_idx_t<X>
}
