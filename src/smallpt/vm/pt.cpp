// -*- C++ -*-
// pt.cpp
#include "events.hpp"
#include "vm.hpp"

#include <smallpt/ref/ref.hpp>
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

struct UnknownMethod
{
    std::string_view sv;
};

int
main(int argc, char* argv[])
{
    auto parse_results = argparse::parse(
        std::tuple{
            argparse::option<'m', "method">(
                "compute method, available options: hybrid, predicated, "
                "scalar, reference",
                "M",
                HYBRID,
                [](std::string_view sv) -> METHOD
                {
                    if(sv == "hybrid") return HYBRID;
                    if(sv == "predicated") return PREDICATED;
                    if(sv == "scalar") return BSE;
                    if(sv == "reference") return REFERENCE;
                    throw UnknownMethod{sv};
                }),
            argparse::option<'t', "threshold">("adaptive sampling threshold",
                                               "T",
                                               0.0001f,
                                               read_number<float, true>),
            argparse::option<'d', "depth">("maximum bounce depth",
                                           "D",
                                           20u,
                                           read_number<unsigned, true>),
            argparse::option<'b', "blur">("blur factor",
                                          "B",
                                          1.f,
                                          read_number<float, true>),
            argparse::option<'r', "res", 2>("image resolution",
                                            {"W", "H"},
                                            {400, 400},
                                            read_number<unsigned, true>),
            argparse::flag<'u', "unlock-res">("unlock image resolution", true),
            argparse::option<'s', "stream-size">(
                "stream size",
                "N",
                512ul,
                read_number<std::size_t, true>),
            argparse::option<'p', "exchange-period">(
                "exchange period in milliseconds for multithreaded hybrid "
                "method. Default: 2",
                "T",
                2,
                read_number<unsigned, true>),
            argparse::option<'f', "fps">("target fps. Default: 10",
                                         "F",
                                         10u,
                                         read_number<unsigned, true>),
            argparse::flag<'v', "verbose">("verbose logging")},
        {argc, argv});

    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        const auto [resx, resy]  = parse_results.template get_opt<'r'>();
        const bool     lock_res  = parse_results.template get_opt<'u'>();
        const float    threshold = parse_results.template get_opt<'t'>();
        const unsigned max_depth = parse_results.template get_opt<'d'>();
        const float    blur      = parse_results.template get_opt<'b'>();
        std::size_t    stream_size =
            align_up(parse_results.template get_opt<'s'>(), 32ul);
        const std::chrono::milliseconds ep{
            parse_results.template get_opt<'p'>()};
        const std::chrono::milliseconds tf{
            1000u / parse_results.template get_opt<'f'>()};

        if(threshold < 0)
            throw std::runtime_error("Negative sampling threshold");

        if(stream_size % simd::native_vector_extent_v<float> != 0)
            throw std::runtime_error(
                "Stream size is not aligned to SIMD lane size");

        if(blur < 1.f || blur > 20.f)
            throw std::runtime_error("Blur B have to be in range [1, 20]");

        Image img(Vec2u{resx, resy});

        if(!val_stream_size(stream_size, img.size()))
            throw std::runtime_error("Stream size is too big");

        logger.debug("exchange period: {}ms", ep.count());

        EventState estate{img.size(),
                          stream_size,
                          threshold,
                          max_depth,
                          blur,
                          lock_res,
                          parse_results.template get_opt<'m'>()};

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

        ViewState vstate{h,
                         window,
                         viewport,
                         osd,
                         logger,
                         img,
                         threshold,
                         max_depth,
                         blur,
                         tf};

        logger.debug("vstate ready");

        logger.debug("image update timeframe: {}", vstate.tf.count());

        while(!estate.req_quit)
        {
            vstate.update_stats(estate);
            vstate.update_osd(img.size(), 0, 0);

            if(estate.run_mp)
            {
                switch(estate.method)
                {
                case HYBRID: hybrid_mp(estate, vstate, ep); break;
                case PREDICATED: predicated_mp(estate, vstate); break;
                case BSE: bse_scanline_mp(estate, vstate); break;
                case REFERENCE: reference_mp(estate, vstate); break;
                }
            }
            else
            {
                switch(estate.method)
                {
                case HYBRID: hybrid(estate, vstate); break;
                case PREDICATED: predicated(estate, vstate); break;
                case BSE: bse_scanline(estate, vstate); break;
                case REFERENCE: reference(estate, vstate); break;
                }
            }

            logger.debug("final reload");
            viewport.reload_img(img.data(), img.width(), img.height());
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

            vstate.threshold = estate.threshold;
            vstate.max_depth = estate.max_depth;
            vstate.blur      = estate.blur;

            logger.debug("stream size: {}", estate.stream_size);
            logger.debug("threshold: {}", estate.threshold);
            logger.debug("max depth: {}", estate.max_depth);
            logger.debug("blur: {}", estate.blur);

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
    catch(const UnknownMethod& ex)
    {
        logger.critical("Unknown method: {}", ex.sv);
        return 1;
    }
    return 0;
}
