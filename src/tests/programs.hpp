// -*- C++ -*-
// programs.hpp
//

#pragma once

#include <common/functional.hpp>
#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

// clang-format off

// test programs
template <typename T>
TKERNEL1(increment, val)
    TADD(val, val, 1)
TKERNEL_END1(val);

// T factorial(T a):
// if a <= 1:
//  return 1
// T c = a - 1
// while c != 0:
//  a *= c
//  c -= 1
// return a
//
// input: a
// output: a
//
// c = a - 1
// loop:
// a = a * c
// c = c - 1
// cmp = c == 0
// jump: cmp: 0 -> loop, 1 -> end
template <std::unsigned_integral T>
TKERNEL1(fac_test, a)
    TLE(cmp, a, 1),
    TCMOV(a, cmp, 1, a),
    IPJMP2(cmp, cinit, HALT),
    LABEL(cinit),
    TSUB(c, a, 1),
    LABEL(loop),
    TMUL(a, a, c),
    TSUB(c, c, 1),
    TEQ(cmp, c, 0),
    IPJMP2(cmp, loop, HALT)
TKERNEL_END1(a);

template <std::floating_point T>
TKERNEL1(union_test, i)
    TMOV(j, i),
    MOV(IVAR(j), TVAR(j))
KERNEL_END(IVAR(j));

// i < 0.5 ? 200 : 100
template <std::floating_point T>
TKERNEL1(union_cmov_test, i)
    TLT(cmp, i, 0.5),
    ICMOV(i, cmp, 200, 100)
KERNEL_END(IVAR(i));

// case i:
// 0: 1
// 1: 3
// 2: 7
// 3: 11
template <typename T>
KERNEL(jmp4_test, IVAR(i))
    IPJMP4(i, l0, l1, l2, l3),

    LABEL(l0),
    IMOV(i, 1),
    JMP(HALT),

    LABEL(l1),
    IMOV(i, 3),
    JMP(HALT),

    LABEL(l2),
    IMOV(i, 7),
    JMP(HALT),

    LABEL(l3),
    IMOV(i, 11)
KERNEL_END(IVAR(i));

// StandardTestSuite uses comparison with reference implementation.
// Scalar boolean operations are clamped to 0 and 1, but
// vector integer operations produce values with either all bits set or none.
// Thus boolean logic tests clamps values explicitly.
// FIXME: The better approach would be using boolean logic theorems and properties.

// a && b
template <std::integral T>
KERNEL(bool_and_test, IVAR(a), IVAR(b))
    ILAND(a, a, b),
    IMIN(a, a, 1)               // clamp SIMD values
KERNEL_END(IVAR(a));

// a || b
template <std::integral T>
KERNEL(bool_or_test, IVAR(a), IVAR(b))
    ILOR(a, a, b),
    IMIN(a, a, 1)               // clamp SIMD values
KERNEL_END(IVAR(a));

// !a
template <std::integral T>
KERNEL(bool_not_test, IVAR(a))
    ILNOT(a, a),
    IMIN(a, a, 1)               // clamp SIMD values
KERNEL_END(IVAR(a));

// a & b
template <std::integral T>
KERNEL(bit_and_test, IVAR(a), IVAR(b))
    IBAND(a, a, b)
KERNEL_END(IVAR(a));

// a | b
template <std::integral T>
KERNEL(bit_or_test, IVAR(a), IVAR(b))
    IBOR(a, a, b)
KERNEL_END(IVAR(a));

// a ^ b
template <std::integral T>
KERNEL(bit_xor_test, IVAR(a), IVAR(b))
    IXOR(a, a, b)
KERNEL_END(IVAR(a));

// ~a
template <std::integral T>
KERNEL(bit_not_test, IVAR(a))
    IBNOT(a, a)
KERNEL_END(IVAR(a));

// (a << b)
template <std::integral T>
TKERNEL2(lshift_test, a, b)
    ILSHFT(a, a, b)
TKERNEL_END1(a);

// (a >> b)
template <std::integral T>
TKERNEL2(rshift_test, a, b)
    IRSHFT(a, a, b)
TKERNEL_END1(a);

// abs(x) > 0
template <std::floating_point T>
TKERNEL1(abs_test, x)
    TABS(x, x),
    TGE(x, x, 0)
