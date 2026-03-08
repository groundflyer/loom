// -*- C++ -*-
// pt.cpp
#include "sampling.hpp"
#include "scene.hpp"

#include <common/logging.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>
#include <vm/bse.hpp>

using namespace loom;
using namespace vm;

// clang-format off

// pix 16/16 x/y
// res 16/16 w/h
template <std::same_as<float> T>
KERNEL(pt, IVAR(pix), IVAR(res))
    IRSHFT(p0, pix, 16),
    IBAND(p1, pix, 0xffff),
    IRSHFT(w, res, 16),
    IBAND(h, res, 0xffff),
    MOV(TVAR(w), IVAR(w)),
    MOV(TVAR(h), IVAR(h)),
    TDIV(r, w, h),
    INSERT_TMACRO(to_sample_space, CS(p0), CS(p1), CS(p0), CS(p1), CS(w), CS(h), CS(r)),
    TMOV_CS(CS(d2), default_cam::DIR2),
    INSERT_TMACRO(gen_ray_dir, CS(d0), CS(d1), CS(p0), CS(p1), CS(tmp0)),
    TMOV_CS(CS(p0), default_cam::POS0),
    TMOV_CS(CS(p1), default_cam::POS1),
    TMOV_CS(CS(p2), default_cam::POS2),
    INSERT_TMACRO(traverse, CS(pid), CS(p), CS(d)),
    INSERT_TMACRO(set_materials, CS(clrr), CS(clrg), CS(clrb), CS(pid))
KERNEL_END(TVAR(clrr), TVAR(clrg), TVAR(clrb), TVAR(t));

// clang-format on

void
render(Image& img) noexcept
{
    bse::function<pt<float>> f;
    const auto [w, h]  = img.res();
    const unsigned res = w << 16 | h;
    for(auto it = img.begin(); it != img.end(); ++it)
    {
        const auto [x, y]       = it.pos();
        const auto [r, g, b, t] = f(x << 16 | y, res);
        *it                     = RGBA{r, g, b, 1};
    }
}

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        const auto [resx, resy] = parse_results.template get_opt<'r'>();
        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        Image img(Vec2u{resx, resy});

        logger.debug("rendering...");
        render(img);
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
    return 0;
}
