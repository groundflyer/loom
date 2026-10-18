// -*- C++ -*-
/// @file timer.hpp
/// @brief Easily measure elapsed time.
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

#include <compare>
#include <type_traits>

namespace loom
{
/// @brief Simple timer inspired by QElapsedTimer.
/// @tparam Clock clock type.
template <typename Clock = std::chrono::steady_clock>
class ElapsedTimer
{
    using ClockDuration =
        std::chrono::duration<typename Clock::rep, typename Clock::period>;
    using TimePoint = typename Clock::time_point;

    TimePoint startpoint = Clock::now();

  public:
    ElapsedTimer() {}

    ElapsedTimer(const ElapsedTimer& rhs) : startpoint(rhs.startpoint) {}

    explicit ElapsedTimer(const TimePoint& rhs) : startpoint(rhs) {}

    bool
    operator==(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint == rhs.startpoint;
    }

    std::strong_ordering
    operator<=>(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint <=> rhs.startpoint;
    }

    ClockDuration
    operator-(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint - rhs.startpoint;
    }

    ClockDuration
    elapsed() const noexcept
    {
        return Clock::now() - startpoint;
    }

    ClockDuration
    restart() noexcept
    {
        const ClockDuration ret = elapsed();
        startpoint              = Clock::now();
        return ret;
    }

    template <typename Rep, typename Period>
    bool
    has_expired(
        const std::chrono::duration<Rep, Period>& timeout) const noexcept
    {
        return elapsed() > timeout;
    }
};

template <typename F,
          typename Clock = std::chrono::steady_clock,
          typename... Args>
decltype(auto)
time_it(F&& f, Args&&... args)
{
    ElapsedTimer<Clock> timer;

    if constexpr(std::is_void_v<std::invoke_result_t<F, Args...>>)
    {
        std::invoke(f, args...);

        return timer.elapsed();
    }
    else
        return std::pair{std::invoke(f, args...), timer.elapsed()};
}
} // namespace loom
