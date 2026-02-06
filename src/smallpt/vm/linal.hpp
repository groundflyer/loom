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
    static constexpr char value = I - 48;
};

template <CharSequence P, std::uint8_t I>
struct access_reg
{
    using type = intlist::append<P, i2c<I>::value>;
};

template <CharSequence P, std::uint8_t I>
using access_reg_t = access_reg<P, I>::type;

template <typename Op,
          typename T,
          CharSequence Dst,
          CharSequence A,
          CharSequence B,
          std::size_t  N>
class make_vec_bin_map
{
    template <std::uint8_t I>
    using f = Compute<Op,
                      Variable<T, access_reg_t<Dst, I>>,
                      Variable<T, access_reg_t<A, I>>,
                      Variable<T, access_reg_t<B, I>>>;

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
                      Variable<MAJOR_TYPE, access_reg_t<A, I>>,
                      Variable<MAJOR_TYPE, access_reg_t<B, I>>,
                      dst_var>;

  public:
    using type = typelist::cons<TMOV_CS(Dst, CS(0)),
                                intlist::map_t<f, std::make_index_sequence<3>>>;
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
} // namespace loom::vm
