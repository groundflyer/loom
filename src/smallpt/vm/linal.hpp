// -*- C++ -*-
// linal.hpp
//

#pragma once

#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

namespace loom::vm
{
namespace detail
{
template <std::uint8_t I>
struct i2c
{
    static constexpr char value = '0' + I;
};

template <CharSequence P, std::uint8_t I>
struct append_idx
{
    using type = intlist::append<P, i2c<I>::value>;
};

template <CharSequence P, std::uint8_t I>
using append_idx_t = append_idx<P, I>::type;

template <typename P, std::uint8_t I, std::uint8_t J>
using append_ij_t = append_idx_t<append_idx_t<P, I>, J>;

template <typename T, CharSequence P, std::uint8_t I>
using var_idx = Variable<T, append_idx_t<P, I>>;

template <typename T, CharSequence P, std::uint8_t I, std::uint8_t J>
using var_ij = Variable<T, append_ij_t<P, I, J>>;

template <typename Op,
          typename T,
          CharSequence Dst,
          CharSequence A,
          CharSequence B,
          std::size_t  N>
class make_vec_bin_map
{
    template <std::uint8_t I>
    using f =
        Compute<Op, var_idx<T, Dst, I>, var_idx<T, A, I>, var_idx<T, B, I>>;

  public:
    using type = intlist::map_t<f, std::make_index_sequence<N>>;
};

template <typename MAJOR_TYPE, CharSequence Dst, CharSequence A, CharSequence B>
class make_dot
{
    using dst_var = Variable<MAJOR_TYPE, Dst>;

    template <std::uint8_t I>
    using f = Compute<ops::fma,
                      dst_var,
                      var_idx<MAJOR_TYPE, A, I>,
                      var_idx<MAJOR_TYPE, B, I>,
                      dst_var>;

  public:
    using type = typelist::cons<TMOV_CS(Dst, CS(0)),
                                intlist::map_t<f, std::make_index_sequence<3>>>;
};

template <typename MAJOR_TYPE,
          CharSequence Dst,
          CharSequence A,
          CharSequence B,
          CharSequence TMP>
class make_cross
{
    template <typename R, std::uint8_t I>
    using tvar_idx = var_idx<MAJOR_TYPE, R, I>;

    template <std::uint8_t K, std::uint8_t I, std::uint8_t J>
    using s = type_sequence<Compute<std::multiplies<MAJOR_TYPE>,
                                    tvar_idx<Dst, K>,
                                    tvar_idx<A, I>,
                                    tvar_idx<B, J>>,
                            Compute<std::multiplies<MAJOR_TYPE>,
                                    Variable<MAJOR_TYPE, TMP>,
                                    tvar_idx<A, J>,
                                    tvar_idx<B, I>>,
                            Compute<std::minus<MAJOR_TYPE>,
                                    tvar_idx<Dst, K>,
                                    tvar_idx<Dst, K>,
                                    Variable<MAJOR_TYPE, TMP>>>;

  public:
    using type = type_sequence<s<0, 1, 2>, s<1, 2, 0>, s<2, 0, 1>>;
};

template <typename MAJOR_TYPE,
          CharSequence Dst,
          CharSequence Mat,
          CharSequence Vec>
class make_mat_vec
{
    template <typename R, std::uint8_t I>
    using tvar_idx = var_idx<MAJOR_TYPE, R, I>;

    template <typename R, std::uint8_t I, std::uint8_t J>
    using tvar_ij = var_ij<MAJOR_TYPE, R, I, J>;

    template <std::uint8_t I, std::uint8_t J>
    using f = Compute<ops::fma,
                      tvar_idx<Dst, I>,
                      tvar_ij<Mat, I, J>,
                      tvar_idx<Vec, J>,
                      tvar_idx<Dst, I>>;

    template <std::uint8_t I>
    using init_dst = Compute<std::identity, tvar_idx<Dst, I>, TVAR(0)>;

    using idxs = std::make_index_sequence<3>;

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
} // namespace detail

#define ARR_PARM_DECL(NAME, N) CS_JOIN(CS_JOIN(CS(NAME), CS(])), CS_JOIN(CS(N), CS(])))

#define MAKE_STD_BINOP_VEC_MACRO(N, NAME, OP)                                \
    using __macro_decl_vec##N##_##NAME = macro_decl<CS(vec##N##_##NAME),     \
                                                    ARR_PARM_DECL(dst, N),   \
                                                    ARR_PARM_DECL(a, N),     \
                                                    ARR_PARM_DECL(b, N)>;    \
    template <typename T, typename DST, typename A, typename B>              \
    struct vec##N##_##NAME :                                                 \
        std::type_identity<                                                  \
            typename detail::make_vec_bin_map<std::OP<T>, T, DST, A, B, N>:: \
                type>                                                        \
    {                                                                        \
    };

MAKE_STD_BINOP_VEC_MACRO(2, add, plus)
MAKE_STD_BINOP_VEC_MACRO(2, sub, minus)
MAKE_STD_BINOP_VEC_MACRO(2, mul, multiplies)
MAKE_STD_BINOP_VEC_MACRO(2, div, divides)

MAKE_STD_BINOP_VEC_MACRO(3, add, plus)
MAKE_STD_BINOP_VEC_MACRO(3, sub, minus)
MAKE_STD_BINOP_VEC_MACRO(3, mul, multiplies)
MAKE_STD_BINOP_VEC_MACRO(3, div, divides)

using __macro_decl_vec3_dot =
    macro_decl<CS(vec3_dot), CS(dst), CS(a[3]), CS(b[3])>;

template <typename T, CharSequence DST, CharSequence A, CharSequence B>
struct vec3_dot :
    std::type_identity<typename detail::make_dot<T, DST, A, B>::type>
{
};

using __macro_decl_vec3_cross =
    macro_decl<CS(vec3_cross), CS(dst[3]), CS(a[3]), CS(b[3]), CS(tmp)>;

template <typename T,
          CharSequence DST,
          CharSequence A,
          CharSequence B,
          CharSequence Tmp>
struct vec3_cross :
    std::type_identity<typename detail::make_cross<T, DST, A, B, Tmp>::type>
{
};

using __macro_decl_mat_vec3_dot =
    macro_decl<CS(mat_vec3_dot), CS(dst[3]), CS(mat[3][3]), CS(vec[3])>;

template <typename T, CharSequence DST, CharSequence MAT, CharSequence VEC>
struct mat_vec3_dot :
    std::type_identity<typename detail::make_mat_vec<T, DST, MAT, VEC>::type>
{
};
} // namespace loom::vm
