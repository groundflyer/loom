// -*- C++ -*-
// pt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "main.hpp"

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

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        constexpr std::size_t ncores = NUM_CORES_L;

        const auto [resx, resy]     = parse_results.template get_opt<'r'>();
        const float       threshold = parse_results.template get_opt<'t'>();
        const bool        lock_res  = parse_results.template get_opt<'l'>();
        const unsigned    max_depth = parse_results.template get_opt<'d'>();
        const std::size_t stream_size =
            align_up(parse_results.template get_opt<'s'>(), 32ul);

        if(threshold < 0)
            throw std::runtime_error("Negative sampling threshold");

        Image img(Vec2u{resx, resy});

        if(stream_size * ncores >= img.size())
            throw std::runtime_error("Stream size is too big");

        logger.debug("making viewport");

        EventState estate{lock_res};

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

        ViewState vstate{h, window, viewport, osd, logger, img};

        logger.debug("vstate ready");

        if(stream_size % simd::native_vector_extent_v<float> != 0)
            throw std::runtime_error(
                "Stream size is not aligned to SIMD lane size");

        if(vstate.img.size() < stream_size)
            throw std::runtime_error("Image size is less than stream size");

        while(!estate.req_quit)
        {
            vstate.update_stats(estate);
            // ElapsedTimer<> timer;

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
                              2ms);
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
