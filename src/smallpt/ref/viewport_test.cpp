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

#include <fmt/ranges.h>

#include <algorithm>

using namespace loom;
using namespace std::literals;

using Image     = ScanlineImage<float, 4>;
using Scene     = cornell_box<float>;
using MatGetter = Scene::MatGetter;
using Camera    = Perspective<float>;
using Vec2f     = Vec2<float>;
using Sample    = std::pair<Vec2<float>, RGB<std::array>>;

constexpr inline MatGetter mat_getter{};

void
render(Image& img, const Camera& cam) noexcept
{
    const Vec2f          res{img.res()};
    ToSampleSpace<float> sample_space(img.res());
    for(auto it = img.begin(); it != img.end(); ++it)
    {
        const Vec2f        ndc = sample_space(it.pos());
        const Ray<float>   ray = cam(ndc);
        const Hider<float> hider{ray};
        const auto [prim_id, isect] = std::apply(hider, Scene::geometry);
        const auto [bsdf, emit_f]   = mat_getter(prim_id);

        *it = RGBA{bsdf.color + emit_f.color};
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
main()
{
    Logger logger(Logger::DEBUG);
    logger.debug("Let's go");

    const int resx = 1000;
    const int resy = 1000;

    Image img(Vec2u{resx, resy});
    logger.debug("image ready");
    const Camera cam = Scene::camera();
    // CameraCapture cc{cam};
    // FilmCapture   fc{&film};
    // DummyAction   da{&film.img};
    try
    {
        logger.debug("making viewport");
        auto viewport = make_viewport(resx, resy, sink, sink, sink);

        logger.debug("viewport is ready. Rendering...");
        render(img, cam);
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
