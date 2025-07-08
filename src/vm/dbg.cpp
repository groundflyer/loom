// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <tests/programs.hpp>
#include <tests/root_finding.hpp>

using namespace std;
using namespace lucid;
using namespace dbg;
using namespace argparse;

template <floating_point T>
using cubic_fz_p = cubic_fz_program<T, typename constants<T>::errt>;

int
main(int argc, char* argv[])
{
#if 0
    return inspect<cubic_fz_p<float>>(
        option<"input", 4>("input values",
                           {"a", "b", "c", "d"},
                           {1.f, 2.f, 3.f, 4.f},
                           read_number<float, true>),
        argc,
        argv);
#endif
    return inspect<fib<uint64_t>>(
        option<"input">("input values", "n", 80, read_number<uint64_t, true>),
        argc,
        argv);
}
