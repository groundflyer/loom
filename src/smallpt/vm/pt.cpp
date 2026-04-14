// -*- C++ -*-
// pt.cpp
#include "pt.hpp"

#include <common/logging.hpp>
#include <common/timer.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>
#include <vm/bse.hpp>

using namespace loom;
using namespace vm;

void
render(Image& img, unsigned ns) noexcept
{
    const auto [w, h]  = img.res();
    const unsigned res = w << 16 | h;
    std::size_t    trc{};
    for(auto [pos, pix]: img)
    {
        bse::function<pt<float>> f;
        const auto [x, y]        = pos;
        const auto [r, g, b, rc] = f(x << 16 | y, res, ns);
        pix                      = RGBA{r, g, b, 1};
        trc += rc;
    }
    fmt::println("total ray count: {}", trc);
}

class RenderPixelBSE
{
    unsigned w;
    unsigned h;
    unsigned res;
    unsigned ns;

    std::atomic_uint&   progress;
    std::atomic_size_t& ray_count;

  public:
    RenderPixelBSE(const Image&        img,
                   unsigned            _ns,
                   std::atomic_uint&   _p,
                   std::atomic_size_t& _rc) noexcept :
        w(img.width()), h(img.height()), res(w << 16 | h), ns(_ns),
        progress(_p), ray_count(_rc)
    {
    }

    template <typename Pixel>
    void
    operator()(Pixel val) const noexcept
    {
        bse::function<pt<float>> f;

        auto [pos, pix] = val;

        const auto [x, y]        = pos;
        const auto [r, g, b, rc] = f(x << 16 | y, res, ns);
        pix                      = RGBA{r, g, b, 1};

        progress.fetch_add(1, std::memory_order_relaxed);
        ray_count.fetch_add(rc, std::memory_order_relaxed);
    }
};

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        const auto [resx, resy] = parse_results.template get_opt<'r'>();
        const unsigned ns       = parse_results.template get_opt<'n'>();
        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        Image img(Vec2u{resx, resy});
        logger.info("Image res: {}x{}; number of samples: {}", resx, resy, ns);

        PADDED_VALUE(SharedCounters, scc);

        logger.debug("rendering...");

        std::thread watcher{
            ProgressWatcher{logger, scc.progress, img.size()}
        };

        ElapsedTimer<> timer;

        parallel_render(img,
                        RenderPixelBSE(img, ns, scc.progress, scc.ray_count));

        const auto e = timer.elapsed();

        watcher.join();

        log_stats(logger, e, scc.ray_count.load(std::memory_order_relaxed));

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
