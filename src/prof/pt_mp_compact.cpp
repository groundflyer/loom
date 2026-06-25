// -*- C++ -*-
// pt_compact.cpp
#include <common/logging.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/vm/utils.hpp>

#include <range/v3/view/chunk.hpp>

using namespace loom;
using namespace vm;

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        const auto [resx, resy]  = parse_results.template get_opt<'r'>();
        const float    threshold = parse_results.template get_opt<'t'>();
        const unsigned max_depth = parse_results.template get_opt<'d'>();

        if(threshold < 0)
            throw std::runtime_error("Negative sampling threshold");

        Image img(Vec2u{resx, resy});

        parallel_for_each(
            ranges::views::chunk(img, RenderPixelSIMDPredicated::N),
            [&](auto chunk) noexcept
            {
                RenderPixelSIMDPredicated{img.res(), threshold, max_depth}(
                    chunk);
            });
    }
    catch(const std::runtime_error& ex)
    {
        logger.critical("{}", ex.what());
        return 1;
    }

    return 0;
}
