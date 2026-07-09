// -*- C++ -*-
// events.hpp
//

#pragma once

#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>

#include <string>

namespace loom
{
inline constexpr char osd_help[] =
    "ESC: close\n"
    "SPACE: switch method\n"
    "T: switch multithreading\n"
    "L: lock/unlock image resolution\n"
    "1/2: decrease/increase (x2) blur\n"
    "Q/W: decrease/increase (x10) adaptive sampling threshold\n"
    "A/S: decrease/increase (5) max depth\n"
    "Z/X: decrease/increase (x2) stream size";

struct EventState
{
    Vec2u       new_res;
    std::size_t img_size;
    std::size_t stream_size;
    float       threshold;
    unsigned    max_depth;
    float       blur;
    bool        lock_res;
    bool        run_stream    = true;
    bool        run_mp        = true;
    bool        req_restart   = false;
    bool        req_resize    = false;
    bool        req_quit      = false;
    bool        req_stats_upd = true;

    EventState(std::size_t is,
               std::size_t ss,
               float       t,
               unsigned    md,
               float       b,
               bool        lr) noexcept;

    FORBID_COPY(EventState);
    FORBID_MOVE(EventState);

    struct ResizeAction
    {
        EventState* estate;
        gl::OSD*    osd;

        void
        operator()(GLFWwindow* /*window*/, int width, int height) noexcept;
    };

    struct KeyAction
    {
        EventState* estate;

        void
        operator()(GLFWwindow* window, int button, int action, int) noexcept;
    };

    ResizeAction
    get_resize_action(gl::OSD* osd) noexcept;

    KeyAction
    get_key_action() noexcept;

    void
    reset() noexcept;

    bool
    req_stop() const noexcept;

    void
    resize(Image& img);
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
    float                     blur;
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
              unsigned                  d,
              float                     b) noexcept;

    FORBID_ALL(ViewState);

    void
    update(EventState& estate);

    void
    update_stats(EventState& estate, const std::size_t stream_size = 0);

    void
    update_osd(const std::size_t pix_done, const float tr, const float avg_tr);
};

bool
predicated_render(EventState& estate, ViewState& vstate);

bool
predicated_mp(EventState& estate, ViewState& vstate);

bool
hybrid_render(EventState& estate, ViewState& vstate);

bool
hybrid_mp(EventState&                     estate,
          ViewState&                      vstate,
          const std::chrono::milliseconds eg);
} // namespace loom
