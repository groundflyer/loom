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
        if(req_resize) { img.resize(max(new_size, min_size)); }
    }
};

template <typename Window>
struct ViewState
{
    const gl::System::Handle& system;
    Window&                   window;
    gl::ImageViewport&        viewport;
    gl::OSD&                  osd;
    Logger&                   logger;
    Image&                    img;
    Vec2u                     old_res;

    ViewState(const gl::System::Handle& h,
              Window&                   w,
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

template <typename Viewport>
static bool
predicated_render(EventState&          estate,
                  ViewState<Viewport>& vstate,
                  const float          threshold,
                  const unsigned       max_depth)
{
    // ProgressReporter pr{vstate.logger, vstate.img.size()};

    // std::size_t progress{}, ray_count{};

    ElapsedTimer<> timer;

    return std::ranges::none_of(
               ranges::views::chunk(vstate.img, RenderPixelSIMDPredicated::N),
               [&](auto chunk)
               {
                   RenderPixelSIMDPredicated{vstate.img.res(),
                                             threshold,
                                             max_depth}(chunk);
                   // progress += p;
                   // ray_count += rc;

                   if(timer.has_expired(15ms))
                   {
                       vstate.update(estate);
                       // pr(progress);
                       timer.restart();
                   }

                   return estate.req_stop();
               }) ||
           [&]()
    {
        // pr();
        vstate.update(estate);
        return estate.req_stop();
    }();
}

template <std::size_t ncores, typename Viewport>
static bool
predicated_mp(EventState&          estate,
              ViewState<Viewport>& vstate,
              const float          threshold,
              const unsigned       max_depth)
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
                    [res, threshold, max_depth, &progress](auto view) noexcept
                    {
                        const auto [p, rc] =
                            RenderPixelSIMDPredicated{res,
                                                      threshold,
                                                      max_depth}(view);
                        progress.fetch_add(p, std::memory_order_relaxed);
                    });
            }
        });
    tp.affine(AFFINITY::PHYSICAL);

    while(!estate.req_stop() &&
          scc.progress.load(std::memory_order_relaxed) < vstate.img.size())
    {
        vstate.update(estate);
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
hybrid_render(EventState&          estate,
              ViewState<Viewport>& vstate,
              const float          threshold,
              const unsigned       max_depth,
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

    auto view = std::views::transform(
        vstate.img,
        InputEncoder(vstate.img.res(), threshold, max_depth));
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
            vstate.update(estate);
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

    vstate.update(estate);

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
hybrid_mp(EventState&          estate,
          ViewState<Viewport>& vstate,
          const float          threshold,
          const unsigned       max_depth,
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

    auto input_view = std::views::transform(
        vstate.img,
        InputEncoder(vstate.img.res(), threshold, max_depth));

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
                vstate.update(estate);
                timer.restart();
            }

            std::this_thread::sleep_for(eg);
        }
        shop.set();
    }

    if(estate.req_stop()) return false;

    shop.finalize(oc);

    img_upd(vstate.img, roc);

    vstate.update(estate);

    return true;
}

static void
clear(Image& img) noexcept
{
    for(auto [_, pix]: img) pix = 0;
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

        const auto [resx, resy]     = parse_results.template get_opt<'r'>();
        const float       threshold = parse_results.template get_opt<'t'>();
        const bool        keep_res  = parse_results.template get_opt<'k'>();
        const unsigned    max_depth = parse_results.template get_opt<'d'>();
        const std::size_t stream_size =
            align_up(parse_results.template get_opt<'s'>(), 32ul);

        if(threshold < 0)
            throw std::runtime_error("Negative sampling threshold");

        Image img(Vec2u{resx, resy});

        if(stream_size * ncores >= img.size())
            throw std::runtime_error("Stream size is too big");

        logger.debug("making viewport");

        logger.info("Hotkeys: ESC - close; SPACE - switch execution method; M "
                    "- toggle multithreading");

        EventState estate{keep_res};

        logger.debug("initializing glfw");
        const auto& h = gl::System::init();

        logger.debug("making window");
        auto window = gl::create_window(h,
                                        resx,
                                        resy,
                                        estate.get_resize_action(),
                                        estate.get_key_action(),
                                        "Loom SmallPT Demo");

        logger.debug("OpenGL version: {}; GLSL version: {}",
                     h.get_string(GL_VERSION),
                     h.get_string(GL_SHADING_LANGUAGE_VERSION));

        logger.debug("making viewport");
        auto viewport = gl::ImageViewport();

        constexpr char osd_text[] =
            "ESC: CLOSE\nSPACE: SWITCH METHOD\nM: SWITCH MULTITHREADING";

        logger.debug("making osd");
        auto osd = gl::OSD(
            5.f,
            resy - 5.f - stb_easy_font_height(const_cast<char*>(osd_text)),
            osd_text,
            resx,
            resy);

        viewport.load_img(img.data(), resx, resy);

        ViewState vstate{h, window, viewport, osd, logger, img};

        logger.debug("vstate ready");

        while(!estate.req_quit)
        {
            // ElapsedTimer<> timer;

            logger.debug("rendering stream?{}, mp?{}",
                         estate.run_stream,
                         estate.run_mp);

            if(estate.run_stream)
            {
                if(estate.run_mp)
                    hybrid_mp<ncores>(estate,
                                      vstate,
                                      threshold,
                                      max_depth,
                                      stream_size,
                                      2ms);
                else
                    hybrid_render(estate,
                                  vstate,
                                  threshold,
                                  max_depth,
                                  stream_size);
            }
            else
            {
                if(estate.run_mp)
                    predicated_mp<ncores>(estate, vstate, threshold, max_depth);
                else
                    predicated_render(estate, vstate, threshold, max_depth);
            }

            logger.debug("rendering done. show still image");
            while(!estate.req_stop() && window.active())
            {
                viewport.draw();
                osd.draw();
                window.swap_buffers();
                h.poll();
                std::this_thread::sleep_for(100ms);
            }

            estate.resize(img);
            logger.debug("new image size: {}x{}", img.width(), img.height());

            estate.reset();
            clear(img);
        }

        logger.debug("Goodbye");
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
    catch(const gl::GLException& ex)
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
