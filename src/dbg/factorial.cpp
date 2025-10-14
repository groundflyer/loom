// -*- C++ -*-
// fibonacci.cpp

#include <tests/programs.hpp>
#include <tests/root_finding.hpp>
#include <vm/dbg.hpp>

using namespace std;
using namespace lucid;
using namespace vm;
using namespace dbg;
using namespace argparse;

int
main(int argc, char* argv[])
{
    return inspect<factorial_int<uint64_t>>(
        option<"input">("input values", "a", 5, read_number<uint64_t, true>),
        argc,
        argv);
}
