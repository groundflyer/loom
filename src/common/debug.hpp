// -*- C++ -*-
/// @file debug.hpp
/// @brief Helper macros for debugging and compiler-specific attributes.

#pragma once

#if !defined(NDEBUG) || defined(DEBUG)
#include <cassert>
#define ASSERT(CONDITION, MESSAGE) assert((CONDITION) && MESSAGE)
#else
#define ASSERT(CONDITION, MESSAGE)
#endif

#define CHECK_INDEX(idx, range) ASSERT(idx < range, "Index out of range.")

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
} // namespace loom
