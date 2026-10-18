// -*- C++ -*-
// vm.hpp
//
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

#include "events.hpp"

namespace loom
{
bool
predicated(EventState& estate, ViewState& vstate);

bool
predicated_mp(EventState& estate, ViewState& vstate);

bool
bse_scanline(EventState& estate, ViewState& vstate);

bool
bse_scanline_mp(EventState& estate, ViewState& vstate);

bool
hybrid(EventState& estate, ViewState& vstate);

bool
hybrid_mp(EventState&                     estate,
          ViewState&                      vstate,
          const std::chrono::milliseconds eg);
} // namespace loom
