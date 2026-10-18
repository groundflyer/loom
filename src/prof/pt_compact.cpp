// -*- C++ -*-
// pt_compact.cpp
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

#include <common/logging.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/vm/utils.hpp>

#include <range/v3/view/chunk.hpp>

#include <algorithm>

using namespace loom;
using namespace vm;

int main(/*int argc, char* argv[]*/)
{
    Image img(Vec2u{200, 200});

    std::ranges::for_each(
        ranges::views::chunk(img, RenderPixelSIMDPredicated::N),
        [&](auto chunk) noexcept
        { RenderPixelSIMDPredicated{img.res(), 0.1, 20, 10}(chunk); });

    return 0;
}
