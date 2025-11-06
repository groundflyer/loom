// -*- C++ -*-
// union.cpp
#include <utils/test.hpp>
#include <utils/union.hpp>

using namespace loom;
using namespace std;
using namespace test;

using types       = type_sequence<bool, int, unsigned, float, double>;
using proxy_types = type_sequence<int, unsigned, float>;

using UnionTest       = typelist::to<types, Union>;
using VectorTest      = UnionVector<types>;
using ProxyUnionTest  = typelist::to<proxy_types, ProxyUnion>;
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
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;
    auto          bgen         = rs.make_gen<bool>(0.5);
    auto          igen         = rs.make_gen<int>(-10000, 10000);
    auto          ugen         = rs.make_gen<unsigned>(0, 10000);
    auto          fgen         = rs.make_gen<float>();
    auto          dgen         = rs.make_gen<double>();
    auto          union_assign = [](auto val) noexcept
    {
        UnionTest t;
        t = val;
        return t;
    };
    auto union_val = []<typename T>(UnionTest tr, T gr) noexcept
    { return (T)tr == gr; };
    const auto      n = tm.num_runs;
    VectorTest      vtest(n);
    VectorProxyTest vptest(n);
    VectorProxyTest vptest2(n);
    auto vu_test = [&vtest]<std::ranges::range In>(const In& in) noexcept
    {
        using t = std::ranges::range_value_t<In>;
        std::ranges::copy(in, vtest.view<t>().begin());
        return vtest.view<t>();
    };
    auto vpu_test = [&vptest, n]<std::ranges::range In>(const In& in) noexcept
    {
        // now we get ProxyUnion with operator[] insteand of view
        for(unsigned i = 0; i < n; ++i) vptest[i] = in[i];
        return vptest.view<std::ranges::range_value_t<In>>();
    };
    auto vpu_swap_test = [&]<std::ranges::range In>(const In& in) noexcept
    {
        for(unsigned i = 0; i < n; ++i)
        {
            vptest[i] = in[i];
            swap(vptest[i], vptest2[i]);
        }
        return vptest2.view<std::ranges::range_value_t<In>>();
    };

    track.check(
        NamedTest(tm.test(bgen, union_assign, union_val), "Union assign bool"));
    track.check(
        NamedTest(tm.test(igen, union_assign, union_val), "Union assign int"));
    track.check(NamedTest(tm.test(ugen, union_assign, union_val),
                          "Union assign unsigned"));
    track.check(NamedTest(tm.test(fgen, union_assign, union_val),
                          "Union assign float"));
    track.check(NamedTest(tm.test(dgen, union_assign, union_val),
                          "Union assign double"));

    track.check(
        NamedTest(tm.rng_with_ref<false>(bgen, vu_test, std::identity{}),
                  "Union vector bool"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(bgen, vu_test, std::identity{}),
                  "Union vector bool"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(igen, vu_test, std::identity{}),
                  "Union vector int"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(ugen, vu_test, std::identity{}),
                  "Union vector unsigned"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(fgen, vu_test, std::identity{}),
                  "Union vector float"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(dgen, vu_test, std::identity{}),
                  "Union vector double"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(igen, vpu_test, std::identity{}),
                  "Union vector proxy int"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(ugen, vpu_test, std::identity{}),
                  "Union vector proxy unsigned"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(fgen, vpu_test, std::identity{}),
                  "Union vector proxy float"));
    track.check(
        NamedTest(tm.rng_with_ref<false>(fgen, vpu_swap_test, std::identity{}),
                  "Union vector proxy swap"));

    return track.ret_code();
}
