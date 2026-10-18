// -*- C++ -*-
// fm.cpp
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

#include "ref.hpp"
#include "../common/render.hpp"
#include "pt.hpp"

namespace loom
{
bool
reference(EventState& estate, ViewState& vstate) noexcept
{
    return scanline<RenderPixel>(estate, vstate);
}

bool
reference_mp(EventState& estate, ViewState& vstate) noexcept
{
    return scanline_mp<RenderPixel>(estate, vstate);
}
} // namespace loom
