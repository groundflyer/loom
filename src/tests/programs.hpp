// -*- C++ -*-
// programs.hpp
//

#pragma once

#include <lang/lang.hpp>
#include <utils/debug.hpp>
#include <utils/functional.hpp>
#include <utils/math.hpp>

#include <range/v3/view/subrange.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

// clang-format off

// test programs
template <typename T>
TBEGIN1(increment, val)
    TADD(val, val, 1)
TEND1(val);

// T factorial(T i):
//     T c = i - 1
//     while c != 0:
//         i *= c
//         c -= 1
//     return i
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
template <std::integral T>
TBEGIN1(factorial_int, a)
    TSUB(c, a, 1),
    LABEL(loop),
    TMUL(a, a, c),
    TSUB(c, c, 1),
    TEQ(cmp, c, 0),
    PJMP2(cmp, loop, HALT)
TEND1(a);

template <std::floating_point T>
TBEGIN1(simple_union_test, i)
    TMOV(j, i),
    MOV(IVAR(j), TVAR(j))
END(IVAR(j));

// i < 0.5 ? 200 : 100
template <std::floating_point T>
TBEGIN1(complex_union_test, i)
    TLT(cmp, i, 0.5),
    PJMP2(cmp, l0, l1),

    LABEL(l0),
    MOV(IVAR(i), IVAR(100)),
    JMP(HALT),

    LABEL(l1),
    MOV(IVAR(i), IVAR(200))
END(IVAR(i));

// case i:
// 0: a + 10
// 1: a - 10
// 2: a * 10
// 3: a / 10
template <typename T>
BEGIN(jmp4_test, IVAR(i), TVAR(a))
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
TEND1(a);

// !i && (a > 0.5) ? -a : a
template <std::floating_point T>
BEGIN(choice_test, IVAR(i), TVAR(a))
    INOT(i, i),
    TGT(cmp, a, 0.5),
    TNEG(ma, a),
    IAND(i, i, cmp),
    TCHOICE(ma, i, ma, a)
TEND1(ma);

// (a << b)
template <std::integral T>
TBEGIN2(lshift_test, a, b)
    LSHFT(a, a, b)
TEND1(a);

// (a >> b)
template <std::integral T>
TBEGIN2(rshift_test, a, b)
    RSHFT(a, a, b)
TEND1(a);

// abs(x) > 0
template <std::floating_point T>
TBEGIN1(abs_test, x)
    ABS(x, x),
    TGE(x, x, 0)
END(IVAR(x));

// abs(trunc(x)) <= abs(x)
template <std::floating_point T>
TBEGIN1(trunc_test, x)
    TRUNC(t, x),
    ABS(t, t),
    ABS(x, x),
    TLE(x, t, x)
END(IVAR(x));

// max(a, b) > min(a, b)
template <std::floating_point T>
TBEGIN2(minmax_test, a, b)
    MAX(x, a, b),
    MIN(n, a, b),
    TGT(r, x, n)
END(IVAR(r));

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
template <std::integral T>
constexpr T
val_fac(T i) noexcept
{
    return lucid::fac(i);
}

// simple union validation
constexpr unsigned
val_sut(float i) noexcept
{
    return static_cast<unsigned>(i);
}

// complex union validation
constexpr unsigned
val_cut(float i) noexcept
{
    return i < 0.5f ? 200 : 100;
}

// jump4 validation
constexpr float
val_jmp4(unsigned i, float a) noexcept
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

constexpr float
val_choice(unsigned i, float a) noexcept
{
    return !i && (a > 0.5f) ? -a : a;
}

constexpr unsigned
val_lshift(unsigned a, unsigned b) noexcept
{
    return a << b;
}

constexpr unsigned
val_rshift(unsigned a, unsigned b) noexcept
{
    return a >> b;
}
} // namespace fn

MK_NIEBLOID(val_inc);
MK_NIEBLOID(val_fac);
MK_NIEBLOID(val_sut);
MK_NIEBLOID(val_cut);
MK_NIEBLOID(val_jmp4);
MK_NIEBLOID(val_choice);
MK_NIEBLOID(val_lshift);
MK_NIEBLOID(val_rshift);

using incr_int     = increment<int>;
using fac_int      = factorial_int<int>;
using fac_unsigned = factorial_int<unsigned>;
using sut          = simple_union_test<float>;
using cut          = complex_union_test<float>;
using jmp4_float   = jmp4_test<float>;
using choice_float = choice_test<float>;
using lshift_u     = lshift_test<unsigned>;
using rshift_u     = rshift_test<unsigned>;
using abs_float    = abs_test<float>;
using trunc_float  = trunc_test<float>;
using minmax_float = minmax_test<float>;

template <typename S>
auto
wrap_stream(S& stream) noexcept
{
    return [&stream](const auto& input) noexcept
    {
        // prepare result container
        std::vector<unsigned> out;
        out.reserve(input.size());
        auto inserter = std::back_inserter(out);

        auto step = [&stream]() noexcept
        {
            stream();
            stream.sort();
        };

        // set initial data
        auto [i, _] = stream.set_inputs(input);
        step();

        // feed all the input data
        while(i != std::ranges::end(input))
        {
            auto exs =
                stream.exchange(ranges::subrange(i, std::ranges::end(input)));
            i += std::ranges::size(exs);
            std::ranges::copy(exs, inserter);
            step();
        }

        // compute the rest
        while(!stream.done()) step();

        std::ranges::copy(stream.get_outputs(), inserter);

        ASSERT(out.size() == input.size(), "Output size doesn't match");

        return out;
    };
}
