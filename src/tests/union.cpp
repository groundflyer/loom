// -*- C++ -*-
// union.cpp
#include <utils/union.hpp>

using namespace lucid;
using namespace std;

using Test = Union<bool, int, unsigned, float, double>;

int
main()
{
    int ret = 0;

    Test test;

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

    return ret;
}
