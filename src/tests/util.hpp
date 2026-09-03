// -*- C++ -*-
// util..hpp
//

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
