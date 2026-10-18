// -*- C++ -*-
// root_finding.cpp

// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
