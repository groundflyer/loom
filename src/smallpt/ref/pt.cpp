// -*- C++ -*-
// viewport.cpp
#include "pt.hpp"

#include <common/functional.hpp>
#include <common/timer.hpp>
#include <common/tuple.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>

#include <fmt/ranges.h>

#include <chrono>
#include <thread>

using namespace loom;

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);
    logger.debug("Let's go");

    try
    {
        const auto [resx, resy] = parse_results.template get_opt<'r'>();
        const float t           = parse_results.template get_opt<'t'>();

        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        if(t < 0) throw std::runtime_error("Negative sampling threshold");

        Image img(Vec2u{resx, resy});
        logger.debug("image ready");
        logger.info("Image res: {}x{}; sampling threshold: {}", resx, resy, t);

        PADDED_VALUE(SharedCounters, scc);

        std::thread watcher{
            ProgressWatcher{logger, scc.progress, img.size()}
        };

        ElapsedTimer<> timer;

        parallel_render(img,
                        RenderPixel(img, t, &scc.progress, &scc.ray_count));
        const auto e = timer.elapsed();
        watcher.join();

        log_stats(logger, e, scc.ray_count.load(std::memory_order_relaxed));

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
