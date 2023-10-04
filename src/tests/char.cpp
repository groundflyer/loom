// -*- C++ -*-
// char.cpp
#include <utils/char.hpp>

using namespace lucid;

#define TEST_s2i(T, BASE, VAL) \
    static_assert(s2i<T, BASE>(#VAL).value() == static_cast<T>(VAL));

int
main()
{
    TEST_s2i(int, 10, 98763);
    TEST_s2i(int, 10, -10034);
    TEST_s2i(unsigned, 16, 0xdeadbeef);
    TEST_s2i(unsigned, 2, 0b11101101);

    static_assert(!s2i<unsigned>("32kjfd30").has_value());

    return 0;
}
