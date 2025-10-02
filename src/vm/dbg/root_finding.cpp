// -*- C++ -*-
// root_finding.cpp

#include <vm/dbg.hpp>

#include <tests/programs.hpp>
#include <tests/root_finding.hpp>

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
    return inspect<cubic_fz_p<float>>(
        option<"input", 4>("input values",
                           {"a", "b", "c", "d"},
                           {1.f, 2.f, 3.f, 4.f},
                           read_number<float, true>),
        argc,
        argv);
}
