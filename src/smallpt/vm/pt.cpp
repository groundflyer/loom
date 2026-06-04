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
#include <vm/mp.hpp>
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

using kernel = pt<float>;

class OutputPixelConverter
{
    std::size_t& r_rc;

  public:
    OutputPixelConverter(std::size_t& rc) noexcept : r_rc(rc) {}

    RGBA<>
    operator()(const vm::output_type<kernel>& o) const noexcept
    {
        const auto [r, g, b, rc, _] = o;
        r_rc += rc;
        return RGBA{r, g, b, 1};
    }
};

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
    input_type<kernel>
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

    RenderPixelBase(const Vec2u& img_res, float _threshold) noexcept :
        w(img_res[0]), h(img_res[1]), res(encode_res(img_res)),
        threshold(_threshold)
    {
    }
};

struct RenderPixelBSE : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    template <typename Pixel>
    std::pair<std::size_t, std::size_t>
    operator()(Pixel val) const noexcept
    {
        bse::function<pt<float>> f;

        auto [pos, pix] = val;

        const auto [r, g, b, rc, _] = f(encode_pp(pos), res, threshold);

        pix = RGBA{r, g, b, 1};

        return {1, rc};
    }
};

struct RenderPixelSIMDPredicated : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    static constexpr std::size_t N = simd::native_vector_extent_v<float>;

    template <typename Chunk>
    std::pair<std::size_t, std::size_t>
    operator()(Chunk view) const noexcept
    {
        simd::embedded_function<kernel> f;

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

        return {vs,
                is_tail ? masked_ray_count(vrc, tail_size) :
                          __builtin_reduce_add(vrc)};
    }
};

enum Method
{
    PREDICATED,
    STREAM
};

struct EventState
{
    Method method      = STREAM;
    bool   mp          = true;
    bool   req_restart = false;
    bool   req_resize  = false;
    bool   req_quit    = false;
    Vec2u  new_size{500, 500};

    static constexpr Vec2u min_size{128, 128};

    EventState() noexcept = default;

    FORBID_COPY(EventState);
    FORBID_MOVE(EventState);

    struct ResizeAction
    {
        bool*  req_restart;
        bool*  req_resize;
        Vec2u* new_size;

        void
        operator()(GLFWwindow*, int width, int height)
        {
            *req_restart = true;
            *req_resize  = true;
            *new_size    = Vec2u(width, height);
        }
    };

    ResizeAction
    make_res_action() noexcept
    {
        return {&req_restart, &req_resize, &new_size};
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
            img.resize(max(new_size, min_size));
            reset();
        }
    }
};

template <typename Viewport>
struct ViewState
{
    Viewport& viewport;
    Logger&   logger;
    Image&    img;
    Vec2u     old_res;

    ViewState(Viewport& v, Logger& l, Image& i) noexcept :
        viewport(v), logger(l), img(i), old_res(img.res())
    {
    }

    FORBID_ALL(ViewState);

    void
    update()
    {
        if(viewport.active())
        {
            logger.debug("reloading image res {}x{}",
                         img.width(),
                         img.height());
            if(all(old_res == img.res()))
                viewport.reload_img(img.data(), img.width(), img.height());
            else
            {
                viewport.load_img(img.data(), img.width(), img.height());
                old_res = img.res();
            }
            viewport.draw();
            glfwPollEvents();
        }
    }
};

template <typename Viewport>
static bool
predicated_render(EventState&          estate,
                  ViewState<Viewport>& vstate,
                  const float          threshold)
{
    ProgressReporter pr{vstate.logger, vstate.img.size()};

    std::size_t progress{}, ray_count{};

    ElapsedTimer<> timer;

    return std::ranges::none_of(
               ranges::views::chunk(vstate.img, RenderPixelSIMDPredicated::N),
               [&](auto chunk)
               {
                   const auto [p, rc] =
                       RenderPixelSIMDPredicated{vstate.img.res(),
                                                 threshold}(chunk);
                   progress += p;
                   ray_count += rc;

                   if(timer.has_expired(15ms))
                   {
                       vstate.update();
                       pr(progress);
                       timer.restart();
                   }

                   return estate.req_stop();
               }) ||
           [&]()
    {
        pr();
        vstate.update();
        return estate.req_stop();
    }();
}

