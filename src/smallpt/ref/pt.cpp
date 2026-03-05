// -*- C++ -*-
// viewport.cpp
#include "cornell_box.hpp"
#include "hider.hpp"
#include "image.hpp"
#include "rng.hpp"
#include "sampling.hpp"

#include <common/argparse.hpp>
#include <common/functional.hpp>
#include <common/logging.hpp>
#include <common/timer.hpp>
#include <common/tuple.hpp>
#include <smallpt/common/viewport.hpp>

#include <fmt/ranges.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>

using namespace loom;
using namespace std::literals;

using Image   = ScanlineImage<float, 4>;
using Scene   = cornell_box<float>;
using Gallery = Scene::Gallery;
using Camera  = Perspective<float>;
using Vec2f   = Vec2<float>;
using Vec3f   = Vec3<float>;
using Sample  = std::pair<Vec2f, RGB<std::array>>;
using Counter = std::array<std::uint32_t, 2>;

static constexpr std::array<std::uint32_t, 2> key{7598293, 1244013411};

Ray<float>
bias_ray(const Vec3f& p, const Vec3f& d) noexcept
{
    constexpr float bias = 0.001;
    return {p + d * bias, d};
}

float
luminance(const RGB<>& rgb) noexcept
{
    const auto [r, g, b] = rgb;
    return 0.2126f * r + 0.7152f * g + 0.0722 * b;
}

struct Shading
{
    static constexpr unsigned max_depth = 4;

    Counter  c;
    unsigned depth;
    Vec3f    p;
    Vec3f    n;
    Vec3f    i;

    static RGB<>
    radiance(const Ray<float>& ray,
             const Counter&    c,
             unsigned          depth = 0) noexcept
    {
        if(depth > max_depth) return {};

        const auto [prim_id, isect] = std::apply(Hider{ray}, Scene::geometry);

        if(!isect) return {};

        const Vec3f p = ray.origin + ray.dir * isect.t;
        const Vec3f n = visit_at(
            prim_id,
            [&p](const auto& prim) noexcept { return normal(p, prim); },
            Scene::geometry,
            constant(Vec3f{0, 1, 0}));

        return visit_at(Gallery::mat_idxs[prim_id],
                        Shading{c, depth, p, n, ray.dir},
                        Gallery::materials,
                        constant(RGB{1, 0, 1}));
    }

    RGB<>
    operator()(const Gallery::Emission& e) const noexcept
    {
        return e.color;
    }

    RGB<>
    operator()(const Gallery::Lambert& mat) const noexcept
    {
        const Vec3f d =
            sample_hemisphere(n, sample_u(threefry2x32r32<key[0], key[1]>(c)));
        const float r = dot(n, d);
        return mat.color *
               radiance(bias_ray(p, d), bump_ctr(c, 641833), depth + 1) * r;
    }

    RGB<>
    operator()(const Gallery::Glass& mat) const noexcept
    {
        const Vec3f wi        = -i;
        const float cos_theta = dot(n, wi);
        const bool  into      = cos_theta > 0;
        const Vec3f nfn       = into ? n : -n;
        const float eta       = mat.compute_eta(into);
        const float f         = mat.reflectance(cos_theta);
        const float ddn       = dot(i, nfn);
        const float cos2t     = 1 - eta * eta * (1 - ddn * ddn);

        auto reflection = [&]() noexcept
        {
            return radiance(bias_ray(p, mat.reflect(nfn, i, cos_theta)),
                            bump_ctr(c, 312398),
                            depth + 1);
        };

        if(cos2t < 0) // total internal reflection
            return reflection();

        auto refraction = [&]() noexcept
        {
            return radiance(
                bias_ray(
                    p,
                    normalize(i * eta - n * ((into ? 1 : -1) *
                                             (ddn * eta + std::sqrt(cos2t))))),
                bump_ctr(c, 987523),
                depth + 1);
        };

        return reflection() * f + refraction() * (1.f - f);
    }
};

void
render(Image&                 img,
       const Camera&          cam,
       const float            threshold,
       std::atomic<unsigned>& progress) noexcept
{
    constexpr unsigned min_samples = 64;

    const float pw = static_cast<float>(1) / img.res()[0]; // pixel width

    const Vec2f          res{img.res()};
    ToSampleSpace<float> sample_space(img.res());

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
    auto parse_results = argparse::parse(
        std::tuple{
            argparse::option<'t', "threshold">("adaptive sampling threshold",
                                               "T",
                                               0.0001f,
                                               read_number<float, true>),
            argparse::option<'r', "res", 2>("image resolution",
                                            {"width", "height"},
                                            {1000, 1000},
                                            read_number<int, true>)},
        {argc, argv});
    Logger logger(Logger::INFO);
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

        logger.info("Elapsed time {:.3%M:%S}", e);
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
