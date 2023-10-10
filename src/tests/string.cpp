// -*- C++ -*-
// char.cpp
#include <utils/string.hpp>

using namespace lucid;

#define TEST_s2i(T, BASE, VAL) \
    static_assert(s2i<T, BASE>(#VAL).value() == static_cast<T>(VAL))

#define FAIL_s2i(VAL) static_assert(!s2i<unsigned>(VAL).has_value())

#define TEST_s2f(T, VAL) \
    static_assert(s2f<T>(#VAL).value() == static_cast<T>(VAL))

#define FAIL_s2f(VAL) static_assert(!s2f<float>(VAL).has_value())

int
main()
{
    TEST_s2i(int, 10, 98763);
    TEST_s2i(int, 10, -10034);
    TEST_s2i(unsigned, 16, 0xdeadbeef);
    TEST_s2i(unsigned, 2, 0b11101101);
    TEST_s2i(int, 10, -2345);

    FAIL_s2i("e334g");
    FAIL_s2i("-");
    FAIL_s2i("+");
    FAIL_s2i("0b");
    FAIL_s2i("0x");

    TEST_s2f(float, 1);
    TEST_s2f(float, 1234.567);
    TEST_s2f(float, 1e2);
    TEST_s2f(float, 0.5e2);
    TEST_s2f(float, 0.5e-2);
    TEST_s2f(double, -1234.567);
    TEST_s2f(double, -1e2);
    TEST_s2f(double, -0.5e2);
    TEST_s2f(double, -0.5e-2);
    TEST_s2f(double, -2430.5e4);

    FAIL_s2f("e334g");
    FAIL_s2f("-");
    FAIL_s2f("+");

    return 0;
}
