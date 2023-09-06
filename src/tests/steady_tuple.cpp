// -*- C++ -*-
// steady_tuple.cpp
#include <utils/steady_tuple.hpp>

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
    int ret = 0;

    lucid::steady_tuple<A, B> t1(lucid::uniform_init, 34, 0.5f);
    const A&                  a1 = t1.template get<0>();
    const B&                  b1 = t1.template get<1>();
    ret += a1.a != b1.a;
    ret += a1.b != b1.b;

    lucid::steady_tuple<A, B> t2(std::piecewise_construct,
                                 std::pair(1, 2.f),
                                 std::pair(3, 4.));
    const A&                  a2 = t2.template get<A>();
    const B&                  b2 = t2.template get<B>();
    ret += a2.a != 1;
    ret += a2.b != 2.f;
    ret += b2.a != 3;
    ret += b2.b != 4.;

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

    lucid::steady_tuple<A, B> t5(A(1, 2ul), B(3l, 4));
    lucid::steady_tuple<A, B> t6(std::move(t5));
    ret += t6.template get<0>().a != 1;
    ret += t6.template get<0>().b != 2ul;
    ret += t6.template get<1>().a != 3l;
    ret += t6.template get<1>().b != 4;

    t5.template get<0>().a = 5;
    t5.template get<0>().b = 6ul;
    t5.template get<1>().a = 7l;
    t5.template get<1>().b = 8;

    t6 = std::move(t5);
    ret += t6.template get<0>().a != 5;
    ret += t6.template get<0>().b != 6ul;
    ret += t6.template get<1>().a != 7l;
    ret += t6.template get<1>().b != 8;

    return ret;
}
