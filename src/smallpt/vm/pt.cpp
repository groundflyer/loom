// -*- C++ -*-
// pt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "pt.hpp"
#include "utils.hpp"

#include <common/logging.hpp>
#include <common/timer.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/viewport.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/subrange.hpp>

#include <algorithm>

using namespace loom;
using namespace vm;

static std::uint32_t
encode_res(const Vec2u& res) noexcept
{
    const auto [w, h] = res;
    return w << 16 | h;
}

static int FORCE_INLINE
masked_ray_count(const vm::simd::native_vector<std::uint32_t> vrc,
                 const std::size_t tail_size) noexcept
{
#if defined(__AVX512F__)
    return _mm512_mask_reduce_add_epi32(static_cast<std::uint16_t>(-1)
                                            << tail_size,
                                        vrc);
#else // AVX or AVX2
    static constexpr std::size_t   N = vm::simd::native_vector_extent_v<float>;
    static constexpr std::uint32_t l = static_cast<std::uint32_t>(-1);
    static constexpr vm::simd::native_vector<std::uint32_t> masks[8] = {
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

template <typename K>
class OutputPixelConverter
{
    std::size_t& r_rc;

  public:
    OutputPixelConverter(std::size_t& rc) noexcept : r_rc(rc) {}

    RGBA<>
    operator()(const vm::output_type<K>& o) const noexcept
    {
        const auto [r, g, b, rc, _] = o;
        r_rc += rc;
        return RGBA{r, g, b, 1};
    }
};

template <typename K>
class InputEncoder
{
    std::uint32_t m_res;
    float         m_t;

  public:
    InputEncoder(const Vec2u& res, const float t) noexcept :
        m_res(encode_res(res)), m_t(t)
    {
    }

    template <typename Ip>
    input_type<K>
    operator()(const Ip ip) const noexcept
    {
        return {encode_pp(ip.first), m_res, m_t};
    }
};

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
        w(img_res[0]), h(img_res[1]), res(encode_res(img_res)),
        threshold(_threshold), progress(_p), ray_count(_rc)
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

        const auto [r, g, b, rc, _] = f(encode_pp(pos), res, threshold);

        pix = RGBA{r, g, b, 1};

        progress.fetch_add(1, std::memory_order_relaxed);
        ray_count.fetch_add(rc, std::memory_order_relaxed);
    }
};

template <typename K>
struct RenderPixelSIMDPredicated : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    static constexpr std::size_t N = simd::native_vector_extent_v<float>;

    template <typename Chunk>
    void
    operator()(Chunk view) const noexcept
    {
        simd::embedded_function<K> f;

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
                vpp[i]              = encode_pp(pos);
            }

            // guarantee tail elems to complete with the first element
            for(std::size_t i = tail_size; i < N; ++i) vpp[i] = vpp[0];
        }
        else
        {
            for(std::size_t i = 0; i < N; ++i)
                vpp[i] = encode_pp(view[i].first);
        }

        const auto [vr, vg, vb, vrc, _] = f(vpp, vres, vt);

        for(std::size_t i = 0; i < vs; ++i)
            view[i].second = RGBA{vr[i], vg[i], vb[i], 1};

        progress.fetch_add(vs, std::memory_order_relaxed);
        ray_count.fetch_add(is_tail ? masked_ray_count(vrc, tail_size) :
                                      __builtin_reduce_add(vrc),
                            std::memory_order_relaxed);
    }
};

