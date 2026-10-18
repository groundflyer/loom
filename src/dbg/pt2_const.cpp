// -*- C++ -*-
// smallpt.cpp
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

#include "pt_macros.hpp"

#include <smallpt/vm/enc.hpp>
#include <smallpt/vm/pt.hpp>
#include <vm/dbg.hpp>

using namespace std;
using namespace loom;
using namespace vm;
using namespace dbg;
using namespace argparse;

int
main(int argc, char* argv[])
{
    return inspect<
        pt2_const<float, constants<CS(40), CS(0.0001), CS(64), CS(1), CS(5)>>,
        pt_macros>(
        argc,
        argv,
        std::tuple{option<"pp", 2>("pixel position",
                                   {"X", "Y"},
                                   {0, 0},
                                   read_number<unsigned, true>)},
        [](const auto& pr)
        { return std::tuple{encode_pp(Vec2u{pr.template get_opt<"pp">()})}; });
}
