// -*- C++ -*-
// io.hpp
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

#include "image.hpp"

#include <fmt/ostream.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace loom
{
template <typename T, unsigned NC, typename Alloc>
void
write_ppm(const ScanlineImage<T, NC, Alloc>& img, const fs::path& filename)
{
    std::ofstream os(filename, std::ios_base::out | std::ios_base::binary);
    const auto [w, h] = img.res();
    fmt::println(os, "P6\n{} {}\n255", w, h);

    for(const auto [_, pix]: img)
    {
        const Vector<std::uint8_t, 3, std::array> rgb{
            downsample<std::uint8_t>(pix)};
        for(const std::uint8_t val: rgb) os << val;
    }
}

} // namespace loom
