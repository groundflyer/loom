// -*- C++ -*-
// fp_mix.cpp
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

#include "fp_mix.hpp"
#include "bm.hpp"
#include "utils.hpp"

#include <tests/generators.hpp>

using namespace loom;
using namespace vm;

int
main(int argc, char* argv[])
{
    const auto parse_results =
        argparse::parse(make_opts(sample_size_opt<5000000>), {argc, argv});
    Logger            logger      = get_logger(parse_results);
    const std::size_t sample_size = get_sample_size(logger, parse_results);
    const std::size_t num_runs    = get_num_runs(logger, parse_results);
    std::FILE*        file        = get_file(logger, parse_results);
    RandomState       rs{parse_results.template get_opt<'S'>().value()};

    log_info(logger, num_runs);

    bm_suite_types(type_sequence<float, double>{},
                   StandardBenchmarkSuite<fp_mix_k, GM_F3, fp_mix_ref>{},
                   sample_size,
                   rs,
                   logger,
                   file,
                   num_runs);

    return 0;
}
