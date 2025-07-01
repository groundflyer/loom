// -*- C++ -*-
// programs.hpp
//

#pragma once

#include <lang/core.hpp>
#include <lang/macros.hpp>
#include <lang/operators.hpp>
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

// 0: a & b;
// 1: a | b;
// 2: a ^ 5;
// 3: ~a;
template <std::integral T>
BEGIN(bit_test, IVAR(i), IVAR(a), IVAR(b))
    PJMP4(i, l0, l1, l2, l3),

    LABEL(l0),
    BAND(a, a, b),
    JMP(HALT),

    LABEL(l1),
    BOR(a, a, b),
    JMP(HALT),

    LABEL(l2),
    XOR(a, a, b),
    JMP(HALT),

    LABEL(l3),
    BNOT(a, a)
END(IVAR(a));

// !i && (a > 0.5) ? -a : a
template <std::floating_point T>
BEGIN(choice_test, IVAR(i), TVAR(a))
    LNOT(i, i),
    TGT(cmp, a, 0.5),
    TNEG(ma, a),
    LAND(i, i, cmp),
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
    TABS(x, x),
    TGE(x, x, 0)
END(IVAR(x));

// abs(trunc(x)) <= abs(x)
template <std::floating_point T>
TBEGIN1(trunc_test, x)
    TTRUNC(t, x),
    TABS(t, t),
    TABS(x, x),
    TLE(x, t, x)
END(IVAR(x));

// ceil(x) >= floor(x)
template <std::floating_point T>
TBEGIN1(ceilfloor, x)
    TCEIL(c, x),
    TFLOOR(x, x),
    TGE(c, c, x)
END(IVAR(c));

// max(a, b) > min(a, b)
template <std::floating_point T>
TBEGIN2(minmax_test, a, b)
    TMAX(x, a, b),
    TMIN(n, a, b),
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
template <std::floating_point T>
constexpr lucid::uint_of_size_t<sizeof(T)>
val_sut(T i) noexcept
{
    return static_cast<lucid::uint_of_size_t<sizeof(T)>>(i);
}

// complex union validation
template <std::floating_point T>
constexpr lucid::uint_of_size_t<sizeof(T)>
val_cut(T i) noexcept
{
    using R = lucid::uint_of_size_t<sizeof(T)>;
    return i < static_cast<T>(0.5) ? static_cast<R>(200) : static_cast<R>(100);
}

// jump4 validation
template <std::floating_point T>
constexpr T
val_jmp4(lucid::uint_of_size_t<sizeof(T)> i, T a) noexcept
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

// bitwise ops validation
template <std::unsigned_integral T>
constexpr T
val_bit(T i, T a, T b)
{
    switch(i)
    {
    case 0: return a & b;
    case 1: return a | b;
    case 2: return a ^ b;
    case 3: return ~a;
    }
    return a;
}

template <std::floating_point T>
constexpr T
val_choice(lucid::uint_of_size_t<sizeof(T)> i, T a) noexcept
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
} // namespace fn

MK_NIEBLOID(val_inc);
MK_NIEBLOID(val_fac);
MK_NIEBLOID(val_sut);
MK_NIEBLOID(val_cut);
MK_NIEBLOID(val_jmp4);
MK_NIEBLOID(val_choice);
MK_NIEBLOID(val_bit);
MK_NIEBLOID(val_lshift);
MK_NIEBLOID(val_rshift);

template <typename S>
class StreamWrapper
{
    S* p_stream;

  public:
    StreamWrapper(S* stream) noexcept : p_stream(stream) {}

    template <typename Input>
    auto
    operator()(const Input& input) noexcept
    {
        // prepare result container
        using program = S::program;
        std::vector<lucid::output_type<program>> out;
        out.reserve(input.size());
        auto inserter = std::back_inserter(out);

        auto step = [s = this->p_stream]() noexcept
        {
            (*s)();
            s->sort();
        };

        // set initial data
        auto [i, _] = p_stream->set_inputs(input);
        step();

        // feed all the input data
        while(i != std::ranges::end(input))
        {
            auto exs = p_stream->exchange(
                ranges::subrange(i, std::ranges::end(input)));
            i += std::ranges::size(exs);
            std::ranges::copy(exs, inserter);
            step();
        }

        // compute the rest
        while(!p_stream->done()) step();

        std::ranges::copy(p_stream->get_outputs(), inserter);

        ASSERT(out.size() == input.size(), "Output size doesn't match");

        return out;
    }
};
