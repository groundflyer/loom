// -*- C++ -*-
// pt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "pt.hpp"

#include <common/logging.hpp>
#include <common/timer.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>

using namespace loom;
using namespace vm;

struct RenderPixelBase
{
    unsigned w;
    unsigned h;
    unsigned res;
    float    threshold;

    std::atomic_uint&   progress;
    std::atomic_size_t& ray_count;

    RenderPixelBase(const Vec2u&        img_res,
                    float               _threshold,
                    std::atomic_uint&   _p,
                    std::atomic_size_t& _rc) noexcept :
        w(img_res[0]), h(img_res[1]), res(w << 16 | h), threshold(_threshold),
        progress(_p), ray_count(_rc)
    {
    }
};

struct RenderPixelBSE : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    template <typename Pixel>
    void
    operator()(Pixel val) const noexcept
    {
        bse::function<pt<float>> f;

        auto [pos, pix] = val;

        const auto [x, y]        = pos;
        const auto [r, g, b, rc] = f(x << 16 | y, res, threshold);
        pix                      = RGBA{r, g, b, 1};

        progress.fetch_add(1, std::memory_order_relaxed);
        ray_count.fetch_add(rc, std::memory_order_relaxed);
    }
};

struct RenderPixelSIMDPredicated : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    static constexpr std::size_t N = simd::native_vector_extent_v<float>;

    static int _FORCE_INLINE
    masked_ray_count(const simd::native_vector<std::uint32_t> vrc,
                     const std::size_t tail_size) noexcept
    {
#if defined(__AVX512F__)
        return _mm512_mask_reduce_add_epi32(static_cast<std::uint16_t>(-1)
                                                << tail_size,
                                            vrc);
#else // AVX or AVX2
        static constexpr std::uint32_t l = static_cast<std::uint32_t>(-1);
        static constexpr simd::native_vector<std::uint32_t> masks[8] = {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {l, 0, 0, 0, 0, 0, 0, 0},
            {l, l, 0, 0, 0, 0, 0, 0},
            {l, l, l, 0, 0, 0, 0, 0},
            {l, l, l, l, 0, 0, 0, 0},
            {l, l, l, l, l, 0, 0, 0},
            {l, l, l, l, l, l, l, 0},
            {l, l, l, l, l, l, l, l}
        };

        return __builtin_reduce_add(vrc & masks[N - tail_size]);
#endif
    }

    template <typename Chunk>
    void
    operator()(Chunk view) const noexcept
    {
        simd::embedded_function<pt<float>> f;

        const simd::native_vector<std::uint32_t> vres = simd::broadcast<N>(res);
        const simd::native_vector<float>   vt = simd::broadcast<N>(threshold);
        simd::native_vector<std::uint32_t> vpp;

        const std::size_t vs = std::ranges::size(view);

        const bool        is_tail   = N != vs;
        const std::size_t tail_size = N - vs;

        if(is_tail)
        {
            for(std::size_t i = 0; i < vs; ++i)
            {
                const auto [pos, _] = view[i];
                const auto [x, y]   = pos;
                vpp[i]              = x << 16 | y;
            }

            // guarantee tail elems to complete with the first element
            for(std::size_t i = tail_size; i < N; ++i) vpp[i] = vpp[0];
        }
        else
        {
            for(std::size_t i = 0; i < N; ++i)
            {
                const auto [x, y] = view[i].first;
                vpp[i]            = x << 16 | y;
            }
        }

        const auto [vr, vg, vb, vrc] = f(vpp, vres, vt);

        for(std::size_t i = 0; i < vs; ++i)
            view[i].second = RGBA{vr[i], vg[i], vb[i], 1};

        progress.fetch_add(vs, std::memory_order_relaxed);
        ray_count.fetch_add(is_tail ? masked_ray_count(vrc, tail_size) :
                                      __builtin_reduce_add(vrc));
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
        constexpr std::size_t ncores = NUM_CORES_P;

        const auto [resx, resy] = parse_results.template get_opt<'r'>();
        const float threshold   = parse_results.template get_opt<'t'>();
        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        const Vec2u res{resx, resy};
        Image       img(res);
        logger.info("Image res: {}x{}; threshold: {}; number of threads: {}",
                    resx,
                    resy,
                    threshold,
                    ncores);

        PADDED_VALUE(SharedCounters, scc);

        logger.debug("rendering...");

        std::thread watcher{
            ProgressWatcher{logger, scc.progress, img.size()}
        };

        ElapsedTimer<> timer;

        parallel_for_each<ncores>(
            ranges::views::chunk(img, RenderPixelSIMDPredicated::N),
            RenderPixelSIMDPredicated(res,
                                      threshold,
                                      scc.progress,
                                      scc.ray_count),
            64,
            AFFINITY::PHYSICAL);
        // parallel_render(
        //     img,
        //     RenderPixelBSE(img, threshold, scc.progress, scc.ray_count));

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
