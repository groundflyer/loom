// -*- C++ -*-
// tuple.cpp
#include <utils/tuple.hpp>

#include <array>

using TT = std::tuple<int, float, double>;

struct A
{
    int           a;
    unsigned long b;
    A(int _a, unsigned long _b) : a(_a), b(_b) {}
    A(A&& other) : a(std::move(other.a)), b(std::move(other.b)) {}

    A&
    operator=(A&& other)
    {
        a = std::move(other.a);
        b = std::move(other.b);
        return *this;
    }

    A()         = delete;
    A(const A&) = delete;
    A&
    operator=(const A&) = delete;
};

struct B
{
    long          a;
    unsigned char b;
    B(long _a, unsigned char _b) : a(_a), b(_b) {}

    B(B&& other) : a(std::move(other.a)), b(std::move(other.b)) {}

    B&
    operator=(B&& other)
    {
        a = std::move(other.a);
        b = std::move(other.b);
        return *this;
    }

    B()         = delete;
    B(const B&) = delete;
};

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

    lucid::steady_tuple<A, B> st1(lucid::uniform_init, 34, 0.5f);
    const A&                  a1 = st1.template get<0>();
    const B&                  b1 = st1.template get<1>();
    ret += a1.a != b1.a;
    ret += a1.b != b1.b;

    lucid::steady_tuple<A, B> st2(std::piecewise_construct,
                                  std::pair(1, 2.f),
                                  std::pair(3, 4.));
    const A&                  a2 = st2.template get<A>();
    const B&                  b2 = st2.template get<B>();
    ret += a2.a != 1;
    ret += a2.b != 2.f;
    ret += b2.a != 3;
    ret += b2.b != 4.;

    lucid::steady_tuple<A, B> st3(lucid::uniform_init,
                                  std::piecewise_construct,
                                  std::pair(34, 0.5f));
    const A&                  a3 = st3.template get<A>();
    const B&                  b3 = st3.template get<B>();
    ret += a3.a != b3.a;
    ret += a3.b != b3.b;

    int                              i1 = 3;
    char                             c1 = 'c';
    lucid::steady_tuple<int&, char&> st4(i1, c1);
    st4.template get<0>() += 5;
    st4.template get<1>() = 'f';
    ret += i1 != 8;
    ret += c1 != 'f';

    lucid::steady_tuple<A, B> st5(A(1, 2ul), B(3l, 4));
    lucid::steady_tuple<A, B> st6(std::move(st5));
    ret += st6.template get<0>().a != 1;
    ret += st6.template get<0>().b != 2ul;
    ret += st6.template get<1>().a != 3l;
    ret += st6.template get<1>().b != 4;

    st5.template get<0>().a = 5;
    st5.template get<0>().b = 6ul;
    st5.template get<1>().a = 7l;
    st5.template get<1>().b = 8;

    st6 = std::move(st5);
    ret += st6.template get<0>().a != 5;
    ret += st6.template get<0>().b != 6ul;
    ret += st6.template get<1>().a != 7l;
    ret += st6.template get<1>().b != 8;

    auto& [a6, b6] = st6;
    ret += a6.a != 5;
    ret += a6.b != 6ul;
    ret += b6.a != 7l;
    ret += b6.b != 8;

    return ret;
}
