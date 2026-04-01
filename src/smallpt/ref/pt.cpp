// -*- C++ -*-
// viewport.cpp
#include "pt.hpp"

#include <common/functional.hpp>
#include <common/logging.hpp>
#include <common/timer.hpp>
#include <common/tuple.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/image.hpp>
#include <smallpt/common/viewport.hpp>

#include <fmt/ranges.h>

#include <chrono>
#include <thread>

using namespace loom;
using namespace std::literals;

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
        logger.info("Image res: {}x{}; sampling threshold {}", resx, resy, t);

        const unsigned   num_pix = img.size();
        std::atomic_uint progress{};
        std::thread      watcher{
            [&logger, &progress, num_pix]() noexcept
            {
                while(progress.load(std::memory_order_relaxed) < num_pix)
                {
                    logger.info(Logger::flush,
                                "Rendering... {}%",
                                static_cast<unsigned>(
                                    static_cast<double>(progress.load(
                                        std::memory_order_relaxed)) /
                                    static_cast<double>(num_pix) * 100));
                    std::this_thread::sleep_for(500ms);
                }
                logger.info(Logger::flush, "Rendering... 100%\n");
            }};

        ElapsedTimer<> timer;

        parallel_render(img, render_pixel(img, t, &progress));
        const auto e = timer.elapsed();
        watcher.join();

#if FMT_VERSION < 110000
        logger.info("Elapsed time {:%M:%S}", e);
#else
        logger.info("Elapsed time {:.3%M:%S}", e);
#endif
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

    // write_ppm(film.img, "viewport.ppm");

    return 0;
}
