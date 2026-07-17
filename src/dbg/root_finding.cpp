// -*- C++ -*-
// root_finding.cpp

#include <vm/dbg.hpp>

#include <tests/programs.hpp>
#include <tests/root_finding.hpp>

using namespace std;
using namespace loom;
using namespace vm;
using namespace dbg;
using namespace argparse;

template <floating_point T>
using cubic_fz_p = bisection<T, typename constants<T>::errt>;

int
main(int argc, char* argv[])
{
    return inspect<cubic_fz_p<double>,
                   type_sequence<PRETTIFY_TMACRO(double, eval),
                                 PRETTIFY_TMACRO(double, root_condition),
                                 PRETTIFY_TMACRO(double, find_bounds),
                                 PRETTIFY_TMACRO(double, median)>>(
        argc,
        argv,
        std::tuple{option<"input", 4>("input values",
                                      {"a", "b", "c", "d"},
                                      {1., 2., 3., 4.},
                                      read_number<double, true>)},
        [](const auto& pr) { return pr.template get_opt<"input">(); });
}
