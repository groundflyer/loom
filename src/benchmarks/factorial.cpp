// -*- C++ -*-
// factorial.cpp
#include "bm.hpp"
#include "utils.hpp"

#include <tests/generators.hpp>
#include <tests/programs.hpp>
#include <tests/vm_test_suite_utils.hpp>

using namespace loom;
using namespace vm;

int
main(int argc, char* argv[])
{
    const auto parse_results =
        argparse::parse(make_opts(sample_size_opt<>), {argc, argv});
    Logger            logger      = get_logger(parse_results);
    const std::size_t sample_size = get_sample_size(logger, parse_results);
    const std::size_t num_runs    = get_num_runs(logger, parse_results);
    std::FILE*        file        = get_file(logger, parse_results);
    RandomState       rs;

    log_info(logger, num_runs);

    bm_suite_types(fac_ints{},
                   StandardBenchmarkSuite<fac_test, GM_FAC, ::fn::val_fac_fn>{},
                   sample_size,
                   rs,
                   logger,
                   file,
                   num_runs);

    return 0;
}
