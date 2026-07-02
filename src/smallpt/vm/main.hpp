// -*- C++ -*-
// main.hpp
//

#pragma once

#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>

namespace loom
{
struct EventState
{
    const bool keep_res;
    bool       run_stream  = true;
    bool       run_mp      = true;
    bool       req_restart = false;
    bool       req_resize  = false;
    bool       req_quit    = false;
    Vec2u      new_size{500, 500};

    static constexpr Vec2u min_size{128, 128};

    EventState(const bool kr) noexcept : keep_res(kr) {};

    FORBID_COPY(EventState);
    FORBID_MOVE(EventState);

    struct ResizeAction
    {
        bool   keep_res;
        bool*  req_restart;
        bool*  req_resize;
        Vec2u* new_size;

        void
        operator()(GLFWwindow*, int width, int height) noexcept
        {
            if(!keep_res)
            {
                *req_restart = true;
                *req_resize  = true;
                *new_size    = Vec2u(width, height);
            }
        }
    };

    struct KeyAction
    {
        bool* run_stream;
        bool* run_mp;
        bool* req_restart;
        bool* req_quit;

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
                case GLFW_KEY_SPACE:
                {
                    *run_stream  = !*run_stream;
                    *req_restart = true;
                    return;
                }
                case GLFW_KEY_M:
                {
                    *run_mp      = !*run_mp;
                    *req_restart = true;
                    return;
                }
                }
            }
        }
    };

    ResizeAction
    get_resize_action() noexcept
    {
        return {keep_res, &req_restart, &req_resize, &new_size};
    }

    KeyAction
    get_key_action() noexcept
    {
        return {&run_stream, &run_mp, &req_restart, &req_quit};
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
