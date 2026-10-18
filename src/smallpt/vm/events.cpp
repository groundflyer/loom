// -*- C++ -*-
// events.cpp
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "events.hpp"

#include <vm/simd.hpp>

#include <fmt/format.h>

namespace loom
{
inline constexpr Vec2u    min_res{128, 128};
inline constexpr float    min_threshold = 0.0001f;
inline constexpr float    max_threshold = 0.01f;
inline constexpr float    min_blur      = 1.f;
inline constexpr float    max_blur      = 20.f;
inline constexpr unsigned min_md        = 2;
inline constexpr unsigned max_md        = 40;
inline constexpr unsigned md_step       = 5;

EventState::EventState(std::size_t is,
                       std::size_t ss,
                       float       t,
                       unsigned    md,
                       float       b,
                       bool        lr,
                       METHOD      m,
                       bool        mp) noexcept :
    img_size(is), stream_size(ss), threshold(t), max_depth(md), blur(b),
    lock_res(lr), method(m), run_mp(mp)
{
}

void
EventState::ResizeAction::operator()(GLFWwindow* /*window*/,
                                     int width,
                                     int height) noexcept
{
    if(!estate->lock_res)
    {
        estate->req_restart = true;
        estate->req_resize  = true;
        estate->new_res     = Vec2u(width, height);
    }
    osd->resize(width, height);
}

void
EventState::KeyAction::operator()(GLFWwindow* window,
                                  int         button,
                                  int         action,
                                  int) noexcept
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
            estate->method = static_cast<METHOD>((estate->method + 1u) % 4u);
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
        case GLFW_KEY_A:
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
        case GLFW_KEY_S:
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
        case GLFW_KEY_Q:
        {
            const unsigned md = estate->max_depth;

            if(md > min_md)
            {
                estate->max_depth     = md > md_step ? md - md_step : min_md;
                estate->req_restart   = true;
                estate->req_stats_upd = true;
            }
            return;
        }
        case GLFW_KEY_W:
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
            if(estate->method == HYBRID && estate->stream_size > 64ul)
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
            if(estate->method == HYBRID &&
               cand * std::thread::hardware_concurrency() < estate->img_size)
            {
                estate->stream_size   = cand;
                estate->req_restart   = true;
                estate->req_stats_upd = true;
            }
            return;
        }
        case GLFW_KEY_1:
        {
            const float cand = estate->blur * 0.5f;

            if(estate->blur != min_blur)
            {
                if(cand > min_blur)
                    estate->blur = cand;
                else
                    estate->blur = min_blur;

                estate->req_restart   = true;
                estate->req_stats_upd = true;
            }
            return;
        }
        case GLFW_KEY_2:
        {
            const float cand = estate->blur * 2.f;
            if(estate->blur != max_blur)
            {
                if(cand < max_blur)
                    estate->blur = cand;
                else
                    estate->blur = max_blur;

                estate->req_restart   = true;
                estate->req_stats_upd = true;
            }

            return;
        }
        }
    }
}

EventState::ResizeAction
EventState::get_resize_action(gl::OSD* osd) noexcept
{
    return {this, osd};
}

EventState::KeyAction
EventState::get_key_action() noexcept
{
    return {this};
}

void
EventState::reset() noexcept
{
    req_restart   = false;
    req_resize    = false;
    req_stats_upd = true;
}

bool
EventState::req_stop() const noexcept
{
    return req_restart || req_quit;
}

void
EventState::resize(Image& img)
{
    if(req_resize)
    {
        img.resize(max(new_res, min_res));
        img_size = img.size();
    }
}

ViewState::ViewState(const gl::System::Handle& h,
                     gl::Window&               w,
                     gl::ImageViewport&        v,
                     gl::OSD&                  o,
                     Logger&                   l,
                     Image&                    i,
                     float                     t,
                     unsigned                  d,
                     float                     b,
                     std::chrono::milliseconds f) noexcept :
    system(h), window(w), viewport(v), osd(o), logger(l), img(i),
    old_res(img.res()), threshold(t), max_depth(d), blur(b), tf(f)
{
}

void
ViewState::update(EventState& estate)
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

const char*
method_string(METHOD m)
{
    switch(m)
    {
    case HYBRID: return "hybrid";
    case PREDICATED: return "predicated";
    case BSE: return "scalar";
    case REFERENCE: return "reference";
    }

    return "error";
}

void
ViewState::update_stats(EventState& estate)
{
    if(estate.req_stats_upd)
    {
        res_mode = estate.lock_res ? "locked" : "window";
        mp       = estate.run_mp ? "on" : "off";
        method   = method_string(estate.method);
        ss       = estate.method == HYBRID ?
                       fmt::format("Stream size: {}\n", estate.stream_size) :
                       "";
        nt       = estate.run_mp ? fmt::format("Number of threads: {}\n",
                                         std::thread::hardware_concurrency()) :
                                   "";

        estate.req_stats_upd = false;
    }
}

void
ViewState::update_osd(const std::size_t           pix_done,
                      const float                 tr,
                      const float                 avg_tr,
                      const std::chrono::seconds& t)
{
    const std::string new_text = fmt::format(
        "CONTROL\n{}\n\n"
        "INFO\n"
        "SIMD: {}\n"
        "Image res: {}x{} ({})\n"
        "Method: {}\n"
        "{}"
        "Multithreading: {}\n"
        "{}"
        "Blur: {:1>.00f}\n"
        "Adaptive sampling threshold: {:6>.04f}\n"
        "Max bounce depth: {}\n\n"
        "STATS\n"
        "Progress: {:>3}%\n"
        "Traversal rate:     {:>5.02f} MRay/s\n"
        "Avg Traversal rate: {:>5.02f} MRay/s\n"
        "Render time:        {:%M:%S}",
        osd_help,
        vm::simd::info::value,
        img.width(),
        img.height(),
        res_mode,
        method,
        ss,
        mp,
        nt,
        blur,
        threshold,
        max_depth,
        static_cast<unsigned>(static_cast<double>(pix_done) /
                              static_cast<double>(img.size()) * 100),
        tr,
        avg_tr,
        t);
    osd.update_text(new_text.data());
}
} // namespace loom
