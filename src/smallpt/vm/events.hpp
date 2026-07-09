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
    "ESC: close\n"
    "SPACE: switch method\n"
    "T: switch multithreading\n"
    "L: lock/unlock image resolution\n"
    "Q/W: decrease/increase (x10) adaptive sampling threshold\n"
    "A/S: decrease/increase max depth\n"
    "Z/X: decrease/increase (x2) stream size";

struct EventState
{
    Vec2u       new_res;
    std::size_t img_size;
    std::size_t stream_size;
    float       threshold;
    unsigned    max_depth;
    bool        lock_res;
    bool        run_stream    = true;
    bool        run_mp        = true;
    bool        req_restart   = false;
    bool        req_resize    = false;
    bool        req_quit      = false;
    bool        req_stats_upd = true;

    static constexpr Vec2u    min_res{128, 128};
    static constexpr float    min_threshold = 0.0001f;
    static constexpr float    max_threshold = 0.01f;
    static constexpr unsigned min_md        = 2;
    static constexpr unsigned max_md        = 40;
    static constexpr unsigned md_step       = 5;

    EventState(std::size_t is,
               std::size_t ss,
               float       t,
               unsigned    md,
               bool        lr) noexcept :
        img_size(is), stream_size(ss), threshold(t), max_depth(md),
        lock_res(lr) {};

    FORBID_COPY(EventState);
    FORBID_MOVE(EventState);

    struct ResizeAction
    {
        EventState* estate;
        gl::OSD*    osd;

        void
        operator()(GLFWwindow* /*window*/, int width, int height) noexcept
        {
            if(!estate->lock_res)
            {
                estate->req_restart = true;
                estate->req_resize  = true;
                estate->new_res     = Vec2u(width, height);
            }
            osd->resize(width, height);
        }
    };

    struct KeyAction
    {
        EventState* estate;

        void
        operator()(GLFWwindow* window, int button, int action, int) noexcept
        {
            if(action == GLFW_PRESS)
            {
                switch(button)
                {
                case GLFW_KEY_ESCAPE:
                {
                    estate->req_quit = true;
                    glfwSetWindowShouldClose(window, true);
                    return;
                }
                case GLFW_KEY_SPACE:
                {
                    estate->run_stream    = !estate->run_stream;
                    estate->req_restart   = true;
                    estate->req_stats_upd = true;
                    return;
                }
                case GLFW_KEY_T:
                {
                    estate->run_mp        = !estate->run_mp;
                    estate->req_restart   = true;
                    estate->req_stats_upd = true;
                    return;
                }
                case GLFW_KEY_L:
                {
                    estate->lock_res      = !estate->lock_res;
                    estate->req_stats_upd = true;
                    return;
                }
                case GLFW_KEY_Q:
                {
                    const float cand = estate->threshold / 10.f;
                    if(estate->threshold != min_threshold)
                    {
                        if(cand > min_threshold)
                            estate->threshold = cand;
                        else
                            estate->threshold = min_threshold;

                        estate->req_restart   = true;
                        estate->req_stats_upd = true;
                    }
                    return;
                }
                case GLFW_KEY_W:
                {
                    const float cand = estate->threshold * 10.f;
                    if(estate->threshold != max_threshold)
                    {
                        if(cand < max_threshold)
                            estate->threshold = cand;
                        else
                            estate->threshold = max_threshold;

                        estate->req_restart   = true;
                        estate->req_stats_upd = true;
                    }

                    return;
                }
                case GLFW_KEY_A:
                {
                    const unsigned md = estate->max_depth;

                    if(md > min_md)
                    {
                        estate->max_depth =
                            md > md_step ? md - md_step : min_md;
                        estate->req_restart   = true;
                        estate->req_stats_upd = true;
                    }
                    return;
                }
                case GLFW_KEY_S:
                {
                    const unsigned md = estate->max_depth;

                    if(md < max_md)
                    {
                        estate->max_depth     = std::min(md + md_step, max_md);
                        estate->req_restart   = true;
                        estate->req_stats_upd = true;
                    }
                    return;
                }
                case GLFW_KEY_Z:
                {
                    if(estate->run_stream && estate->stream_size > 64ul)
                    {
                        estate->stream_size /= 2ul;
                        estate->req_restart   = true;
                        estate->req_stats_upd = true;
                    }
                    return;
                }
                case GLFW_KEY_X:
                {
                    const std::size_t cand = estate->stream_size * 2ul;
                    if(estate->run_stream &&
                       cand * NUM_CORES_L < estate->img_size)
                    {
                        estate->stream_size   = cand;
                        estate->req_restart   = true;
                        estate->req_stats_upd = true;
                    }
                    return;
                }
                }
            }
        }
    };

    ResizeAction
    get_resize_action(gl::OSD* osd) noexcept
    {
        return {this, osd};
    }

    KeyAction
    get_key_action() noexcept
    {
        return {this};
    }

    void
    reset() noexcept
    {
        req_restart   = false;
        req_resize    = false;
        req_stats_upd = true;
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
            img.resize(max(new_res, min_res));
            img_size = img.size();
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
    float                     threshold;
    unsigned                  max_depth;
    const char*               res_mode;
    const char*               method;
    const char*               mp;
    std::string               ss;
    std::string               nt;

    ViewState(const gl::System::Handle& h,
              gl::Window&               w,
              gl::ImageViewport&        v,
              gl::OSD&                  o,
              Logger&                   l,
              Image&                    i,
              float                     t,
              unsigned                  d) noexcept :
        system(h), window(w), viewport(v), osd(o), logger(l), img(i),
        old_res(img.res()), threshold(t), max_depth(d)
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
    update_stats(EventState& estate, const std::size_t stream_size = 0)
    {
        if(estate.req_stats_upd)
        {
            res_mode = estate.lock_res ? "locked" : "window";
            mp       = estate.run_mp ? "on" : "off";
            method   = estate.run_stream ? "hybrid" : "predicated";
            ss       = estate.run_stream ?
                           fmt::format("Stream size: {}\n", stream_size) :
                           "";
            nt       = estate.run_mp ?
                           fmt::format("Number of threads: {}\n", NUM_CORES_L) :
                           "";

            estate.req_stats_upd = false;
        }
    }

    void
    update_osd(const std::size_t                          pix_done,
               const std::size_t                          ray_count,
               const std::chrono::steady_clock::duration& dur)
    {
        const std::string new_text = fmt::format(
            "HOTKEYS\n{}\n\n"
            "INFO\n"
            "SIMD: {}\n"
            "Image res: {}x{} ({})\n"
            "Method: {}\n"
            "{}"
            "Multithreading: {}\n"
            "{}"
            "Adaptive sampling threshold: {:6>.04f}\n"
            "Max bounce depth: {}\n\n"
            "STATS\n"
            "Progress: {:>3}%\n"
            "Traversal rate: {:0>3.0f} MRay/s",
            osd_help,
            vm::simd::info::value,
            img.width(),
            img.height(),
            res_mode,
            method,
            ss,
            mp,
            nt,
            threshold,
            max_depth,
            static_cast<unsigned>(static_cast<double>(pix_done) /
                                  static_cast<double>(img.size()) * 100),
            static_cast<double>(ray_count / 100ul) /
                static_cast<double>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                        .count()));
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
