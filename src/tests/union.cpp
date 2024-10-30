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

struct range_val_id
{
    template <typename In>
    constexpr In
    operator()(unsigned, In&& in) const noexcept
    {
        return std::forward<In>(in);
    }
};

constexpr inline range_val_id rng_id{};

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

    track.check(NamedTest(tm.rng_with_ref<false>(bgen, vu_test, rng_id),
                          "Union vector bool"));
    track.check(NamedTest(tm.rng_with_ref<false>(bgen, vu_test, rng_id),
                          "Union vector bool"));
    track.check(NamedTest(tm.rng_with_ref<false>(igen, vu_test, rng_id),
                          "Union vector int"));
    track.check(NamedTest(tm.rng_with_ref<false>(ugen, vu_test, rng_id),
                          "Union vector unsigned"));
    track.check(NamedTest(tm.rng_with_ref<false>(fgen, vu_test, rng_id),
                          "Union vector float"));
    track.check(NamedTest(tm.rng_with_ref<false>(dgen, vu_test, rng_id),
                          "Union vector double"));
    track.check(NamedTest(tm.rng_with_ref<false>(igen, vpu_test, rng_id),
                          "Union vector proxy int"));
    track.check(NamedTest(tm.rng_with_ref<false>(ugen, vpu_test, rng_id),
                          "Union vector proxy unsigned"));
    track.check(NamedTest(tm.rng_with_ref<false>(fgen, vpu_test, rng_id),
                          "Union vector proxy float"));
    track.check(NamedTest(tm.rng_with_ref<false>(fgen, vpu_swap_test, rng_id),
                          "Union vector proxy swap"));

    return track.ret_code();
}
