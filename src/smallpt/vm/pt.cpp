// -*- C++ -*-
// pt.cpp
#include "pt.hpp"

#include <common/logging.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>
#include <vm/bse.hpp>

using namespace loom;
using namespace vm;

void
render(Image& img) noexcept
{
    bse::function<pt<float>> f;
    const auto [w, h]  = img.res();
    const unsigned res = w << 16 | h;
    for(auto [pos, pix]: img)
    {
        const auto [x, y]       = pos;
        const auto [r, g, b, t] = f(x << 16 | y, res);
        pix                     = RGBA{r, g, b, 1};
    }
}

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        const auto [resx, resy] = parse_results.template get_opt<'r'>();
        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        Image img(Vec2u{resx, resy});

        logger.debug("rendering...");
        render(img);
        logger.debug("making viewport");
        auto viewport = make_viewport(resx, resy, sink, sink, sink);
        logger.debug("render is ready. Loading...");
        viewport.load_img(img.data(), resx, resy);
        logger.debug("loaded image");

        while(viewport.active())
        {
            viewport.draw();
            glfwPollEvents();
        }
    }
    catch(const std::runtime_error& ex)
    {
        logger.critical("{}", ex.what());
        return 1;
    }
    catch(const GLException& ex)
    {
        logger.critical("OpenGL error: {}", ex.what());
        return 1;
    }
    catch(const ReadException& ex)
    {
        logger.critical("Failed to parse number {}", ex.sv);
        return 1;
    }
    return 0;
}
