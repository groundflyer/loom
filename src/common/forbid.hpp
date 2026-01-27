// -*- C++ -*-
/// @file forbid.hpp
/// @brief Convenience macros to delete class constructors and assignment
/// operators.

#pragma once

/// @brief Delete default constructor.
#define FORBID_DEFAULT(CLASS) CLASS() = delete

/// @brief Delete copy constructor and copy-assign operator.
#define FORBID_COPY(CLASS)                   \
    CLASS(const CLASS&)            = delete; \
    CLASS& operator=(const CLASS&) = delete

/// @brief Delete move constructor and move-assign operator.
#define FORBID_MOVE(CLASS)              \
    CLASS(const CLASS&&)      = delete; \
    CLASS& operator=(CLASS&&) = delete

/// @brief Delete default, copy, move constructors and assign operators.
#define FORBID_ALL(CLASS)  \
    FORBID_DEFAULT(CLASS); \
    FORBID_COPY(CLASS);    \
    FORBID_MOVE(CLASS)
