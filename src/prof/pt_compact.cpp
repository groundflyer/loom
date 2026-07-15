// -*- C++ -*-
// pt_compact.cpp
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
