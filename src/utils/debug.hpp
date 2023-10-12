// -*- C++ -*-
/// @file debug.hpp
/// @brief Helper macros for debugging and static analysis.

#pragma once

#ifndef NDEBUG
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
