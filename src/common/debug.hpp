// -*- C++ -*-
/// @file debug.hpp
/// @brief Helper macros for debugging and compiler-specific attributes.
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

#include <cassert>

#define ASSERT(CONDITION, MESSAGE) assert((CONDITION) && MESSAGE)

#define CHECK_INDEX(idx, range)    ASSERT(idx < range, "Index out of range.")

#ifdef __clang__
#define _REINIT [[clang::reinitializes]]
#else
#define _REINIT
#endif

#define FORCE_INLINE __attribute__((always_inline, nodebug))

/// @brief Project's main namespace.
namespace loom
{
namespace fn
{
struct forbidden_sink_fn
{
    template <typename... Ts>
    constexpr void
    operator()(Ts&&...) const noexcept
    {
        ASSERT(false, "This sink shouldn't have been called.");
    }
};
} // namespace fn

/// @brief Always asserts if called.
inline constexpr fn::forbidden_sink_fn forbidden_sink;

#ifdef __EXCEPTIONS
inline constexpr bool allow_throws = true;
#define MUSTTAIL
#else
inline constexpr bool allow_throws = false;
#define MUSTTAIL [[clang::musttail]]
#endif
} // namespace loom
