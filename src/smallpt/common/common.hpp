// -*- C++ -*-
// common.hpp
//

#pragma once

#include "image.hpp"

#include <common/argparse.hpp>
#include <common/logging.hpp>
#include <common/memory.hpp>
#include <common/string.hpp>
#include <common/threadpool.hpp>

using namespace std::literals;
using namespace std::chrono_literals;

namespace loom
{
using Image = ScanlineImage<float, 4, CacheLineAlignedAllocator<float>>;

struct SharedCounters
{
    std::atomic_uint   progress{};
    std::atomic_size_t ray_count{};
};

struct ProgressReporter
{
    Logger&     logger;
    std::size_t num_pix;

    void
    operator()(std::size_t pix_done) const noexcept
    {
        logger.info(Logger::flush,
                    "Rendering... {}%",
                    static_cast<unsigned>(static_cast<double>(pix_done) /
                                          static_cast<double>(num_pix) * 100));
    }

    void
    operator()() const noexcept
    {
        logger.info(Logger::flush, "Rendering... 100%\n");
    }
};

struct ProgressWatcher
{
    ProgressReporter  reporter;
    std::atomic_uint& progress;

    void
    operator()() const noexcept
    {
        while(progress.load(std::memory_order_relaxed) < reporter.num_pix)
        {
            reporter(progress.load(std::memory_order_relaxed));
            std::this_thread::sleep_for(500ms);
        }
        reporter();
    }
};

auto
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
                                            {400, 400},
                                            read_number<int, true>),
            argparse::flag<'k', "keep-res">("keep image resolution"),
            argparse::option<'s', "stream-size">(
                "stream size",
                "N",
                512ul,
                read_number<std::size_t, true>),
            argparse::flag<'v', "verbose">("verbose logging")},
        {argc, argv});
}

template <typename... Args>
static void
parallel_render(Image& img, Args&&... args) noexcept
{
    parallel_for_each(img, std::forward<Args>(args)...);
}

template <typename Duration>
static double
traversal_rate(const std::size_t rc, const Duration& dur) noexcept
{
    return static_cast<double>(rc / 100ul) /
           static_cast<double>(
               std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                   .count());
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
    logger.info("Average traversal rate {:.2f} MRay/s", traversal_rate(rc, e));
}
} // namespace loom