template <typename K>
static std::size_t
function_render(Image& img, const float t)
{
    if(img.size() % simd::native_vector_extent_v<float> != 0)
        throw std::runtime_error("Image size is not aligned to SIMD lane size");

    simd::hybrid_function<K,
                          true,
                          simd::STATIC_MAJOR,
                          std::ratio<1, 16>,
                          std::ratio<1, 16>>
        f(img.size());

    const auto r = f.template operator()<true>(
        std::views::transform(img, InputEncoder<K>(img.res(), t)));

    std::vector<output_type<K>> result_buffer;
    result_buffer.reserve(img.size());
    std::ranges::copy(r, std::back_inserter(result_buffer));
    std::ranges::sort(
        result_buffer,
        std::less<unsigned>{},
        [width = img.width()](const auto& t) noexcept
        { return pixel_offset(decode_pp(std::get<4>(t)), width, 4); });
    std::size_t ret = 0;

    std::ranges::copy(
        std::views::transform(result_buffer, OutputPixelConverter<K>(ret)),
        OutputAdapter<Image::iterator>(img.begin()));

    return ret;
}

template <typename K>
static std::size_t
stream_render(Image& img, const float t, const unsigned stream_size)
{
    if(stream_size % simd::native_vector_extent_v<float> != 0)
        throw std::runtime_error(
            "Stream size is not aligned to SIMD lane size");

    if(img.size() < stream_size)
        throw std::runtime_error("Image size is less than stream size");

    simd::hybrid_stream<K,
                        true,
                        simd::STATIC_MAJOR,
                        std::ratio<1, 2>,
                        std::ratio<1, 16>>
        s(stream_size);

    std::vector<output_type<K>> result_buffer;
    result_buffer.reserve(img.size());
    auto        inserter = std::back_inserter(result_buffer);
    std::size_t ret      = 0;

    auto view = std::views::transform(img, InputEncoder<K>(img.res(), t));
    auto it   = s.set_inputs(view);

    s();
    s.sort();

    while(it != std::ranges::end(view))
    {
        auto exs = s.exchange(ranges::subrange(it, std::ranges::end(view)));
        it += std::ranges::size(exs);
        std::ranges::copy(exs, inserter);
        s();
        s.sort();
    }

    while(!s.done())
    {
        s();
        s.sort();
    }

    std::ranges::copy(s.get_output_zipped(), inserter);

    std::ranges::sort(
        result_buffer,
        std::less<unsigned>{},
        [width = img.width()](const auto& t) noexcept
        { return pixel_offset(decode_pp(std::get<4>(t)), width, 4); });

    std::ranges::copy(
        std::views::transform(result_buffer, OutputPixelConverter<K>(ret)),
        OutputAdapter<Image::iterator>(img.begin()));

    return ret;
}

using smallpt = pt<float>;
// using smallpt_primary = primary<float>;

int
main(int argc, char* argv[])
{
    auto   parse_results = parse_args(argc, argv);
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    try
    {
        constexpr std::size_t ncores = NUM_CORES_L;

        const auto [resx, resy] = parse_results.template get_opt<'r'>();
        const float threshold   = parse_results.template get_opt<'t'>();
        if(resx < 0 || resy < 0)
            throw std::runtime_error("Negative resolution");

        if(threshold < 0)
            throw std::runtime_error("Negative sampling threshold");

        const Vec2u res{resx, resy};
        Image       img(res);
        logger.info("Image res: {}x{}; threshold: {}; number of threads: {}",
                    resx,
                    resy,
                    threshold,
                    ncores);

        PADDED_VALUE(SharedCounters, scc);

        logger.debug("rendering...");

        // std::thread watcher{
        //     ProgressWatcher{logger, scc.progress, img.size()}
        // };

        ElapsedTimer<> timer;

        // using f = RenderPixelSIMDPredicated<smallpt>;
        // parallel_for_each<ncores>(
        // std::ranges::for_each(ranges::views::chunk(img, f::N),
        //                       f(res, threshold, scc.progress,
        //                       scc.ray_count));
        // parallel_render(
        //     img,
        //     RenderPixelBSE(img, threshold, scc.progress, scc.ray_count));

        // const std::size_t rc = function_render<smallpt>(img, threshold);
        const std::size_t rc = stream_render<smallpt>(img, threshold, 288);

        const auto e = timer.elapsed();

        // watcher.join();

        // log_stats(logger, e, scc.ray_count.load(std::memory_order_relaxed));
        log_stats(logger, e, rc);

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
