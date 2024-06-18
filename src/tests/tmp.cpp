// -*- C++ -*-
// test some template metaprogramming functions
// if it compiles, it works

#include <utils/tmp.hpp>

#include <cstdint>

using namespace std;

namespace lucid::intlist
{
using testlist1 = integer_sequence<int, 1, 2, 3, 4>;
using testlist2 = integer_sequence<int, 1, 2, 3, 4, 0, 0, 0>;
using testlist3 = integer_sequence<int, 1, 2, 1, 3, 4, 3, 4, 2, 1>;

template <int val>
using int_constant = integral_constant<int, val>;

using testlist1_ts = type_sequence<int_constant<1>,
                                   int_constant<2>,
                                   int_constant<3>,
                                   int_constant<4>>;

static_assert(is_same_v<reverse<testlist1>, integer_sequence<int, 4, 3, 2, 1>>);
static_assert(
    is_same_v<nappend<integer_sequence<int, 1, 2, 3, 4>, 3, 0>, testlist2>);
static_assert(is_same_v<take<2, testlist1>, integer_sequence<int, 1, 2>>);
static_assert(is_same_v<drop<2, testlist1>, integer_sequence<int, 3, 4>>);
static_assert(elem<2, testlist1>);
static_assert(!elem<5, testlist1>);
static_assert(index<2, testlist1> == 1);
static_assert(is_same_v<purge<3, integer_sequence<int, 3, 1, 2, 3, 4, 3, 3>>,
                        integer_sequence<int, 1, 2, 4>>);
static_assert(
    is_same_v<map_f<decltype([](int i) { return i + 20; }), testlist1>,
              integer_sequence<int, 21, 22, 23, 24>>);
static_assert(is_same_v<map_t<int_constant, testlist1>, testlist1_ts>);
static_assert(
    is_same_v<map_t<int_constant, integer_sequence<int>>, type_sequence<>>);
static_assert(!has_duplicates<testlist1>);
static_assert(has_duplicates<testlist2>);
static_assert(is_same_v<testlist1, nub<testlist3>>);
constexpr inline unsigned five = 5u;
static_assert(is_same_v<powerof2s<five>,
                        std::integer_sequence<unsigned, 1, 2, 4, 8, 16>>);

using lcnt = lehmer_code<463>;
using lcnv = index_sequence<3, 4, 1, 0, 1, 0>;
static_assert(is_same_v<lcnv, lcnt>);

constexpr size_t L = 3;
using pm_seq0      = make_index_sequence<L>;
using pm_seq1      = index_sequence<0, 2, 1>;
using pm_seq2      = index_sequence<1, 0, 2>;
using pm_seq3      = index_sequence<1, 2, 0>;
using pm_seq4      = index_sequence<2, 0, 1>;
using pm_seq5      = index_sequence<2, 1, 0>;
static_assert(is_same_v<nlperm<0, L>, pm_seq0>);
static_assert(is_same_v<nlperm<1, L>, pm_seq1>);
static_assert(is_same_v<nlperm<2, L>, pm_seq2>);
static_assert(is_same_v<nlperm<3, L>, pm_seq3>);
static_assert(is_same_v<nlperm<4, L>, pm_seq4>);
static_assert(is_same_v<nlperm<5, L>, pm_seq5>);
} // namespace lucid::intlist

template <typename T>
struct test_f
{
    static constexpr size_t value = sizeof(T);
};

namespace lucid::typelist
{
using testlist1    = type_sequence<bool, int, float, char>;
using testlist2    = type_sequence<double, unsigned, short>;
using testlist3    = type_sequence<long, size_t, unsigned char>;
using testlist4    = type_sequence<int, bool, int, float, int, char, int>;
using testlist4nub = type_sequence<int, bool, float, char>;
using testmap      = type_sequence<type_sequence<char, unsigned char>,
                              type_sequence<int, unsigned int>,
                              type_sequence<float, double>>;
using splitest1  = type_sequence<bool, int, float, bool, double, unsigned, int>;
using splittest2 = type_sequence<type_sequence<int, float>,
                                 type_sequence<double, unsigned, int>>;
#define I(i) std::integral_constant<int, i>
using flatten_test =
    type_sequence<I(1),
                  I(2),
                  type_sequence<I(3), I(4), type_sequence<I(5), I(6)>, I(7)>,
                  type_sequence<I(8), I(9)>,
                  I(10)>;
using flatten_val =
    type_sequence<I(1), I(2), I(3), I(4), I(5), I(6), I(7), I(8), I(9), I(10)>;

static_assert(length<testlist1> == 4);
static_assert(
    is_same_v<reverse<testlist1>, type_sequence<char, float, int, bool>>);
static_assert(is_same_v<take<2, testlist1>, type_sequence<bool, int>>);
static_assert(is_same_v<take<length<testlist1> - 1, testlist1>,
                        type_sequence<bool, int, float>>);
static_assert(is_same_v<drop<2, testlist1>, type_sequence<float, char>>);
static_assert(elem<float, testlist1>);
static_assert(!elem<long, testlist1>);
static_assert(index<float, testlist1> == 2);
static_assert(!same<testlist1>);
static_assert(same<type_sequence<int, int, int>>);
static_assert(is_same_v<zip<testlist2, testlist3>,
                        type_sequence<type_pair<double, long>,
                                      type_pair<unsigned, size_t>,
                                      type_pair<short, unsigned char>>>);
static_assert(is_same_v<zip<testlist1, testlist2>,
                        type_sequence<type_pair<bool, double>,
                                      type_pair<int, unsigned>,
                                      type_pair<float, short>>>);
static_assert(is_same_v<concat<type_sequence<testlist1, testlist2, testlist3>>,
                        type_sequence<bool,
                                      int,
                                      float,
                                      char,
                                      double,
                                      unsigned,
                                      short,
                                      long,
                                      size_t,
                                      unsigned char>>);
static_assert(
    is_same_v<purge<int, testlist4>, type_sequence<bool, float, char>>);
static_assert(has_duplicates<testlist4>);
static_assert(!has_duplicates<testlist1>);
static_assert(is_same_v<filter<std::is_integral, testlist1>,
                        type_sequence<bool, int, char>>);
static_assert(is_same_v<testlist4nub, nub<testlist4>>);
static_assert(is_same_v<lookup<float, testmap>, double>);
static_assert(is_void_v<lookup<long*, testmap>>);
static_assert(is_same_v<find<is_floating_point, testlist1>, float>);
static_assert(is_void_v<find<is_pointer, testlist1>>);
static_assert(is_same_v<split<bool, splitest1>, splittest2>);
static_assert(
    is_same_v<to_integer_sequence<intlist::testlist1_ts>, intlist::testlist1>);
static_assert(
    is_same_v<enumerate<testlist2>,
              type_sequence<type_pair<double, integral_constant<size_t, 0>>,
                            type_pair<unsigned, integral_constant<size_t, 1>>,
                            type_pair<short, integral_constant<size_t, 2>>>>);
static_assert(is_same_v<flatten<flatten_test>, flatten_val>);
static_assert(
    is_same_v<
        map<test_f,
            type_sequence<int8_t, int16_t, int32_t, int64_t, float, double>>,
        integer_sequence<size_t, 1, 2, 4, 8, 4, 8>>);
} // namespace lucid::typelist

int
main()
{
    return 0;
}
