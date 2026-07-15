#include "utils.hpp"
#include "vm.hpp"

#include <common/timer.hpp>
#include <vm/mp.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/subrange.hpp>

#include <algorithm>

namespace loom
{
bool
predicated(EventState& estate, ViewState& vstate)
{
    ElapsedTimer<> img_timer, osd_timer, avg_timer;

    std::size_t progress{}, tmp_rc{}, all_rc{};

    return std::ranges::none_of(
               ranges::views::chunk(vstate.img, RenderPixelSIMDPredicated::N),
               [&](auto chunk)
               {
                   const auto [p, rc] =
                       RenderPixelSIMDPredicated{vstate.img.res(),
                                                 estate.threshold,
                                                 estate.max_depth,
                                                 estate.blur}(chunk);
                   progress += p;
                   tmp_rc += rc;

                   if(img_timer.has_expired(vstate.tf))
                   {
                       vstate.update(estate);
                       img_timer.restart();
                   }

                   if(osd_timer.has_expired(osd_timeframe))
                   {
                       vstate.update_stats(estate);
                       all_rc += tmp_rc;
                       vstate.update_osd(
                           progress,
                           ray_traversal_rate(tmp_rc, osd_timer.restart()),
                           ray_traversal_rate(all_rc, avg_timer.elapsed()));
                       tmp_rc = 0;
                   }

                   return estate.req_stop();
               }) ||
           [&]()
    {
        vstate.update(estate);
        return estate.req_stop();
    }();
}

bool
predicated_mp(EventState& estate, ViewState& vstate)
{
    struct Shared
    {
        std::atomic_size_t counter{};
        std::atomic_size_t progress{};
        std::atomic_size_t ray_count{};
        std::atomic_flag   flag{};
    };

    PADDED_VALUE(Shared, scc);

    auto r = ranges::views::chunk(vstate.img, RenderPixelSIMDPredicated::N);

    thread_pool<ncores> tp(
        [&,
         &c         = scc.counter,
         &s         = scc.flag,
         &progress  = scc.progress,
         &ray_count = scc.ray_count,
         res        = vstate.img.res(),
         is         = vstate.img.size()]()
        {
            constexpr std::size_t cs = 16;

            while(!s.test(std::memory_order_relaxed))
            {
                const std::size_t b =
                    c.fetch_add(cs, std::memory_order_relaxed);

                if(b >= is) return;

                std::ranges::for_each(
                    std::views::counted(std::ranges::begin(r) + b, cs),
                    [res,
                     threshold = estate.threshold,
                     max_depth = estate.max_depth,
                     blur      = estate.blur,
                     &progress,
                     &ray_count](auto view) noexcept
                    {
                        const auto [p, rc] =
                            RenderPixelSIMDPredicated{res,
                                                      threshold,
                                                      max_depth,
                                                      blur}(view);
                        progress.fetch_add(p, std::memory_order_relaxed);
                        ray_count.fetch_add(rc, std::memory_order_relaxed);
                    });
            }
        });

    tp.affine(AFFINITY::PHYSICAL);

    ElapsedTimer<> osd_timer, avg_timer;

    std::size_t all_rc{};

    while(!estate.req_stop() &&
          scc.progress.load(std::memory_order_relaxed) < vstate.img.size())
    {
        vstate.update(estate);

        if(osd_timer.has_expired(osd_timeframe))
        {
            const std::size_t rc =
                scc.ray_count.exchange(0, std::memory_order_relaxed);
            all_rc += rc;
            vstate.update_stats(estate);
            vstate.update_osd(scc.progress.load(std::memory_order_relaxed),
                              ray_traversal_rate(rc, osd_timer.restart()),
                              ray_traversal_rate(all_rc, avg_timer.elapsed()));
        }

        std::this_thread::sleep_for(vstate.tf);
    }

    if(estate.req_stop())
    {
        scc.flag.test_and_set(std::memory_order_relaxed);
        return false;
    }

    {
        const std::size_t rc =
            scc.ray_count.exchange(0, std::memory_order_relaxed);
        all_rc += rc;
        vstate.update(estate);
        vstate.update_osd(vstate.img.size(),
                          ray_traversal_rate(rc, osd_timer.elapsed()),
                          ray_traversal_rate(all_rc, avg_timer.elapsed()));
    }

    return true;
}

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
hybrid(EventState& estate, ViewState& vstate)
{
    const float       threshold   = estate.threshold;
    const unsigned    max_depth   = estate.max_depth;
    const float       blur        = estate.blur;
    const std::size_t stream_size = estate.stream_size;

    vm::simd::hybrid_stream<pt_float,
                            false,
                            true,
                            vm::simd::SUCCESSOR_MAJOR,
                            std::ratio<1, 16>,
                            std::ratio<1, 16>>
        s(stream_size);

    std::vector<vm::output_type<pt_float>> result_buffer(stream_size);
    remote_output_control                  roc(result_buffer.data());

    std::size_t progress{}, tmp_rc{}, all_rc{};

    auto view = std::views::transform(
        vstate.img,
        InputEncoder(vstate.img.res(), threshold, max_depth, blur));
    auto it = s.set_inputs(view);

    s();
    s.sort();

    ElapsedTimer<> img_timer, osd_timer, avg_timer;

    while(!estate.req_stop() && it != std::ranges::end(view))
    {
        auto exs = s.exchange(ranges::subrange(it, std::ranges::end(view)));
        const auto se = std::ranges::size(exs);
        it += se;
        progress += se;

        std::ranges::copy(exs, roc.get_output_iterator());

        tmp_rc += img_upd(vstate.img, roc);

        if(img_timer.has_expired(vstate.tf))
        {
            vstate.update(estate);
            img_timer.restart();
        }

        if(osd_timer.has_expired(osd_timeframe))
        {
            vstate.update_stats(estate);
            all_rc += tmp_rc;
            vstate.update_osd(progress,
                              ray_traversal_rate(tmp_rc, osd_timer.restart()),
                              ray_traversal_rate(all_rc, avg_timer.elapsed()));
            tmp_rc = 0;
        }

        s();
        s.sort();
    }

    if(estate.req_stop()) return false;

    osd_timer.restart();

    while(!estate.req_stop() && !s.done())
    {
        s();
        s.sort();
    }

    if(estate.req_stop()) return false;

    std::ranges::copy(s.get_output_zipped(), roc.get_output_iterator());

    tmp_rc = img_upd(vstate.img, roc);

    vstate.update(estate);
    vstate.update_stats(estate);
    all_rc += tmp_rc;
    vstate.update_osd(vstate.img.size(),
                      ray_traversal_rate(tmp_rc, osd_timer.elapsed()),
                      ray_traversal_rate(all_rc, avg_timer.elapsed()));

    return true;
}

template <typename K>
using mp_stream = vm::simd::hybrid_stream<K,
                                          false,
                                          true,
                                          vm::simd::SUCCESSOR_MAJOR,
                                          std::ratio<1, 16>,
                                          std::ratio<1, 16>>;

template <typename K>
using mp_context = vm::mp::Context<K, mp_stream>;

/// @param eg exchange granularity
bool
hybrid_mp(EventState&                     estate,
          ViewState&                      vstate,
          const std::chrono::milliseconds eg)
{
    const float       threshold   = estate.threshold;
    const unsigned    max_depth   = estate.max_depth;
    const float       blur        = estate.blur;
    const std::size_t stream_size = estate.stream_size;

    vm::mp::Shop<type_sequence<pt_float>, mp_context, ncores> shop(stream_size);

    std::vector<vm::output_type<pt_float>> result_buffer(stream_size * ncores);
    remote_output_control                  roc(result_buffer.data());

    auto input_view = std::views::transform(
        vstate.img,
        InputEncoder(vstate.img.res(), threshold, max_depth, blur));

    auto oc = std::tuple{roc.get_output_iterator()};

    ElapsedTimer<> img_timer, osd_timer, avg_timer;

    std::size_t all_rc{};

    {
        std::size_t progress{}, tmp_rc{};

        auto ic = shop.bind_inputs(input_view);

        progress += stream_size;

        auto tp = shop.get_thread_pool();
        tp.affine(AFFINITY::PHYSICAL);

        while(!estate.req_stop() && !vm::mp::is_complete(ic))
        {
            if(shop.exchange(ic, oc))
            {
                progress += roc.get_count();
                tmp_rc += img_upd(vstate.img, roc);
            }

            if(img_timer.has_expired(vstate.tf))
            {
                vstate.update(estate);
                img_timer.restart();
            }

            if(osd_timer.has_expired(osd_timeframe))
            {
                vstate.update_stats(estate);
                all_rc += tmp_rc;
                vstate.update_osd(
                    progress,
                    ray_traversal_rate(tmp_rc, osd_timer.restart()),
                    ray_traversal_rate(all_rc, avg_timer.elapsed()));
                tmp_rc = 0;
            }

            std::this_thread::sleep_for(eg);
        }
        shop.set();
    }

    if(estate.req_stop()) return false;

    shop.finalize(oc);

    const std::size_t tmp_rc = img_upd(vstate.img, roc);

    vstate.update(estate);
    vstate.update_stats(estate);
    all_rc += tmp_rc;
    vstate.update_osd(vstate.img.size(),
                      ray_traversal_rate(tmp_rc, osd_timer.elapsed()),
                      ray_traversal_rate(all_rc, avg_timer.elapsed()));

    return true;
}
} // namespace loom
