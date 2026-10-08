#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "vm.hpp"
#include "../common/render.hpp"
#include "utils.hpp"

#include <vm/mp.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/subrange.hpp>

namespace loom
{
struct ChunkImg
{
    template <typename Img>
    auto
    operator()(Img&& img) const
    {
        return ranges::views::chunk(std::forward<Img>(img),
                                    RenderPixelSIMDPredicated::N);
    }
};

inline constexpr ChunkImg chunkify{};

bool
predicated(EventState& estate, ViewState& vstate)
{
    return scanline<RenderPixelSIMDPredicated>(estate, vstate, chunkify);
}

bool
predicated_mp(EventState& estate, ViewState& vstate)
{
    return scanline_mp<RenderPixelSIMDPredicated>(estate, vstate, chunkify);
}

bool
bse_scanline(EventState& estate, ViewState& vstate)
{
    return scanline<RenderPixelBSE>(estate, vstate);
}

bool
bse_scanline_mp(EventState& estate, ViewState& vstate)
{
    return scanline_mp<RenderPixelBSE>(estate, vstate);
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

#ifdef DEMO_WRITE_PROGRESS
    std::FILE* concatfile = std::fopen("hybrid_concatfile.txt", "a+");

    static constexpr std::string_view wpref = "hybrid";

    write_image(vstate.img, concatfile, wpref, 0ul, .0);
#endif

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

#ifdef DEMO_WRITE_PROGRESS
            write_image(
                vstate.img,
                concatfile,
                wpref,
                progress,
                std::chrono::duration<double>(img_timer.restart()).count());
#else
            img_timer.restart();
#endif
        }

        if(osd_timer.has_expired(osd_timeframe))
        {
            vstate.update_stats(estate);
            all_rc += tmp_rc;
            const auto at = avg_timer.elapsed();
            vstate.update_osd(
                progress,
                ray_traversal_rate(tmp_rc, osd_timer.restart()),
                ray_traversal_rate(all_rc, at),
                std::chrono::duration_cast<std::chrono::seconds>(at));
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
    const auto at = avg_timer.elapsed();
    vstate.update_osd(vstate.img.size(),
                      ray_traversal_rate(tmp_rc, osd_timer.elapsed()),
                      ray_traversal_rate(all_rc, at),
                      std::chrono::duration_cast<std::chrono::seconds>(at));

#ifdef DEMO_WRITE_PROGRESS
    write_image(vstate.img,
                concatfile,
                wpref,
                progress,
                std::chrono::duration<double>(img_timer.elapsed()).count());
    std::fclose(concatfile);
#endif

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

#ifdef DEMO_WRITE_PROGRESS
    std::FILE* concatfile = std::fopen("hybrid_mp_concatfile.txt", "a+");

    static constexpr std::string_view wpref = "hybrid_mp";

    write_image(vstate.img, concatfile, wpref, 0ul, .0);
#endif

    const unsigned num_workers = std::thread::hardware_concurrency();
    vm::mp::Shop<type_sequence<pt_float>, mp_context> shop(num_workers,
                                                           stream_size);

    std::vector<vm::output_type<pt_float>> result_buffer(stream_size *
                                                         num_workers);
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

#ifdef DEMO_WRITE_PROGRESS
                write_image(
                    vstate.img,
                    concatfile,
                    wpref,
                    progress,
                    std::chrono::duration<double>(img_timer.restart()).count());
#else
                img_timer.restart();
#endif
            }

            if(osd_timer.has_expired(osd_timeframe))
            {
                vstate.update_stats(estate);
                all_rc += tmp_rc;
                const auto at = avg_timer.elapsed();
                vstate.update_osd(
                    progress,
                    ray_traversal_rate(tmp_rc, osd_timer.restart()),
                    ray_traversal_rate(all_rc, at),
                    std::chrono::duration_cast<std::chrono::seconds>(at));
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
    const auto at = avg_timer.elapsed();
    vstate.update_osd(vstate.img.size(),
                      ray_traversal_rate(tmp_rc, osd_timer.elapsed()),
                      ray_traversal_rate(all_rc, at),
                      std::chrono::duration_cast<std::chrono::seconds>(at));

#ifdef DEMO_WRITE_PROGRESS
    write_image(vstate.img,
                concatfile,
                wpref,
                vstate.img.size(),
                std::chrono::duration<double>(img_timer.elapsed()).count());
    std::fclose(concatfile);
#endif

    return true;
}
} // namespace loom
