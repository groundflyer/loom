// -*- C++ -*-
// tuple.cpp
#include <utils/common.hpp>
#include <utils/test.hpp>
#include <utils/tuple.hpp>

#include <array>

using TT = std::tuple<int, float, double>;

struct A
{
    int           a;
    unsigned long b;

    FORBID_DEFAULT(A);
    FORBID_COPY(A);

    A(int _a, unsigned long _b) : a(_a), b(_b) {}
    A(A&& other) : a(std::move(other.a)), b(std::move(other.b)) {}

    A&
    operator=(A&& other)
    {
        a = std::move(other.a);
        b = std::move(other.b);
        return *this;
    }
};

struct B
{
    long          a;
    unsigned char b;

    FORBID_DEFAULT(B);
    FORBID_COPY(B);

    B(long _a, unsigned char _b) : a(_a), b(_b) {}

    B(B&& other) : a(std::move(other.a)), b(std::move(other.b)) {}

    B&
    operator=(B&& other)
    {
        a = std::move(other.a);
        b = std::move(other.b);
        return *this;
    }
};

struct C
{
    explicit C(unsigned& i) noexcept { ++i; }
};

struct D
{
    bool value;

    D(bool v) : value(v) {}

    FORBID_DEFAULT(D);
    FORBID_COPY(D);
};

using namespace lucid::test;

int
main()
{
    Track track;

    track.check(NamedTest(
        []() noexcept
        {
            TT t{1, 2.f, 3.};
            lucid::visit_at(
                1ul,
                [](auto& v) noexcept { v += 1; },
                t,
                lucid::sink);
            return std::get<1>(t) == 3.f;
        },
        "visit modify reference"));

    track.check(NamedTest(
        []() noexcept
        {
            const TT t2{4, 5.f, 6.};
            return lucid::visit_at(
                       2ul,
                       [](const auto& v) -> double { return v * 10; },
                       t2,
                       []() { return .0; }) == 60.;
        },
        "visit and return"));

    track.check(NamedTest(
        []() noexcept
        {
            std::tuple<std::array<char, 4>, std::array<int, 4>> ta{
                {'a', 'b', 'c', 'd'},
                {  1,   2,   3,   4}
            };
            auto st = lucid::map_tuple<lucid::unique_tuple>(
                ta,
                [](auto& a) -> decltype(auto) { return a[1]; });
            st.template get<0>() = 'x';
            st.template get<1>() += 5;
            return std::get<0>(ta)[1] == 'x' && std::get<1>(ta)[1] == 7;
        },
        "map tuple to unique_tuple"));

    track.check(NamedTest(
        []() noexcept
        {
            lucid::unique_tuple<A, B> st1(lucid::uniform_init, 34, 0);
            const A&                  a1 = st1.template get<0>();
            const B&                  b1 = st1.template get<1>();
            return a1.a == b1.a && a1.b == b1.b;
        },
        "unique_tuple uniform_init"));

    track.check(NamedTest(
        []() noexcept
        {
            unsigned                     i = 0;
            lucid::unique_tuple<C, C, C> t(lucid::uniform_init, i);
            return i == 3;
        },
        "unique_tuple uniform_init with reference"));

    track.check(NamedTest(
        []() noexcept
        {
            lucid::unique_tuple<A, B> st2(std::piecewise_construct,
                                          std::pair(1, 2),
                                          std::pair(3, 4));
            const A&                  a2 = st2.template get<A>();
            const B&                  b2 = st2.template get<B>();
            return a2.a == 1 && a2.b == 2 && b2.a == 3 && b2.b == 4;
        },
        "unique_tuple piecewise_construct"));

    track.check(NamedTest(
        []() noexcept
        {
            lucid::unique_tuple<A, B> st3(lucid::uniform_init,
                                          std::piecewise_construct,
                                          std::pair(34, 0));
            const A&                  a3 = st3.template get<A>();
            const B&                  b3 = st3.template get<B>();
            return a3.a == b3.a && a3.b == b3.b;
        },
        "unique_tuple uniform_init piecewise_construct"));

    track.check(NamedTest(
        []() noexcept
        {
            int                              i1 = 3;
            char                             c1 = 'c';
            lucid::unique_tuple<int&, char&> st4(i1, c1);
            st4.template get<0>() += 5;
            st4.template get<1>() = 'f';
            return i1 == 8 && c1 == 'f';
        },
        "unique_tuple of refs"));

    lucid::unique_tuple<A, B> st5(A(1, 2ul), B(3l, 4));
    lucid::unique_tuple<A, B> st6(std::move(st5));

    track.check(NamedTest(
        [&]() noexcept
        {
            return st6.template get<0>().a == 1 &&
                   st6.template get<0>().b == 2ul &&
                   st6.template get<1>().a == 3l &&
                   st6.template get<1>().b == 4;
        },
        "unique_tuple move construct"));

    track.check(NamedTest(
        [&]() noexcept
        {
            st5.template get<0>().a = 5;
            st5.template get<0>().b = 6ul;
            st5.template get<1>().a = 7l;
            st5.template get<1>().b = 8;

            st6 = std::move(st5);
            return st6.template get<0>().a == 5 &&
                   st6.template get<0>().b == 6ul &&
                   st6.template get<1>().a == 7l &&
                   st6.template get<1>().b == 8;
        },
        "unique_tuple move assign"));

    track.check(NamedTest(
        [&]() noexcept
        {
            auto& [a6, b6] = st6;
            return a6.a == 5 && a6.b == 6ul && b6.a == 7l && b6.b == 8;
        },
        "unique_tuple structured binding"));

    track.check(NamedTest(
        []() noexcept
        {
            lucid::unique_tuple<D, D> t(false, false);
            lucid::for_each(
                t,
                [](D& e, bool value) noexcept { e.value = value; },
                true);
            const auto& [d1, d2] = t;
            return d1.value && d2.value;
        },
        "unique_tuple for_each"));

    track.check(NamedTest(
        []() noexcept
        {
            TT t1{0, 0, 0};
            TT t2{1, 1.f, 1.};

            lucid::zip_with_for_each(
                t1,
                [](auto& a, auto b) noexcept { a = b; },
                t2);

            return std::get<0>(t1) == std::get<0>(t2) &&
                   std::get<1>(t1) == std::get<1>(t2) &&
                   std::get<2>(t1) == std::get<2>(t2);
        },
        "zip_with_for_each"));

    return track.ret_code();
}
