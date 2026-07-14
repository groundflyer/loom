// -*- C++ -*-
// events.hpp
//

#pragma once

#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>

#include <string>

namespace loom
{
inline constexpr std::size_t               ncores = NUM_CORES_L;
inline constexpr std::chrono::milliseconds osd_timeframe{500};

inline constexpr char osd_help[] =
    "ESC: close\n"
    "SPACE: switch method\n"
    "T: switch multithreading\n"
    "L: lock/unlock image resolution\n"
    "1/2: decrease/increase (x2) blur\n"
    "Q/W: decrease/increase (x10) adaptive sampling threshold\n"
    "A/S: decrease/increase (5) max depth\n"
    "Z/X: decrease/increase (x2) stream size";

enum METHOD : unsigned
{
    REFERENCE,
    PREDICATED,
    HYBRID
};

struct EventState
{
    Vec2u       new_res;
    std::size_t img_size;
    std::size_t stream_size;
    float       threshold;
    unsigned    max_depth;
    float       blur;
    bool        lock_res;
    METHOD      method        = HYBRID;
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
    std::chrono::milliseconds tf;

    ViewState(const gl::System::Handle& h,
              gl::Window&               w,
              gl::ImageViewport&        v,
              gl::OSD&                  o,
              Logger&                   l,
              Image&                    i,
              float                     t,
              unsigned                  d,
              float                     b,
              std::chrono::milliseconds f) noexcept;

    FORBID_ALL(ViewState);

    void
    update(EventState& estate);

    void
    update_stats(EventState& estate);

    void
    update_osd(const std::size_t pix_done, const float tr, const float avg_tr);
};

template <typename Dur>
static float
ray_traversal_rate(const std::size_t ray_count, const Dur dur) noexcept
{
    return static_cast<double>(ray_count / 100ul) /
           static_cast<double>(
               std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                   .count());
}
} // namespace loom
