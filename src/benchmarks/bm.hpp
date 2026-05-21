// -*- C++ -*-
// bm.hpp
//

#pragma once

#include <common/logging.hpp>
#include <common/string.hpp>
#include <vm/simd.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <cpuid.h>

namespace loom
{
static std::string
get_cpu_model() noexcept
{
    std::uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);

    std::uint32_t stepping   = eax & 0xf;
    std::uint32_t model      = (eax >> 4) & 0xf;
    std::uint32_t family     = (eax >> 8) & 0xf;
    std::uint32_t ext_model  = (eax >> 16) & 0xf;
    std::uint32_t ext_family = (eax >> 20) & 0xff;

    std::uint32_t display_family =
        (family == 0xf) ? family + ext_family : family;
    std::uint32_t display_model =
        (family == 6 || family == 0xf) ? ((ext_model << 4) | model) : model;

    __cpuid(0, eax, ebx, ecx, edx);

    char vendor[13]{};

    std::memcpy(vendor, &ebx, 4);
    std::memcpy(vendor + 4, &edx, 4);
    std::memcpy(vendor + 8, &ecx, 4);

    return fmt::format("{} fam{} mod{} step{}",
                       rtrim(vendor),
                       display_family,
                       display_model,
                       stepping);
}

#if FMT_VERSION < 110000
template <typename Dur>
static double
to_format(const Dur& dur) noexcept
{
    return static_cast<double>(
               std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                   .count()) /
           1000;
}
#endif

template <typename R>
static constexpr double
ratio2percent() noexcept
{
    return static_cast<double>(R::num) / static_cast<double>(R::den) * 100;
}

template <typename Dur = std::chrono::steady_clock::duration>
using bm_result = std::pair<std::string, Dur>;

template <typename Dur = std::chrono::steady_clock::duration, typename... Bms>
static std::array<bm_result<Dur>, sizeof...(Bms)>
run_benchmarks(Logger& logger, std::FILE* file, Bms... bms)
{
    auto decorate = [&](auto&& bm)
    {
        const std::string i{bm.info()};
        logger.info("Benchmarking {}...", i);
        const Dur dur = bm();
#if FMT_VERSION < 110000
        const auto durs = to_format(dur);
        logger.info("Result {:.3f}s", durs);
        if(file)
            fmt::print(file,
                       "{:%FT%TZ},{},clang-{},{},{},{},{:.3f}\n",
                       std::chrono::floor<std::chrono::seconds>(
                           std::chrono::system_clock::now()),
                       REPO_HASH,
                       rtrim(__clang_version__),
                       get_cpu_model(),
                       vm::simd::info::value,
                       i,
                       durs);
#else
        logger.info("Result {:.3%S}s", dur);
        if(file)
            fmt::println(file,
                         "{:%FT%TZ},{},clang-{},{},{},{},{:.3%S}",
                         std::chrono::floor<std::chrono::seconds>(
                             std::chrono::system_clock::now()),
                         REPO_HASH,
                         rtrim(__clang_version__),
                         get_cpu_model(),
                         vm::simd::info::value,
                         i,
                         dur);
#endif

        return bm_result<Dur>{std::move(i), dur};
    };

    return {decorate(bms)...};
}

template <typename Dur, std::size_t N>
static void
print_results(std::array<bm_result<Dur>, N>&& results) noexcept
{
    std::ranges::sort(results,
                      std::less<std::size_t>{},
                      [](const auto& e) noexcept { return e.second.count(); });

    const Dur fastest_dur = results[0].second;

    std::size_t id_width = 10;

    for(const auto& [id, _]: results) id_width = std::max(id_width, id.size());

    fmt::print("{:<{}} | {:^7} | {:>3}\n",
               "Benchmark",
               id_width,
               "Time",
               "Slowdown");

    fmt::print("{0:-^{1}}{2}{0:-^9}{2}{0:-^9}\n", "", id_width + 1, "\u253c");

    for(const auto& [id, dur]: results)
#if FMT_VERSION < 110000
        fmt::print("{:<{}} | {:>6.03f}s | {:>3.0f}%\n",
                   id,
                   id_width,
                   to_format(dur),
                   (static_cast<double>(dur.count()) /
                    static_cast<double>(fastest_dur.count())) *
                       100);
#else
        fmt::print("{:<{}} | {:>6.3%S}s | {:>3.0f}%\n",
                   id,
                   id_width,
                   dur,
                   (static_cast<double>(dur.count()) /
                    static_cast<double>(fastest_dur.count())) *
                       100);
#endif
}
} // namespace loom
