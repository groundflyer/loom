// -*- C++ -*-
// lang.cpp

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/lang.hpp>

#include <array>
#include <initializer_list>

using namespace lucid;

template <typename T>
struct increment
{
    using val      = Variable<T, "val">;
    using one      = Variable<T, "one">;
    using commands = type_sequence<Store<one, std::integral_constant<T, 1>>,
                                   Compute<val, std::plus<T>, val, one>>;

    using inputs  = type_sequence<val>;
    using outputs = type_sequence<val>;
    using program = Program<inputs, outputs, commands>;
};

// T factorial(T i):
//     T c = i - 1
//     while c != 0:
//         i *= c
//         c -= 1
//     return i
//
// input: ax
// outupt: ax
//
// o = 1
// z = 0
// cx = ax - 1
// loop:
// ax = ax * cx
// cx = cx - 1
// cmp = cx == 0
// jump: cmp: 0 -> loop, 1 -> end
template <typename T>
struct factorial
{
    using ax   = Variable<T, "ax">; // accumulator
    using cx   = Variable<T, "cx">; // counter
    using one  = Variable<T, "one">;
    using zero = Variable<T, "zero">;
    using cmp  = Variable<std::uint32_t, "cmp">;
    using loop = Label<"loop">;

    using commands =
        type_sequence<Store<one, std::integral_constant<T, 1>>,
                      Store<zero, std::integral_constant<T, 0>>,
                      Compute<cx, std::minus<T>, ax, one>,
                      loop,
                      Compute<ax, std::multiplies<T>, ax, cx>,
                      Compute<cx, std::minus<T>, cx, one>,
                      Compute<cmp, std::equal_to<T>, cx, zero>,
                      ParmJump<cmp, type_sequence<loop, EndLabel>>>;

    using inputs  = type_sequence<ax>;
    using outputs = type_sequence<ax>;

    using program = Program<inputs, outputs, commands>;
};

template <typename T>
using ar4 = std::array<T, 4>;

int main(/*int argc, char* argv[]*/)
{
    using incr_int = increment<int>;
    bse::evaluator<incr_int::program> p;
    int                               ret = p(-1);

    using iar4mem =
        typename bve::basic_vector_evaluator<incr_int::program,
                                             ar4,
                                             typename incr_int::one>::memory;
    static_assert(std::ranges::random_access_range<iar4mem>);
    static_assert(std::swappable<iar4mem::value_type>);
    bve::evaluator<incr_int::program, ar4, typename incr_int::one> pv;
    ret += !std::ranges::all_of(pv(0),
                                [](const auto i) constexpr noexcept
                                { return i == 1; });

    auto v1 = {0, 1, 2, 3};
    auto v2 = {1, 2, 3, 4};
    ret += !std::ranges::equal(pv(v1), v2);

    using fac_unsigned = factorial<unsigned>;
    bse::evaluator<fac_unsigned::program> fac;

    const unsigned fac_in  = 6;
    const unsigned fac_chk = 720;
    auto           fac_out = fac(fac_in);
    ret += fac_out != fac_chk;

    bve::evaluator<fac_unsigned::program, ar4> vfac;

    const auto vfac_in  = {4u, 5u, 6u, 7u};
    const auto vfac_chk = {24u, 120u, 720u, 5040u};
    auto       vfac_out = vfac(vfac_in);
    std::ranges::sort(vfac_out);
    ret += !std::ranges::equal(vfac_out, vfac_chk);

    return ret;
}
