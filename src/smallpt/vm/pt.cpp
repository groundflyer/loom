// -*- C++ -*-
// pt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "events.hpp"

#include <vm/simd.hpp>

#include <common/logging.hpp>
#include <common/timer.hpp>

using namespace loom;
using namespace vm;

static void
clear(Image& img) noexcept
{
    for(auto [_, pix]: img) pix = 0;
}

EventState::ResizeAction resize_action;
EventState::KeyAction    key_action;

static void
resize_callback(GLFWwindow* window, int width, int height) noexcept
{
    resize_action(window, width, height);
    gl::post_size(width, height);
}

static void
key_callback(GLFWwindow* window,
             int         key,
             int /*scancode*/,
             int action,
             int mods) noexcept
{
    key_action(window, key, action, mods);
}

static bool
val_stream_size(const std::size_t stream_size,
                const std::size_t img_size) noexcept
{
    return stream_size * NUM_CORES_L < img_size && stream_size > 64;
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
        const bool  lock_res    = parse_results.template get_opt<'l'>();
        float       threshold   = parse_results.template get_opt<'t'>();
        unsigned    max_depth   = parse_results.template get_opt<'d'>();
        std::size_t stream_size =
            align_up(parse_results.template get_opt<'s'>(), 32ul);
        const std::chrono::milliseconds ep{
            parse_results.template get_opt<'p'>()};

        if(threshold < 0)
            throw std::runtime_error("Negative sampling threshold");

        if(stream_size % simd::native_vector_extent_v<float> != 0)
            throw std::runtime_error(
                "Stream size is not aligned to SIMD lane size");

        Image img(Vec2u{resx, resy});

        if(!val_stream_size(stream_size, img.size()))
            throw std::runtime_error("Stream size is too big");

        logger.debug("exchange period: {}ms", ep.count());

        logger.debug("making viewport");

        EventState estate{img.size(),
                          stream_size,
                          threshold,
                          max_depth,
                          lock_res};

        logger.debug("initializing glfw");
        const auto& h = gl::System::init();

        logger.debug("making window");
        gl::Window window(resx,
                          resy,
                          resize_callback,
                          key_callback,
                          "Loom SmallPT Demo");

        logger.debug("OpenGL version: {}; GLSL version: {}",
                     h.get_string(GL_VERSION),
                     h.get_string(GL_SHADING_LANGUAGE_VERSION));

        logger.debug("making viewport");
        gl::ImageViewport viewport{};

        logger.debug("making osd");
        gl::OSD osd(5.f, 5.f, osd_help, resx, resy);

        resize_action = estate.get_resize_action(&osd);
        key_action    = estate.get_key_action();

        viewport.load_img(img.data(), resx, resy);

        ViewState
            vstate{h, window, viewport, osd, logger, img, threshold, max_depth};

        logger.debug("vstate ready");

        while(!estate.req_quit)
        {
            vstate.update_stats(estate, stream_size);
            vstate.update_osd(
                0,
                0,
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    1ms));

            logger.debug("rendering stream?{}, mp?{}",
                         estate.run_stream,
                         estate.run_mp);

            if(estate.run_stream)
            {
                if(estate.run_mp)
                    hybrid_mp(estate,
                              vstate,
                              threshold,
                              max_depth,
                              stream_size,
                              ep);
                else
                    hybrid_render(estate,
                                  vstate,
                                  threshold,
                                  max_depth,
                                  stream_size);
            }
            else
            {
                if(estate.run_mp)
                    predicated_mp(estate, vstate, threshold, max_depth);
                else
                    predicated_render(estate, vstate, threshold, max_depth);
            }

            logger.debug("rendering done. show still image");
            while(!estate.req_stop() && window.active())
            {
                viewport.draw();
                osd.draw();
                window.swap_buffers();
                h.poll();
                std::this_thread::sleep_for(100ms);
            }

            estate.resize(img);
            logger.debug("new image size: {}x{}", img.width(), img.height());

            stream_size = estate.stream_size;
            threshold = vstate.threshold = estate.threshold;
            max_depth = vstate.max_depth = estate.max_depth;

            logger.debug("stream size: {}", stream_size);
            logger.debug("threshold: {}", threshold);
            logger.debug("max depth: {}", max_depth);

            estate.reset();
            clear(img);
        }

        logger.debug("Goodbye");
    }
    catch(const std::runtime_error& ex)
    {
        logger.critical("{}", ex.what());
        return 1;
    }
    catch(const std::length_error& ex)
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
