// -*- C++ -*-
// tuple.cpp
#include <utils/test.hpp>
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

TESTS_BEGIN

    CHECK("visit modify reference",
          []() noexcept
          {
              TT t{1, 2.f, 3.};
              lucid::visit(
                  1ul,
                  [](auto& v) noexcept { v += 1; },
                  t,
                  lucid::sink);
              return std::get<1>(t) == 3.f;
          })

    CHECK("visit and return",
          []() noexcept
          {
              const TT t2{4, 5.f, 6.};
              double   v = lucid::visit(
                  2ul,
                  [](const auto& v) -> double { return v * 10; },
                  t2,
                  []() { return .0; });
              return v == 60.;
          })

    CHECK("map tuple to steady_tuple",
          []() noexcept
          {
              std::tuple<std::array<char, 4>, std::array<int, 4>> ta{
                  {'a', 'b', 'c', 'd'},
                  {  1,   2,   3,   4}
              };
              auto st = lucid::map_tuple<lucid::steady_tuple>(
                  [](auto& a) -> decltype(auto) { return a[1]; },
                  ta);
              st.template get<0>() = 'x';
              st.template get<1>() += 5;
              return std::get<0>(ta)[1] == 'x' && std::get<1>(ta)[1] == 7;
          })

    CHECK("steady_tuple uniform_init",
          []() noexcept
          {
              lucid::steady_tuple<A, B> st1(lucid::uniform_init, 34, 0.5f);
              const A&                  a1 = st1.template get<0>();
              const B&                  b1 = st1.template get<1>();
              return a1.a == b1.a && a1.b == b1.b;
          })

    CHECK("steady_tuple piecewise_construct",
          []() noexcept
          {
              lucid::steady_tuple<A, B> st2(std::piecewise_construct,
                                            std::pair(1, 2.f),
                                            std::pair(3, 4.));
              const A&                  a2 = st2.template get<A>();
              const B&                  b2 = st2.template get<B>();
              return a2.a == 1 && a2.b == 2.f && b2.a == 3 && b2.b == 4.;
          })

    CHECK("steady_tuple uniform_init piecewise_construct",
          []() noexcept
          {
              lucid::steady_tuple<A, B> st3(lucid::uniform_init,
                                            std::piecewise_construct,
                                            std::pair(34, 0.5f));
              const A&                  a3 = st3.template get<A>();
              const B&                  b3 = st3.template get<B>();
              return a3.a == b3.a && a3.b == b3.b;
          });

    CHECK("steady_tuple of refs",
          []() noexcept
          {
              int                              i1 = 3;
              char                             c1 = 'c';
              lucid::steady_tuple<int&, char&> st4(i1, c1);
              st4.template get<0>() += 5;
              st4.template get<1>() = 'f';
              return i1 == 8 && c1 == 'f';
          })

    lucid::steady_tuple<A, B> st5(A(1, 2ul), B(3l, 4));
    lucid::steady_tuple<A, B> st6(std::move(st5));

    CHECK("steady_tuple move construct",
          [&]() noexcept
          {
              return st6.template get<0>().a == 1 &&
                     st6.template get<0>().b == 2ul &&
                     st6.template get<1>().a == 3l &&
                     st6.template get<1>().b == 4;
          })

    CHECK("steady_tuple move assign",
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
          })

    CHECK("steady_tuple structured binding",
          [&]() noexcept
          {
              auto& [a6, b6] = st6;
              return a6.a == 5 && a6.b == 6ul && b6.a == 7l && b6.b == 8;
          })

TESTS_END
