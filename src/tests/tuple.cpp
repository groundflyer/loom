// -*- C++ -*-
// tuple.cpp
#include <utils/tuple.hpp>

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
    return ret;
}
