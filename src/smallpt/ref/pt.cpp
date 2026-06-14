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
        const auto [resx, resy]  = parse_results.template get_opt<'r'>();
        const float    t         = parse_results.template get_opt<'t'>();
        const unsigned max_depth = parse_results.template get_opt<'d'>();

        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        if(t < 0) throw std::runtime_error("Negative sampling threshold");

        logger.debug("initializing glfw");
        const auto& h = gl::System::init();

        logger.debug("making window");
        auto w =
            gl::create_window(h, resx, resy, sink, sink, "Reference SmallPT");

        logger.debug("OpenGL version: {}; GLSL version: {}",
                     h.get_string(GL_VERSION),
                     h.get_string(GL_SHADING_LANGUAGE_VERSION));

        logger.debug("making viewport");
        auto viewport = gl::ImageViewport();

        logger.debug("making osd");
        auto osd = gl::OSD(10.f, 20.f, "OSD", resx, resy);

        const Vec2u res{resx, resy};
        Image       img(res);
        logger.debug("image ready");
        logger.info("Image res: {}x{}; sampling threshold: {}", resx, resy, t);

        PADDED_VALUE(SharedCounters, scc);

        std::thread watcher{
            ProgressWatcher{ProgressReporter{logger, img.size()},
                            scc.progress}
        };

        ElapsedTimer<> timer;

        parallel_render(
            img,
            RenderPixel(res, t, max_depth, scc.progress, scc.ray_count));
        const auto e = timer.elapsed();
        watcher.join();

        log_stats(logger, e, scc.ray_count.load(std::memory_order_relaxed));

        logger.debug("render is ready. Loading...");
        viewport.load_img(img.data(), resx, resy);
        logger.debug("loaded image");

        while(w.active())
        {
            viewport.draw();
            osd.draw();
            w.swap_buffers();
            h.poll();
            std::this_thread::sleep_for(100ms);
        }
    }
    catch(const std::runtime_error& ex)
    {
        logger.critical("{}", ex.what());
        return 1;
    }
    catch(const gl::GLException& ex)
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
