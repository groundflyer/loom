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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

using namespace loom;
using namespace std::literals;

void
render(Image&                 img,
       const Camera&          cam,
       const float            threshold,
       std::atomic<unsigned>& progress) noexcept
{
    constexpr unsigned min_samples = 64;

    ToSampleSpace<float> sample_space(img.res());
    const auto [res, ratio] = sample_space;
    const float pw          = ratio / res[0]; // pixel width

#pragma omp parallel for schedule(dynamic)
    for(auto it = img.begin(); it != img.end(); ++it)
    {
        const Vec2f         pp   = sample_space(it.pos()); // pixel position
        const std::uint32_t c0   = it.pos()[1] * img.res()[0] + it.pos()[0];
        unsigned            ns   = 0;
        float               mean = .0f;
        float               m2   = .0f;

        RGB r{};

        while(ns < min_samples ||
              std::sqrt(m2 / static_cast<float>(ns - 1)) / ns >= threshold)
        {
            const RGB v = Shading::radiance(
                cam(jitter_sample_pos(pw,
                                      pp,
                                      sample_u(threefry2x32r32<key[0], key[1]>(
                                          Counter{c0, ns})))),
                Counter{c0, ns + min_samples + 1001});
            const float x = std::min(luminance(v), 1.f);
            ++ns;
            const float delta = x - mean;
            mean += delta / ns;
            const float delta2 = x - mean;
            m2 += delta * delta2;

            r += v;
        }

        *it = RGBA{r / static_cast<float>(ns), 1};

        progress.fetch_add(1, std::memory_order_relaxed);
    }
}

#if 0
struct FilmCapture
{
    FilmRGBA* film;

    FilmRGBA&
    operator()(const Vec2i& res)
    {
        *film = FilmRGBA(Vec2u(res));
        return *film;
    }

    const Image&
    operator()(const perspective::shoot& cam) noexcept
    {
        *film = render(*film, cam);
        return film->img;
    }
};

struct CameraCapture
{
    perspective::shoot cam;

    Mat4
    get_transform(int key) const noexcept
    {
        auto tt = [&](const Vec3& t) noexcept { return dot(cam.transform, translate(t)); };
        switch(key)
        {
        case GLFW_KEY_UP: return tt(Vec3(0_r, 0.1_r, 0_r));
        case GLFW_KEY_DOWN: return tt(Vec3(0_r, -0.1_r, 0_r));
        }

        return cam.transform;
    }

    perspective::shoot
    operator()(int key, int action, int /*mods*/) noexcept
    {
        if(action == GLFW_PRESS || action == GLFW_REPEAT) cam.transform = get_transform(key);
        return cam;
    }

    const Image&
    operator()(FilmRGBA& film) noexcept
    {
        film = render(film, cam);
        return film.img;
    }
};

struct MouseAction
{
    Vec2 res;
    Vec2 prev_pos{0_r};
    bool pressed = false;

    void
    operator()(int button, bool _pressed, int /* mods */) noexcept
    {
        pressed = (button == GLFW_MOUSE_BUTTON_LEFT) && _pressed;
    }

    void
    operator()(const Vec2& new_pos) noexcept
    {
        if (pressed)
        {
            const Vec2 pix_delta = new_pos - prev_pos;
            const Vec2 ndc_delta = pix_delta / res;
            const Vec2 angle_delta = loom::transform(invert_fov, ndc_delta);
         }
    }
};

struct DummyAction
{
    Image* img;

    template <typename... Args>
    const Image&
    operator()(Args&&...) const noexcept
    {
        return *img;
    }
};
#endif

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);
    logger.debug("Let's go");

    const Camera cam = Scene::camera();
    // CameraCapture cc{cam};
    // FilmCapture   fc{&film};
    // DummyAction   da{&film.img};
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

        const unsigned        num_pix = product(img.res());
        std::atomic<unsigned> progress{};
        std::thread           watcher{
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
        render(img, cam, t, progress);
        const auto e = timer.elapsed();
        watcher.join();

#if FMT_VERSION < 11
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
