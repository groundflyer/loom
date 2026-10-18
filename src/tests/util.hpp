// -*- C++ -*-
// util..hpp
//
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


#pragma once

#include <common/argparse.hpp>
#include <common/random.hpp>
#include <common/test.hpp>

namespace loom::test
{
template <unsigned def = 1000>
std::pair<unsigned, unsigned>
parse_test_args(int argc, char* argv[])
{
    const auto pr =
        argparse::parse(std::tuple{argparse::option<'n', "numruns">(
                                       "number of runs for parametric tests",
                                       "N",
                                       def,
                                       read_number<unsigned, allow_throws>),
                                   argparse::option<'s', "seed">(
                                       "random seed",
                                       "S",
                                       0,
                                       read_number<unsigned, allow_throws>)},
                        {argc, argv});

#ifdef __EXCEPTIONS
    return {pr.template get_opt<'n'>(), pr.template get_opt<'s'>()};
#else
    return {pr.template get_opt<'n'>().value(),
            pr.template get_opt<'s'>().value()};
#endif
}

template <unsigned def = 1000>
std::tuple<Track, ParmTestMaker, RandomState>
test_init(int argc, char* argv[])
{
    const auto [nr, seed] = parse_test_args<def>(argc, argv);
    return {Track{}, ParmTestMaker{nr}, RandomState{seed}};
}
} // namespace loom::test