KERNEL_END(IVAR(x));

// abs(trunc(x)) <= abs(x)
template <std::floating_point T>
TKERNEL1(trunc_test, x)
    TTRUNC(t, x),
    TABS(t, t),
    TABS(x, x),
    TLE(x, t, x)
KERNEL_END(IVAR(x));

// ceil(x) >= floor(x)
template <std::floating_point T>
TKERNEL1(ceilfloor, x)
    TCEIL(c, x),
    TFLOOR(x, x),
    TGE(c, c, x)
KERNEL_END(IVAR(c));

// max(a, b) > min(a, b)
template <std::floating_point T>
TKERNEL2(minmax_test, a, b)
    TMAX(x, a, b),
    TMIN(n, a, b),
    TGT(r, x, n)
KERNEL_END(IVAR(r));

// fibonacci number
template <std::unsigned_integral T>
TKERNEL1(fib_test, n)
    TLE(cmp, n, 1),
    TCMOV(c, cmp, n, 1),
    IPJMP2(cmp, work, HALT),
    LABEL(work),
    TMOV(p, 0),
    TMOV(c, 1),
    TMOV(i, 2),
    LABEL(loop),
    TADD(t, p, c),
    TMOV(p, c),
    TMOV(c, t),
    TADD(i, i, 1),
    TLE(cmp, i, n),
    IPJMP2(cmp, HALT, loop)
TKERNEL_END1(c);

/// abs(fma(a, b, -c) / (a * b + (-c)) - 1) < 1e-6
template <std::floating_point T>
TKERNEL3(arithmetic_test, a, b, c)
    TNEG(c, c),
    TFMA(t, a, b, c),
    TMUL(a, a, b),
    TADD(a, a, c),
    TDIV(a, t, a),
    TSUB(a, a, 1),
    TABS(a, a),
    TLT(a, a, 1e-6)
KERNEL_END(IVAR(a));

// clang-format on

// validation programs
namespace fn
{
// increment validation
template <std::integral T>
constexpr T
val_inc(T i) noexcept
{
    return i + 1;
}

// factorial validation
template <std::unsigned_integral T>
constexpr T
val_fac(T i) noexcept
{
    return loom::fac(i);
}

// simple union validation
template <std::floating_point T>
constexpr loom::uint_of_size_t<sizeof(T)>
val_union1(T i) noexcept
{
    return static_cast<loom::uint_of_size_t<sizeof(T)>>(i);
}

// complex union validation
template <std::floating_point T>
constexpr loom::uint_of_size_t<sizeof(T)>
val_union2(T i) noexcept
{
    using R = loom::uint_of_size_t<sizeof(T)>;
    return i < static_cast<T>(0.5) ? static_cast<R>(200) : static_cast<R>(100);
}

// jump4 validation
template <std::unsigned_integral T>
constexpr T
val_jmp4(const T i) noexcept
{
    switch(i)
    {
    case 0: return 1;
    case 1: return 3;
    case 2: return 7;
    case 3: return 11;
    }
    return i;
}

template <std::unsigned_integral T>
constexpr T
val_lshift(T a, T b) noexcept
{
    return a << b;
}

template <std::unsigned_integral T>
constexpr T
val_rshift(T a, T b) noexcept
{
    return a >> b;
}

template <std::unsigned_integral T>
constexpr T
val_fib(T n) noexcept
{
    if(n <= 1) return n;

    T p = 0, c = 1;
    for(T i = 2; i <= n; ++i)
    {
        T t = p + c;
        p   = c;
        c   = t;
    }

    return c;
}

// libc++ bug: version 20 std::bit_ operations don't correspond to operators
// std::bit_not returns wrong values with u8 and u16: cast to int might be the
// problem
template <std::unsigned_integral T>
constexpr T
val_bit_not(T v) noexcept
{
    return ~v;
}
} // namespace fn

MK_NIEBLOID(val_inc);
MK_NIEBLOID(val_fac);
MK_NIEBLOID(val_union1);
MK_NIEBLOID(val_union2);
MK_NIEBLOID(val_jmp4);
MK_NIEBLOID(val_lshift);
MK_NIEBLOID(val_rshift);
MK_NIEBLOID(val_fib);
MK_NIEBLOID(val_bit_not);
MK_NIEBLOID(val_arithmetic);
