// -*- C++ -*-
// main.hpp
//

#pragma once

#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>
#include <vm/simd.hpp>

#include <fmt/format.h>

namespace loom
{
inline constexpr char osd_help[] =
    "ESC: close\n1: switch method\n2: switch multithreading\n3: lock/unlock "
    "image resolution";

struct EventState
{
    bool  lock_res;
    bool  run_stream  = true;
    bool  run_mp      = true;
    bool  req_restart = false;
    bool  req_resize  = false;
    bool  req_quit    = false;
    Vec2u new_size{500, 500};

    static constexpr Vec2u min_size{128, 128};

    EventState(const bool kr) noexcept : lock_res(kr) {};

    FORBID_COPY(EventState);
    FORBID_MOVE(EventState);

    struct ResizeAction
    {
        bool*    lock_res;
        bool*    req_restart;
        bool*    req_resize;
        Vec2u*   new_size;
        gl::OSD* osd;

        void
        operator()(GLFWwindow* /*window*/, int width, int height) noexcept
        {
            if(!*lock_res)
            {
                *req_restart = true;
                *req_resize  = true;
                *new_size    = Vec2u(width, height);
            }
            osd->resize(width, height);
        }
    };

    struct KeyAction
    {
        bool* run_stream;
        bool* run_mp;
        bool* req_restart;
        bool* req_quit;
        bool* lock_res;

        void
        operator()(GLFWwindow* window, int button, int action, int) noexcept
        {
            if(action == GLFW_PRESS)
            {
                switch(button)
                {
                case GLFW_KEY_ESCAPE:
                {
                    *req_quit = true;
                    glfwSetWindowShouldClose(window, true);
                    return;
                }
                case GLFW_KEY_1:
                {
                    *run_stream  = !*run_stream;
                    *req_restart = true;
                    return;
                }
                case GLFW_KEY_2:
                {
                    *run_mp      = !*run_mp;
                    *req_restart = true;
                    return;
                }
                case GLFW_KEY_3:
                {
                    *lock_res = !*lock_res;
                    return;
                }
                }
            }
        }
    };

    ResizeAction
    get_resize_action(gl::OSD* osd) noexcept
    {
        return {&lock_res, &req_restart, &req_resize, &new_size, osd};
    }

    KeyAction
    get_key_action() noexcept
    {
        return {&run_stream, &run_mp, &req_restart, &req_quit, &lock_res};
    }

    void
    reset() noexcept
    {
        req_restart = false;
        req_resize  = false;
    }

    bool
    req_stop() const noexcept
    {
        return req_restart || req_quit;
    }

    void
    resize(Image& img)
    {
        if(req_resize)
        {
            const Vec2u final_size = max(new_size, min_size);
            img.resize(final_size);
        }
    }
};

struct ViewState
{
    const gl::System::Handle& system;
    gl::Window&               window;
    gl::ImageViewport&        viewport;
    gl::OSD&                  osd;
    Logger&                   logger;
    Image&                    img;
    Vec2u                     old_res;

    ViewState(const gl::System::Handle& h,
              gl::Window&               w,
              gl::ImageViewport&        v,
              gl::OSD&                  o,
              Logger&                   l,
              Image&                    i) noexcept :
        system(h), window(w), viewport(v), osd(o), logger(l), img(i),
        old_res(img.res())
    {
    }

    FORBID_ALL(ViewState);

    void
    update(EventState& estate)
    {
        if(window.active())
        {
            if(all(old_res == img.res()))
                viewport.reload_img(img.data(), img.width(), img.height());
            else
            {
                viewport.load_img(img.data(), img.width(), img.height());
                old_res = img.res();
            }
            viewport.draw();
            osd.draw();
            window.swap_buffers();
            system.poll();
        }
        else
            estate.req_quit = true;
    }

    void
    update_stats(const EventState& estate)
    {
        const std::string new_text =
            fmt::format("Hotkeys:\n{}\nStatistics:\nSIMD: {}\nImage res: {}x{} "
                        "({})\nMethod: "
                        "{}\nMultithreading: {}",
                        osd_help,
                        vm::simd::info::value,
                        img.width(),
                        img.height(),
                        estate.lock_res ? "locked" : "window",
                        estate.run_stream ? "hybrid" : "predicated",
                        estate.run_mp ? "on" : "off");
        osd.update_text(new_text.data());
    }
};

bool
predicated_render(EventState&    estate,
                  ViewState&     vstate,
                  const float    threshold,
                  const unsigned max_depth);

bool
predicated_mp(EventState&    estate,
              ViewState&     vstate,
              const float    threshold,
              const unsigned max_depth);

bool
hybrid_render(EventState&    estate,
              ViewState&     vstate,
              const float    threshold,
              const unsigned max_depth,
              const unsigned stream_size);

bool
hybrid_mp(EventState&                     estate,
          ViewState&                      vstate,
          const float                     threshold,
          const unsigned                  max_depth,
          const unsigned                  stream_size,
          const std::chrono::milliseconds eg);
} // namespace loom
