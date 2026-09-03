// -*- C++ -*-
// root_finding.cpp
#include "bm.hpp"
#include "utils.hpp"

#include <tests/root_finding.hpp>
#include <tests/vm_test_suite_utils.hpp>

using namespace loom;
using namespace vm;

template <std::floating_point T>
struct RFGenMaker
{
    using cnsts = constants<T>;

    static constexpr T lbound = cnsts::lbound;
    static constexpr T rbound = cnsts::rbound;

    auto
    operator()(RandomState& rs) const noexcept
    {
        return [g_f  = rs.make_gen<T>(lbound, rbound),
                g_a1 = [g_b = rs.make_gen<bool>(0.5),
                        g_a = rs.make_gen<T>(1, rbound)]() mutable noexcept
                { return g_a() * (g_b() ? T(-1) : T(1)); }]() mutable noexcept
        { return std::tuple{g_a1(), g_f(), g_f(), g_f()}; };
    }
};

template <typename T>
using RFKernel = bisection<T, typename constants<T>::errt>;

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

    bm_suite_types(
        all_floats{},
        StandardBenchmarkSuite<RFKernel, RFGenMaker, ::fn::val_cubic_fz_fn>{},
        sample_size,
        rs,
        logger,
        file,
        num_runs);

    return 0;
}
