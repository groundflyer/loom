// -*- C++ -*-
// union.cpp
#include <utils/union.hpp>

using namespace lucid;
using namespace std;

using UnionTest = Union<bool, int, unsigned, float, double>;
using VectorTest =
    UnionVector<type_sequence<bool, int, unsigned, float, double>>;

static_assert(std::is_copy_constructible_v<UnionTest>);
static_assert(std::is_copy_assignable_v<UnionTest>);
static_assert(std::is_move_constructible_v<UnionTest>);
static_assert(std::is_move_assignable_v<UnionTest>);

static_assert(std::is_copy_constructible_v<VectorTest>);
static_assert(std::is_copy_assignable_v<VectorTest>);
static_assert(std::is_move_constructible_v<VectorTest>);
static_assert(std::is_move_assignable_v<VectorTest>);

int
main()
{
    int ret = 0;

    UnionTest test;

    test = true;
    ret += !(bool)test;
    test = false;
    ret += (bool)test;

    test = 42;
    ret += int(test) != 42;

    test = 55u;
    ret += static_cast<unsigned>(test) != 55u;

    test = 0.5f;
    ret += float(test) != 0.5f;

    test = 1.2;
    ret += test.ref<double>() != 1.2;

    VectorTest vtest(1000u, -0.5);
    ret += std::ranges::none_of(vtest.view<double>(),
                                [](double v) noexcept { return v == -0.5; });

    std::ranges::fill(vtest.view<int>(), 10);
    ret += std::ranges::none_of(vtest.view<int>(),
                                [](int v) noexcept { return v == 10; });

    auto vtest2(vtest);
    ret += std::ranges::none_of(vtest2.view<int>(),
                                [](int v) noexcept { return v == 10; });

    return ret;
}