template <std::size_t ncores, typename Viewport>
static bool
predicated_mp(EventState&          estate,
              ViewState<Viewport>& vstate,
              const float          threshold)
{
    struct Shared
    {
        std::atomic_size_t counter{};
        std::atomic_size_t progress{};
        std::atomic_flag   flag{};
    };
    PADDED_VALUE(Shared, scc);

    auto r = ranges::views::chunk(vstate.img, RenderPixelSIMDPredicated::N);

    thread_pool<ncores> tp(
        [&,
         &c        = scc.counter,
         &s        = scc.flag,
         &progress = scc.progress,
         res       = vstate.img.res(),
         is        = vstate.img.size(),
         threshold]()
        {
            constexpr std::size_t cs = 16;

            while(!s.test(std::memory_order_relaxed))
            {
                const std::size_t b =
                    c.fetch_add(cs, std::memory_order_relaxed);

                if(b >= is) return;

                std::ranges::for_each(
                    std::views::counted(std::ranges::begin(r) + b, cs),
                    [res, threshold, &progress](auto view) noexcept
                    {
                        const auto [p, rc] =
                            RenderPixelSIMDPredicated{res, threshold}(view);
                        progress.fetch_add(p, std::memory_order_relaxed);
                    });
            }
        });
    tp.affine(AFFINITY::PHYSICAL);

    while(!estate.req_stop() &&
          scc.progress.load(std::memory_order_relaxed) < vstate.img.size())
    {
        vstate.update();
        std::this_thread::sleep_for(15ms);
    }

    if(estate.req_stop())
    {
        scc.flag.test_and_set(std::memory_order_relaxed);
        return false;
    }

    return true;
}

#if 0
static std::size_t
full_range_render(Image& img, const float t)
{
    if(img.size() % simd::native_vector_extent_v<float> != 0)
        throw std::runtime_error("Image size is not aligned to SIMD lane size");

    simd::hybrid_function<kernel,
                          true,
                          simd::STATIC_MAJOR,
                          std::ratio<1, 16>,
                          std::ratio<1, 16>>
        f(img.size());

    const auto r = f.template operator()<true>(
        std::views::transform(img, InputEncoder(img.res(), t)));

    std::vector<output_type<kernel>> result_buffer;
    result_buffer.reserve(img.size());
    std::ranges::copy(r, std::back_inserter(result_buffer));
    std::ranges::sort(
        result_buffer,
        std::less<unsigned>{},
        [width = img.width()](const auto& t) noexcept
        { return pixel_offset(decode_pp(std::get<4>(t)), width, 4); });
    std::size_t ret = 0;

    std::ranges::copy(
        std::views::transform(result_buffer, OutputPixelConverter(ret)),
        OutputAdapter<Image::iterator>(img.begin()));

    return ret;
}
#endif

template <typename T>
class remote_output_control
{
    T*          p_origin;
    std::size_t m_count = 0;

    class output_iterator : public std::output_iterator_tag
    {
        T*           p_origin;
        std::size_t* p_offset;

      public:
        using value_type      = T;
        using difference_type = std::ptrdiff_t;

        output_iterator() noexcept = default;
        output_iterator(T* origin, std::size_t* offset) noexcept :
            p_origin(origin), p_offset(offset)
        {
        }

        value_type&
        operator*() noexcept
        {
            return *(p_origin + *p_offset);
        }

        output_iterator&
        operator++() noexcept
        {
            ++*p_offset;
            return *this;
        }

        output_iterator
        operator++(int) noexcept
        {
            output_iterator ret(*this);
            ++*p_offset;
            return ret;
        }
    };

  public:
    remote_output_control() noexcept = default;
    remote_output_control(T* data) noexcept : p_origin(data) {}

    output_iterator
    get_output_iterator() noexcept
    {
        return output_iterator(p_origin, &m_count);
    }

    std::size_t
    get_count() const noexcept
    {
        return m_count;
    }

    auto
    release() noexcept
    {
        std::size_t old_count = m_count;
        m_count               = 0;
        return std::views::counted(p_origin, old_count);
    }
};

template <typename T>
static std::size_t
img_upd(Image& img, remote_output_control<T>& roc)
{
    std::size_t ret{};
    std::ranges::for_each(roc.release(),
                          [&](const auto& o) noexcept
                          {
                              const auto [r, g, b, rc, pp] = o;
                              ret += rc;
                              img[decode_pp(pp)] = RGBA{r, g, b, 1};
                          });
    return ret;
}

