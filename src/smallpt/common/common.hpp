// -*- C++ -*-
// common.hpp
//

#pragma once

#include "image.hpp"

#include <common/argparse.hpp>
#include <common/string.hpp>

namespace loom
{
using Image = ScanlineImage<float, 4>;

auto
parse_args(int argc, char* argv[]) noexcept
{
    return argparse::parse(
        std::tuple{
            argparse::option<'t', "threshold">("adaptive sampling threshold",
                                               "T",
                                               0.0001f,
                                               read_number<float, true>),
            argparse::option<'r', "res", 2>("image resolution",
                                            {"width", "height"},
                                            {1000, 1000},
                                            read_number<int, true>)},
        {argc, argv});
}
} // namespace loom
