// -*- C++ -*-
// tuple.cpp
#include <utils/steady_tuple.hpp>
#include <utils/tuple.hpp>

#include <array>

using TT = std::tuple<int, float, double>;

int
main()
{
    TT t{1, 2.f, 3.};
    lucid::visit(
        1ul,
        [](auto& v) noexcept { v += 1; },
        t,
        lucid::sink);
    int ret = std::get<1>(t) != 3.f;

    const TT t2{4, 5.f, 6.};
    double   v = lucid::visit(
        2ul,
        [](const auto& v) -> double { return v * 10; },
        t2,
        []() { return .0; });
    ret += v != 60.;

    std::tuple<std::array<char, 4>, std::array<int, 4>> ta{
        {'a', 'b', 'c', 'd'},
        {  1,   2,   3,   4}
    };
    auto st = lucid::map_tuple<lucid::steady_tuple>(
        [](auto& a) -> decltype(auto) { return a[1]; },
        ta);
    st.template get<0>() = 'x';
    st.template get<1>() += 5;
    ret += std::get<0>(ta)[1] != 'x';
    ret += std::get<1>(ta)[1] != 7;

    return ret;
}
