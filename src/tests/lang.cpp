// -*- C++ -*-
// lang.cpp

#include <lang/bse.hpp>
#include <lang/lang.hpp>

using namespace lucid;

template <typename T>
struct increment
{
    using val = Variable<T, "val">;
    using one = Variable<T, "one">;
    using commands =
        type_sequence<StoreConstant<one, std::integral_constant<T, 1>>,
                      ComputeStore<val, std::plus<T>, val, one>>;

    using inputs  = type_sequence<val>;
    using outputs = type_sequence<val>;
    using program = Program<inputs, outputs, commands>;
    using eval    = basic_scalar_evaluator_t<program>;
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
        type_sequence<StoreConstant<one, std::integral_constant<T, 1>>,
                      StoreConstant<zero, std::integral_constant<T, 0>>,
                      ComputeStore<cx, std::minus<T>, ax, one>,
                      loop,
                      ComputeStore<ax, std::multiplies<T>, ax, cx>,
                      ComputeStore<cx, std::minus<T>, cx, one>,
                      ComputeStore<cmp, std::equal_to<T>, cx, zero>,
                      Jump<cmp, type_sequence<loop, EndLabel>>>;

    using inputs  = type_sequence<ax>;
    using outputs = type_sequence<ax>;

    using program = Program<inputs, outputs, commands>;

    using eval = basic_scalar_evaluator_t<program>;
};

int main(/*int argc, char* argv[]*/)
{
    using incr_int = increment<int>;
    typename incr_int::eval p;
    int                     ret = p(-1);

    using fac_unsigned = factorial<unsigned>;
    typename fac_unsigned::eval fac;
    auto                        fr = fac(4);
    ret += fr != 24;
    return ret;
}
