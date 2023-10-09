// -*- C++ -*-
// lang.cpp

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/lang.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <array>
#include <initializer_list>

using namespace lucid;

template <typename T>
struct increment
{
    using val = Variable<T, "val">;

    TBEGIN1(program, val)
        TADD(val, val, 1)
    TEND1(val)
};

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
template <typename T>
struct factorial
{
    using a = Variable<T, "a">; // accumulator

    TBEGIN1(program, a)
        TSUB(c, a, 1)
        , LABEL(loop)
        , TMUL(a, a, c)
        , TSUB(c, c, 1)
        , TEQ(cmp, c, 0)
        , PJMP2(cmp, loop, END)
    TEND1(a)
};

int main(/*int argc, char* argv[]*/)
{
    using incr_int = increment<int>;
    bse::evaluator<incr_int::program> p;
    int                               ret = p(-1);

    auto ok = [&](std::string_view name) noexcept
    { fmt::print("{} : {}\n", name, ret == 0 ? "OK" : "FAIL"); };
    ok("increment bse");

    using iar4mem =
        typename bve::basic_vector_evaluator<incr_int::program, 4>::memory;
    static_assert(std::ranges::random_access_range<iar4mem>);
    static_assert(std::swappable<iar4mem::value_type>);
    bve::evaluator<incr_int::program, 4> pv;
    ret += !std::ranges::all_of(pv(0),
                                [](const auto i) constexpr noexcept
                                { return i == 1; });
    ok("increment bve single input");

    auto v1 = {0, 1, 2, 3};
    auto v2 = {1, 2, 3, 4};
    ret += !std::ranges::equal(pv(v1), v2);
    ok("increment bve multi input");

    using fac_unsigned = factorial<unsigned>;
    bse::evaluator<fac_unsigned::program> fac;

    const unsigned fac_in  = 6;
    const unsigned fac_chk = 720;
    auto           fac_out = fac(fac_in);
    ret += fac_out != fac_chk;
    ok("fac bse");

    bve::evaluator<fac_unsigned::program, 4> vfac;

    static_assert(std::is_swappable_v<
                  bve::basic_vector_evaluator<fac_unsigned::program,
                                              4>::memory::value_type>);

    const auto vfac_in  = {4u, 5u, 6u, 7u};
    const auto vfac_chk = {24u, 120u, 720u, 5040u};
    auto       vfac_out = vfac(vfac_in);
    std::ranges::sort(vfac_out);
    ret += !std::ranges::equal(vfac_out, vfac_chk);
    ok("fac bve");
    fmt::print("vfac_out = {}\n", vfac_out);

    return ret;
}
