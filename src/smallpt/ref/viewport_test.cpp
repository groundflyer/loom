// -*- C++ -*-
// viewport.cpp
#include "cornell_box.hpp"
#include "hider.hpp"
#include "image.hpp"
#include "sampling.hpp"

#include <smallpt/common/viewport.hpp>
#include <utils/argparse.hpp>
#include <utils/functional.hpp>
#include <utils/logging.hpp>
#include <utils/timer.hpp>
#include <utils/tuple.hpp>

#include <fmt/ranges.h>

#include <algorithm>
#include <random>

using namespace loom;
using namespace std::literals;

using real    = double;
using Image   = ScanlineImage<float, 4>;
using Scene   = cornell_box<real>;
using Gallery = Scene::Gallery;
using Camera  = Perspective<real>;
using Vec2r   = Vec2<real>;
using Vec3r   = Vec3<real>;
using Sample  = std::pair<Vec2r, RGB<std::array>>;

Ray<real>
bias_ray(const Vec3r& p, const Vec3r& d) noexcept
{
    constexpr real bias = 0.001;
    return {p + d * bias, d};
}

struct Shading
{
    static constexpr unsigned max_depth = 4;

    unsigned                    depth;
    Vec3r                       p;
    Vec3r                       n;
    Vec3r                       i;
    std::default_random_engine& g;

    static RGB<>
    radiance(const Ray<real>&            ray,
             std::default_random_engine& g,
             unsigned                    depth = 0) noexcept
    {
        if(depth > max_depth) return {};

        const auto [prim_id, isect] = std::apply(Hider{ray}, Scene::geometry);

        if(!isect) return {};

        const Vec3r p = ray.origin + ray.dir * isect.t;
        const Vec3r n = visit_at(
            prim_id,
            [&p](const auto& prim) noexcept { return normal(p, prim); },
            Scene::geometry,
            constant(Vec3r{0, 1, 0}));

        return visit_at(Gallery::mat_idxs[prim_id],
                        Shading{depth, p, n, ray.dir, g},
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
        const Vec3r d = sample_hemisphere(n, sample_u<real>(g));
        const float r = dot(n, d);
        return mat.color * radiance(bias_ray(p, d), g, depth + 1) * r;
    }

    RGB<>
    operator()(const Gallery::Glass& mat) const noexcept
    {
        const Vec3r wi        = -i;
        const real  cos_theta = dot(n, wi);
        const bool  into      = cos_theta > 0;
        const Vec3r nfn       = into ? n : -n;
        const real  eta       = mat.compute_eta(into);
        const real  f         = mat.reflectance(cos_theta);
        const real  ddn       = dot(i, nfn);
        const real  cos2t     = 1 - eta * eta * (1 - ddn * ddn);

        auto reflection = [&]() noexcept
        {
            return radiance(bias_ray(p, mat.reflect(nfn, i, cos_theta)),
                            g,
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
                g,
                depth + 1);
        };

        return reflection() * static_cast<float>(f) +
               refraction() * static_cast<float>(1 - f);
    }
};

void
render(Image&                      img,
       const Camera&               cam,
       std::default_random_engine& g,
       const unsigned              ns) noexcept
{
    const real pw = static_cast<real>(1) / img.res()[0]; // pixel width
    const real sw = pw / ns;                             // sample width

    const Vec2r         res{img.res()};
    ToSampleSpace<real> sample_space(img.res());
    for(auto it = img.begin(); it != img.end(); ++it)
    {
        const Vec2r pp = sample_space(it.pos()); // pixel position
        RGB         r{};
        for(unsigned sy = 0; sy < ns; ++sy)
            for(unsigned sx = 0; sx < ns; ++sx)
            {
                const Vec2r sp = jitter_sample_pos(
                    g,
                    sw,
                    (pp - pw * static_cast<real>(0.5)) + Vec2r{sx, sy} * sw);
                r += Shading::radiance(cam(sp), g);
            }
        *it = RGBA{r / static_cast<float>(ns * ns), 1};
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
    auto parse_results =
        argparse::parse(std::tuple{argparse::option<'s', "samples">(
                            "Number of horizontal pixel samples",
                            "N",
                            4u,
                            read_number<unsigned, true>)},
                        {argc, argv});
    Logger logger(Logger::DEBUG);
    logger.debug("Let's go");

    const int resx = 1000;
    const int resy = 1000;

    Image img(Vec2u{resx, resy});
    logger.debug("image ready");
    const Camera               cam = Scene::camera();
    std::random_device         r;
    std::default_random_engine g(r());
    // CameraCapture cc{cam};
    // FilmCapture   fc{&film};
    // DummyAction   da{&film.img};
    try
    {
        logger.debug("making viewport");
        auto viewport = make_viewport(resx, resy, sink, sink, sink);

        const unsigned ps = parse_results.template get_opt<'s'>();
        logger.info(fmt::runtime("Rendering with {0}x{0} pixel samples"), ps);
        render(img, cam, g, ps);
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
        logger.critical(fmt::runtime("{}"), ex.what());
        return 1;
    }
    catch(const GLException& ex)
    {
        logger.critical(fmt::runtime("OpenGL error: {}"), ex.what());
        return 1;
    }
    catch(const ReadException& ex)
    {
        logger.critical(fmt::runtime("Failed to parse number {}"), ex.sv);
        return 1;
    }

    // write_ppm(film.img, "viewport.ppm");

    return 0;
}
