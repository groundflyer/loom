// -*- C++ -*-
// common.hpp
//

#pragma once

#include "image.hpp"

#include <common/argparse.hpp>
#include <common/logging.hpp>
#include <common/string.hpp>
#include <common/threadpool.hpp>

using namespace std::literals;

namespace loom
{
using Image = ScanlineImage<float, 4>;

struct SharedCounters
{
    std::atomic_uint   progress{};
    std::atomic_size_t ray_count{};
};

struct ProgressWatcher
{
    Logger&           logger;
    std::atomic_uint& progress;
    std::size_t       num_pix;

    void
    operator()() const noexcept
    {
        while(progress.load(std::memory_order_relaxed) < num_pix)
        {
            logger.info(Logger::flush,
                        "Rendering... {}%",
                        static_cast<unsigned>(static_cast<double>(progress.load(
                                                  std::memory_order_relaxed)) /
                                              static_cast<double>(num_pix) *
                                              100));
            std::this_thread::sleep_for(500ms);
        }
        logger.info(Logger::flush, "Rendering... 100%\n");
    }
};

static auto
parse_args(int argc, char* argv[]) noexcept
{
    return argparse::parse(
        std::tuple{
            argparse::option<'t', "threshold">("adaptive sampling threshold",
                                               "T",
                                               0.0001f,
                                               read_number<float, true>),
            argparse::option<'r', "res", 2>("image resolution",
                                            {"width", "height"},
                                            {1000, 1000},
                                            read_number<int, true>),
            argparse::option<'n', "samples">("number of pixel samples",
                                             "N",
                                             16u,
                                             read_number<unsigned, true>),
            argparse::flag<'v', "verbose">("verbose logging")},
        {argc, argv});
}

template <typename... Args>
static void
parallel_render(Image& img, Args&&... args) noexcept
{
    parallel_for_each(img, std::forward<Args>(args)...);
}

template <typename Dur>
static void
log_stats(Logger& logger, const Dur& e, const std::size_t rc) noexcept
{
#if FMT_VERSION < 110000
    logger.info("Elapsed time {:%M:%S}", e);
#else
    logger.info("Elapsed time {:.3%M:%S}", e);
#endif
    logger.info(
        "Average traversal rate {:.2f} MRay/s",
        static_cast<double>(rc / 100000ul) /
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::seconds>(e).count()));
}
} // namespace loom
