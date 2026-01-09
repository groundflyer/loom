// -*- C++ -*-
// viewport.cpp
#include "cornell_box.hpp"
#include "hider.hpp"
#include "image.hpp"
#include "sampling.hpp"

#include <smallpt/common/viewport.hpp>
#include <utils/functional.hpp>
#include <utils/logging.hpp>
#include <utils/timer.hpp>
#include <utils/tuple.hpp>

#include <fmt/ranges.h>

#include <algorithm>
#include <random>

using namespace loom;
using namespace std::literals;

using Image   = ScanlineImage<float, 4>;
using Scene   = cornell_box<float>;
using Gallery = Scene::Gallery;
using Camera  = Perspective<float>;
using Vec2f   = Vec2<float>;
using Vec3f   = Vec3<float>;
using Sample  = std::pair<Vec2<float>, RGB<std::array>>;

Ray<float>
bias_ray(const Vec3f& p, const Vec3f& d) noexcept
{
    constexpr float bias = 0.001;
    return {p + d * bias, d};
}

struct Shading
{
    Vec3f                       p;
    Vec3f                       n;
    Vec3f                       wi;
    std::default_random_engine& g;

    std::pair<float, float>
    sample_light() const noexcept
    {
        const Vec3f lp  = sample(sample_u<float>(g), Scene::light);
        const Vec3f ld  = normalize(lp - p);
        const float dnl = dot(ld, n);
        if(dnl > 0)
        {
            const auto [_, isect] =
                std::apply(Hider{bias_ray(p, ld)}, Scene::shadow_set);
            return {static_cast<float>(!isect.intersect), dnl};
        }
        else
            return {0, dnl};
    }

    static RGBA<>
    radiance(const Ray<float>& ray, std::default_random_engine& g) noexcept
    {
        const auto [prim_id, isect] = std::apply(Hider{ray}, Scene::geometry);

        if(!isect) return {};

        const Vec3f p = ray.origin + ray.dir * isect.t;
        const Vec3f n = visit_at(
            prim_id,
            [&p](const auto& prim) noexcept { return normal(p, prim); },
            Scene::geometry,
            constant(Vec3f{0, 1, 0}));

        return visit_at(Gallery::mat_idxs[prim_id],
                        Shading{p, n, -ray.dir, g},
                        Gallery::materials,
                        constant(RGBA{1, 0, 1}));
    }

    RGBA<>
    operator()(const Gallery::Emission& e) const noexcept
    {
        return RGBA{e.color};
    }

    RGBA<>
    operator()(const Gallery::Lambert& mat) const noexcept
    {
        const auto [s, dnl] = sample_light();
        return RGBA{mat.color * dnl * s * Scene::light_intensity};
    }

    RGBA<>
    operator()(const Gallery::Glass& mat) const noexcept
    {
        const float dvn = dot(n, wi);
        const Vec3f d   = mat.sample(n, wi, dvn);
        return radiance(bias_ray(p, d), g) * mat.reflectance(dvn);
    }
};

void
render(Image& img, const Camera& cam, std::default_random_engine& g) noexcept
{
    const Vec2f          res{img.res()};
    ToSampleSpace<float> sample_space(img.res());
    for(auto it = img.begin(); it != img.end(); ++it)
        *it = Shading::radiance(cam(sample_space(it.pos())), g);
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
main()
{
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

        logger.debug("viewport is ready. Rendering...");
        render(img, cam, g);
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

    // write_ppm(film.img, "viewport.ppm");

    return 0;
}
