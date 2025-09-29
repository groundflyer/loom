// -*- C++ -*-
// dbg.cpp

#include <tests/programs.hpp>
#include <tests/root_finding.hpp>
#include <vm/dbg.hpp>

using namespace std;
using namespace lucid;
using namespace vm;
using namespace dbg;
using namespace argparse;

template <floating_point T>
using cubic_fz_p = cubic_fz_program<T, typename constants<T>::errt>;

int
main(int argc, char* argv[])
{
    return inspect<fib<uint64_t>>(
        option<"input">("input values", "n", 80, read_number<uint64_t, true>),
        argc,
        argv);
}
