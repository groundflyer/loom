#include "main.hpp"
#include "utils.hpp"

#include <common/timer.hpp>
#include <vm/mp.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/subrange.hpp>

#include <algorithm>

namespace loom
{
inline constexpr std::size_t ncores = NUM_CORES_L;

bool
predicated_render(EventState&    estate,
                  ViewState&     vstate,
                  const float    threshold,
                  const unsigned max_depth)
{
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

bool
predicated_mp(EventState&    estate,
              ViewState&     vstate,
              const float    threshold,
              const unsigned max_depth)
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
    vm::simd::hybrid_function<pt_float,
                          true,
                              true,
                          simd::STATIC_MAJOR,
                          std::ratio<1, 16>,
                          std::ratio<1, 16>>
        f(img.size());

    const auto r = f.template operator()<true>(
        std::views::transform(img, InputEncoder(img.res(), t)));

    std::vector<vm::output_type<pt_float>> result_buffer;
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

bool
hybrid_render(EventState&    estate,
              ViewState&     vstate,
              const float    threshold,
              const unsigned max_depth,
              const unsigned stream_size)
{
    vm::simd::hybrid_stream<pt_float,
                            true,
                            true,
                            vm::simd::STATIC_MAJOR,
                            std::ratio<1, 16>,
                            std::ratio<1, 16>>
        s(stream_size);

    std::vector<vm::output_type<pt_float>> result_buffer(stream_size);
    remote_output_control                  roc(result_buffer.data());

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
using mp_stream = vm::simd::hybrid_stream<K,
                                          false,
                                          true,
                                          vm::simd::STATIC_MAJOR,
                                          std::ratio<1, 16>,
                                          std::ratio<1, 16>>;

template <typename K>
using mp_context = vm::mp::Context<K, mp_stream>;

/// @param eg exchange granularity
bool
hybrid_mp(EventState&                     estate,
          ViewState&                      vstate,
          const float                     threshold,
          const unsigned                  max_depth,
          const unsigned                  stream_size,
          const std::chrono::milliseconds eg)
{
    vm::mp::Shop<type_sequence<pt_float>, mp_context, ncores> shop(stream_size);

    std::vector<vm::output_type<pt_float>> result_buffer(stream_size * ncores);
    remote_output_control                  roc(result_buffer.data());

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

        while(!estate.req_stop() && !vm::mp::is_complete(ic))
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
} // namespace loom
