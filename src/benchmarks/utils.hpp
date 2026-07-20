// -*- C++ -*-
// utils.hpp
//

#pragma once

#include <common/argparse.hpp>
#include <common/logging.hpp>
#include <common/string.hpp>
#include <vm/simd.hpp>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace loom
{
template <vm::simd::ReconvMethod rcnv>
constexpr std::string_view
rcnv_string() noexcept
{
    switch(rcnv)
    {
    case(vm::simd::WARP_FAIR): return "warp-fair";
    case(vm::simd::HYBRID_MAJOR): return "hybrid-major";
    case(vm::simd::WARP_MAJOR): return "warp-major";
    case(vm::simd::HYBRID_FAIR): return "hybrid-fair";
    case(vm::simd::SUCCESSOR_FIRST): return "successor-first";
    case(vm::simd::SUCCESSOR_MAJOR): return "successor-major";
    }
}

template <typename... BMs>
static void
run(argparse::ArgsRange args, BMs&&... bms) noexcept
{
    constexpr std::size_t def_num_runs = 10;

    auto parse_results = argparse::parse(
        std::tuple{
            argparse::flag<'v', "verbose">("verbose logging"),
            argparse::option<'l', "log">("path to benchmark logs", "FILE", ""),
            argparse::option<'n', "num-runs">(
                "number of runs for each benchmark",
                "N",
                def_num_runs,
                read_number<std::size_t, false>)},
        args);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    std::FILE* file = nullptr;

    const std::string_view path = parse_results.template get_opt<'l'>();

    if(!path.empty())
    {
        file = std::fopen(path.data(), "a+");
        if(!file)
            logger.error("Failed to open {} for read/write: {}",
                         path,
                         strerror(errno));
        else
            logger.info("Logging results into {}", path);
    }

    const std::optional<std::size_t> onr =
        parse_results.template get_opt<'n'>();

    if(!onr)
        logger.warning("Failed to parse num-runs. Falling to default {}",
                       def_num_runs);

    const std::size_t num_runs = onr.value_or(def_num_runs);

    logger.info("CPU: {}; SIMD: {}", get_cpu_model(), vm::simd::info::value);
    logger.info("clang-{}; fast-math: {}", rtrim(__clang_version__), fast_math);
    logger.info("Running each benchmark {} times", num_runs);

    print_results(run_benchmarks(logger, file, num_runs, bms...));

    if(file) std::fclose(file);
}
} // namespace loom
