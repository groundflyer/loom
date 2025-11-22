// -*- C++ -*-
// programs.hpp
//

#pragma once

#include <utils/functional.hpp>
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
TKERNEL1(factorial_int, a)
    TLE(cmp, a, 1),
    TCMOV(a, cmp, 1, a),
    PJMP2(cmp, cinit, HALT),
    LABEL(cinit),
    TSUB(c, a, 1),
    LABEL(loop),
    TMUL(a, a, c),
    TSUB(c, c, 1),
    TEQ(cmp, c, 0),
    PJMP2(cmp, loop, HALT)
TKERNEL_END1(a);

template <std::floating_point T>
TKERNEL1(simple_union_test, i)
    TMOV(j, i),
    MOV(IVAR(j), TVAR(j))
KERNEL_END(IVAR(j));

// i < 0.5 ? 200 : 100
template <std::floating_point T>
TKERNEL1(complex_union_test, i)
    TLT(cmp, i, 0.5),
    PJMP2(cmp, l0, l1),

    LABEL(l0),
    MOV(IVAR(i), IVAR(100)),
    JMP(HALT),

    LABEL(l1),
    MOV(IVAR(i), IVAR(200))
KERNEL_END(IVAR(i));

// case i:
// 0: a + 10
// 1: a - 10
// 2: a * 10
// 3: a / 10
template <typename T>
KERNEL(jmp4_test, IVAR(i), TVAR(a))
    PJMP4(i, l0, l1, l2, l3),

    LABEL(l0),
    TADD(a, a, 100),
    JMP(HALT),

    LABEL(l1),
    TSUB(a, a, 50),
    JMP(HALT),

    LABEL(l2),
    TMUL(a, a, 5),
    JMP(HALT),

    LABEL(l3),
    TDIV(a, a, 2)
TKERNEL_END1(a);

// a & b
template <std::integral T>
KERNEL(bit_and_test, IVAR(a), IVAR(b))
    BAND(a, a, b)
KERNEL_END(IVAR(a));

// a | b
template <std::integral T>
KERNEL(bit_or_test, IVAR(a), IVAR(b))
    BOR(a, a, b)
KERNEL_END(IVAR(a));

// a ^ b
template <std::integral T>
KERNEL(bit_xor_test, IVAR(a), IVAR(b))
    XOR(a, a, b)
KERNEL_END(IVAR(a));

// ~a
template <std::integral T>
KERNEL(bit_not_test, IVAR(a))
    BNOT(a, a)
KERNEL_END(IVAR(a));

// !i && (a > 0.5) ? -a : a
template <std::floating_point T>
KERNEL(cmov_test, IVAR(i), TVAR(a))
    LNOT(i, i),
    TGT(cmp, a, 0.5),
    TNEG(ma, a),
    LAND(i, i, cmp),
    TCMOV(ma, i, ma, a)
TKERNEL_END1(ma);

// (a << b)
template <std::integral T>
TKERNEL2(lshift_test, a, b)
    LSHFT(a, a, b)
TKERNEL_END1(a);

// (a >> b)
template <std::integral T>
TKERNEL2(rshift_test, a, b)
    RSHFT(a, a, b)
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
TKERNEL1(fib, n)
    TLE(cmp, n, 1),
    TCMOV(c, cmp, n, 1),
    PJMP2(cmp, work, HALT),
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
    PJMP2(cmp, HALT, loop)
TKERNEL_END1(c);

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
val_sut(T i) noexcept
{
    return static_cast<loom::uint_of_size_t<sizeof(T)>>(i);
}

// complex union validation
template <std::floating_point T>
constexpr loom::uint_of_size_t<sizeof(T)>
val_cut(T i) noexcept
{
    using R = loom::uint_of_size_t<sizeof(T)>;
    return i < static_cast<T>(0.5) ? static_cast<R>(200) : static_cast<R>(100);
}

// jump4 validation
template <std::floating_point T>
constexpr T
val_jmp4(loom::uint_of_size_t<sizeof(T)> i, T a) noexcept
{
    switch(i)
    {
    case 0: return a + 100;
    case 1: return a - 50;
    case 2: return a * 5;
    case 3: return a / 2;
    }
    return a;
}

template <std::floating_point T>
constexpr T
val_cmov(loom::uint_of_size_t<sizeof(T)> i, T a) noexcept
{
    return !i && (a > static_cast<T>(0.5)) ? -a : a;
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
} // namespace fn

MK_NIEBLOID(val_inc);
MK_NIEBLOID(val_fac);
MK_NIEBLOID(val_sut);
MK_NIEBLOID(val_cut);
MK_NIEBLOID(val_jmp4);
MK_NIEBLOID(val_cmov);
MK_NIEBLOID(val_lshift);
MK_NIEBLOID(val_rshift);
MK_NIEBLOID(val_fib);
