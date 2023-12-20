// -*- C++ -*-
// union.cpp
#include <utils/test.hpp>
#include <utils/union.hpp>

using namespace lucid;
using namespace std;
using namespace test;

using types = type_sequence<bool, int, unsigned, float, double>;
using proxy_types = type_sequence<int, unsigned, float>;

using UnionTest      = typelist::to<types, Union>;
using VectorTest     = UnionVector<types>;
using ProxyUnionTest = typelist::to<proxy_types, ProxyUnion>;
using VectorProxyTest = UnionVector<proxy_types>;

static_assert(std::is_copy_constructible_v<UnionTest>);
static_assert(std::is_copy_assignable_v<UnionTest>);
static_assert(std::is_move_constructible_v<UnionTest>);
static_assert(std::is_move_assignable_v<UnionTest>);

static_assert(std::is_copy_constructible_v<VectorTest>);
static_assert(std::is_copy_assignable_v<VectorTest>);
static_assert(std::is_move_constructible_v<VectorTest>);
static_assert(std::is_move_assignable_v<VectorTest>);

static_assert(std::is_swappable_v<ProxyUnionTest>);

int
main(int argc, char* argv[])
{
    Track       track(argc, argv);
    RandomState rs;
    auto        bgen         = rs.make_gen<bool>(0.5);
    auto        igen         = rs.make_gen<int>(-10000, 10000);
    auto        ugen         = rs.make_gen<unsigned>(0, 10000);
    auto        fgen         = rs.make_gen<float>(-1000.f, 1000.f);
    auto        dgen         = rs.make_gen<double>(-1000., 1000.);
    auto        union_assign = [](auto val) noexcept
    {
        UnionTest t;
        t = val;
        return t;
    };
    auto union_val = []<typename T>(UnionTest tr, T gr) noexcept
    { return (T)tr == gr; };
    VectorTest      vtest;
    VectorProxyTest vptest;
    VectorProxyTest vptest2;
    auto            vu_test =
        [&vtest]<std::ranges::range In>(unsigned n, const In& in) noexcept
    {
        vtest.resize(n);
        using t = std::ranges::range_value_t<In>;
        std::ranges::copy(in, vtest.view<t>().begin());
        return vtest.view<t>();
    };
    auto vpu_test =
        [&vptest]<std::ranges::range In>(unsigned n, const In& in) noexcept
    {
        vptest.resize(n);
        // now we get ProxyUnion with operator[] insteand of view
        for(unsigned i = 0; i < n; ++i) vptest[i] = in[i];
        return vptest.view<std::ranges::range_value_t<In>>();
    };
    auto vpu_swap_test =
        [&]<std::ranges::range In>(unsigned n, const In& in) noexcept
    {
        vptest.resize(n);
        vptest2.resize(n);
        for(unsigned i = 0; i < n; ++i)
        {
            vptest[i] = in[i];
            swap(vptest[i], vptest2[i]);
        }
        return vptest2.view<std::ranges::range_value_t<In>>();
    };

    track.parm_test("Union assign bool", bgen, union_assign, union_val);
    track.parm_test("Union assign int", igen, union_assign, union_val);
    track.parm_test("Union assign unsigned", ugen, union_assign, union_val);
    track.parm_test("Union assign float", fgen, union_assign, union_val);
    track.parm_test("Union assign double", dgen, union_assign, union_val);
    track.parm_range_test<false>("Union vector bool", bgen, vu_test);
    track.parm_range_test<false>("Union vector int", igen, vu_test);
    track.parm_range_test<false>("Union vector unsigned", ugen, vu_test);
    track.parm_range_test<false>("Union vector float", fgen, vu_test);
    track.parm_range_test<false>("Union vector double", dgen, vu_test);
    track.parm_range_test<false>("Union vector proxy int", igen, vpu_test);
    track.parm_range_test<false>("Union vector proxy unsigned", ugen, vpu_test);
    track.parm_range_test<false>("Union vector proxy float", fgen, vpu_test);
    track.parm_range_test<false>("Union vector proxy swap",
                                 fgen,
                                 vpu_swap_test);

    return track.code();
}
