// -*- C++ -*-

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

#include "generators.hpp"
#include "util.hpp"
#include "vm_test_suite_utils.hpp"

#include <benchmarks/fp_mix.hpp>

#include <array>

#include <fmt/core.h>

using namespace loom;
using namespace vm;
using namespace test;

int
main(int argc, char* argv[])
{
    auto [track, tm, rs] = test_init(argc, argv);

    test_suite_types(all_floats{},
                     RefBSESuite<fp_mix_k, GM_F3, fp_mix_ref, true>{},
                     track,
                     tm,
                     rs);

    return track.ret_code();
}
