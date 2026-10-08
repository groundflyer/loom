// -*- C++ -*-
// io.hpp
//

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
