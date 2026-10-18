// -*- C++ -*-
// bm.hpp
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

#include <common/logging.hpp>
#include <common/string.hpp>
#include <vm/simd.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
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

template <typename Dur>
static std::string
format_duration(Dur            d,
                const unsigned z,
                const unsigned f,
                const bool     always_mins = false) noexcept
{
    const auto mins = std::chrono::duration_cast<std::chrono::minutes>(d);
    d -= mins;

    if(mins.count() || always_mins)
        return std::format("{:02}:{:0{}.{}f}",
                           mins.count(),
                           std::chrono::duration<double>(d).count(),
                           z,
                           f);
    else
        return std::format("{:0{}.{}f}s",
                           std::chrono::duration<double>(d).count(),
                           z,
                           f);
}

template <typename Dur = std::chrono::steady_clock::duration>
using bm_result = std::pair<std::string, Dur>;

#ifdef __FAST_MATH__
constexpr inline bool fast_math = true;
#else
constexpr inline bool fast_math = false;
#endif

template <typename Dur = std::chrono::steady_clock::duration, typename... Bms>
static std::array<bm_result<Dur>, sizeof...(Bms)>
run_benchmarks(Logger&     logger,
               std::FILE*  file,
               std::size_t num_runs,
               Bms... bms)
{
    auto decorate = [&](auto&& bm)
    {
        const std::string i{bm.info()};
        Dur               avg{};
        for(std::size_t j = 0; j < num_runs; ++j)
        {
            logger.info("Benchmarking {} [{}/{}]...", i, j + 1ul, num_runs);
            const Dur dur = bm();
            avg += dur;
            logger.info("Result {}", format_duration(dur, 6, 3));
            if(file)
                fmt::print(
                    file,
                    "{:%FT%TZ},{},clang-{},fast-math:{},{},{},{},{:09.6f}\n",
                    std::chrono::floor<std::chrono::seconds>(
                        std::chrono::system_clock::now()),
                    REPO_HASH,
                    rtrim(__clang_version__),
                    fast_math,
                    get_cpu_model(),
                    vm::simd::info::value,
                    i,
                    std::chrono::duration<double>(dur).count());
        }

        return bm_result<Dur>{std::move(i), avg / num_runs};
    };

    return {decorate(bms)...};
}

namespace fn
{
template <typename Dur, std::size_t N>
static void
print_results(std::array<bm_result<Dur>, N> results) noexcept
{
    std::ranges::sort(results,
                      std::less<std::size_t>{},
                      [](const auto& e) noexcept { return e.second.count(); });

    const Dur fastest_dur = results.front().second;
    const Dur slowest_dur = results.back().second;

    std::size_t id_width = 10;

    for(const auto& [id, _]: results) id_width = std::max(id_width, id.size());

    const bool has_mins =
        std::chrono::duration_cast<std::chrono::minutes>(slowest_dur).count();
    const unsigned time_width = has_mins ? 9 : 7;
    const unsigned sd_width =
        magnitude(static_cast<unsigned>(static_cast<double>(
            slowest_dur.count() / static_cast<double>(fastest_dur.count())))) +
        8;

    fmt::print("| {:<{}} | {:^{}} | {:>{}} |\n",
               "Benchmark",
               id_width,
               "Time",
               time_width,
               "Slowdown",
               sd_width);

    fmt::print("|:{0:-^{1}}|:{0:-^{2}}:|:{0:-^{3}}:|\n",
               "",
               id_width + 1,
               time_width,
               sd_width);

    for(const auto& [id, dur]: results)
        fmt::print("| {:<{}} | {} | {:>{}.2f}x |\n",
                   id,
                   id_width,
                   format_duration(dur, 6, 3, has_mins),
                   static_cast<double>(dur.count()) /
                       static_cast<double>(fastest_dur.count()),
                   sd_width - 1);
}
} // namespace fn
MK_NIEBLOID(print_results)
} // namespace loom
