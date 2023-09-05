// -*- C++ -*-
// steady_tuple.cpp
#include <utils/steady_tuple.hpp>

struct A
{
    int   a;
    float b;
    A(int _a, float _b) : a(_a), b(_b) {}

    A()         = delete;
    A(const A&) = delete;
    A(A&&)      = delete;
    A&
    operator=(const A&) = delete;
    A&
    operator=(A&&) = delete;
};

struct B
{
    int   a;
    float b;
    B(int _a, float _b) : a(_a), b(_b) {}

    B()         = delete;
    B(const B&) = delete;
    B(B&&)      = delete;
    B&
    operator=(const B&) = delete;
    B&
    operator=(B&&) = delete;
};

int
main()
{
    int ret = 0;

    lucid::steady_tuple<A, B> t1(lucid::uniform_init, 34, 0.5f);
    const A&                  a1 = t1.template get<0>();
    const B&                  b1 = t1.template get<1>();
    ret += a1.a != b1.a;
    ret += a1.b != b1.b;

    lucid::steady_tuple<A, B> t2(std::piecewise_construct,
                                 std::pair(1, 2.f),
                                 std::pair(3, 4.f));
    const A&                  a2 = t2.template get<A>();
    const B&                  b2 = t2.template get<B>();
    ret += a2.a != 1;
    ret += a2.b != 2.f;
    ret += b2.a != 3;
    ret += b2.b != 4.f;

    lucid::steady_tuple<A, B> t3(lucid::uniform_init,
                                 std::piecewise_construct,
                                 std::pair(34, 0.5f));
    const A&                  a3 = t3.template get<A>();
    const B&                  b3 = t3.template get<B>();
    ret += a3.a != b3.a;
    ret += a3.b != b3.b;

    int                              i1 = 3;
    char                             c1 = 'c';
    lucid::steady_tuple<int&, char&> t4(i1, c1);
    t4.template get<0>() += 5;
    t4.template get<1>() = 'f';
    ret += i1 != 8;
    ret += c1 != 'f';

    return ret;
}
