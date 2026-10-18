// -*- C++ -*-
/// @file forbid.hpp
/// @brief Convenience macros to delete class constructors and assignment
/// operators.
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