template <typename Viewport>
static bool
stream_render(EventState&          estate,
              ViewState<Viewport>& vstate,
              const float          threshold,
              const unsigned       stream_size)
{
    if(stream_size % simd::native_vector_extent_v<float> != 0)
        throw std::runtime_error(
            "Stream size is not aligned to SIMD lane size");

    if(vstate.img.size() < stream_size)
        throw std::runtime_error("Image size is less than stream size");

    simd::hybrid_stream<kernel,
                        true,
                        simd::STATIC_MAJOR,
                        std::ratio<1, 16>,
                        std::ratio<1, 16>>
        s(stream_size);

    std::vector<output_type<kernel>> result_buffer(stream_size);
    remote_output_control            roc(result_buffer.data());

    // std::size_t ray_count{}; //, progress{};

    auto view =
        std::views::transform(vstate.img,
                              InputEncoder(vstate.img.res(), threshold));
    auto it = s.set_inputs(view);

    s();
    s.sort();

    ElapsedTimer<> timer;

    while(!estate.req_stop() && it != std::ranges::end(view))
    {
        auto exs = s.exchange(ranges::subrange(it, std::ranges::end(view)));
        const auto se = std::ranges::size(exs);
        it += se;

        std::ranges::copy(exs, roc.get_output_iterator());

        img_upd(vstate.img, roc);

        if(timer.has_expired(15ms))
        {
            vstate.update();
            timer.restart();
        }

        // progress += se;

        s();
        s.sort();
    }

    if(estate.req_stop()) return false;

    while(!estate.req_stop() && !s.done())
    {
        s();
        s.sort();
    }

    if(estate.req_stop()) return false;

    std::ranges::copy(s.get_output_zipped(), roc.get_output_iterator());

    img_upd(vstate.img, roc);

    vstate.update();

    return true;
}

template <typename K>
using mp_stream = simd::hybrid_stream<K,
                                      false,
                                      simd::STATIC_MAJOR,
                                      std::ratio<1, 16>,
                                      std::ratio<1, 16>>;

template <typename K>
using mp_context = mp::Context<K, mp_stream>;

/// @param eg exchange granularity
template <std::size_t ncores,
          typename Viewport,
          typename D = std::chrono::milliseconds>
static bool
stream_mp(EventState&          estate,
          ViewState<Viewport>& vstate,
          const float          threshold,
          const unsigned       stream_size,
          const D              eg = D{2})
{
    if(stream_size % simd::native_vector_extent_v<float> != 0)
        throw std::runtime_error(
            "Stream size is not aligned to SIMD lane size");

    if(vstate.img.size() < stream_size)
        throw std::runtime_error("Image size is less than stream size");

    mp::Shop<type_sequence<kernel>, mp_context, ncores> shop(stream_size);

    std::vector<output_type<kernel>> result_buffer(stream_size * ncores);
    remote_output_control            roc(result_buffer.data());

    auto input_view =
        std::views::transform(vstate.img,
                              InputEncoder(vstate.img.res(), threshold));

    auto oc = std::tuple{roc.get_output_iterator()};

    // std::size_t ret{}, progress{};

    // ProgressReporter pr{logger, img.size()};

    {
        auto ic = shop.bind_inputs(input_view);

        // progress += stream_size;

        auto tp = shop.get_thread_pool();
        tp.affine(AFFINITY::PHYSICAL);

        ElapsedTimer<> timer;

        while(!estate.req_stop() && !mp::is_complete(ic))
        {
            if(shop.exchange(ic, oc))
            {
                // progress += roc.get_count();
                img_upd(vstate.img, roc);
            }

            if(timer.has_expired(15ms))
            {
                vstate.update();
                timer.restart();
            }

            std::this_thread::sleep_for(eg);
        }
        shop.set();
    }

    if(estate.req_stop()) return false;

    shop.finalize(oc);

    img_upd(vstate.img, roc);

    vstate.update();

    return true;
}

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

        logger.debug("making viewport");

        EventState estate;

        auto viewport =
            make_viewport(resx, resy, estate.make_res_action(), sink, sink);

        viewport.load_img(img.data(), resx, resy);

        ViewState vstate{viewport, logger, img};

        logger.debug("vstate ready");

        while(!estate.req_quit)
        {
            // ElapsedTimer<> timer;

            logger.debug("rendering");
            // predicated_render(estate, vstate, threshold);
            // predicated_mp<ncores>(estate, vstate, threshold);
            // stream_render(estate, vstate, threshold, 256);
            stream_mp<ncores>(estate, vstate, threshold, 256, 2ms);

            // const auto e = timer.elapsed();

            // watcher.join();

            // log_stats(logger, e,
            // scc.ray_count.load(std::memory_order_relaxed));
            // log_stats(logger, e, rc);

            logger.debug("rendering done. show still image");
            while(!estate.req_stop() && viewport.active())
            {
                viewport.draw();
                glfwPollEvents();
                std::this_thread::sleep_for(100ms);
            }

            logger.debug("resizing image");
            estate.resize(img);
            logger.debug("new image size: {}x{}", img.width(), img.height());
        }
    }
    catch(const std::runtime_error& ex)
    {
        logger.critical("{}", ex.what());
        return 1;
    }
    catch(const std::length_error& ex)
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
