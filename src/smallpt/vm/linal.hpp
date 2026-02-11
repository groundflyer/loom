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

template <typename T, CharSequence P, std::uint8_t I>
using var_idx = Variable<T, append_idx_t<P, I>>;

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
} // namespace detail

#define MAKE_STD_BINOP_VEC3_MACRO(NAME, OP)                                  \
    using __macro_decl_vec3_##NAME =                                         \
        macro_decl<CS(vec3_##NAME), CS(dst[3]), CS(a[3]), CS(b[3])>;         \
    template <typename T, typename DST, typename A, typename B>              \
    struct vec3_##NAME :                                                     \
        std::type_identity<                                                  \
            typename detail::make_vec_bin_map<std::OP<T>, T, DST, A, B, 3>:: \
                type>                                                        \
    {                                                                        \
    };

MAKE_STD_BINOP_VEC3_MACRO(add, plus)
MAKE_STD_BINOP_VEC3_MACRO(sub, minus)
MAKE_STD_BINOP_VEC3_MACRO(mul, multiplies)
MAKE_STD_BINOP_VEC3_MACRO(div, divides)

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
} // namespace loom::vm
