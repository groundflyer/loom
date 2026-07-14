// -*- C++ -*-
// fm.cpp
#include "ref.hpp"
#include "pt.hpp"

#include <common/threadpool.hpp>
#include <common/timer.hpp>

#include <algorithm>

namespace loom
{
bool
reference(EventState& estate, ViewState& vstate) noexcept
{
    ElapsedTimer<> img_timer, osd_timer, avg_timer;

    std::size_t progress{}, tmp_rc{}, all_rc{};

    bool ret = std::ranges::none_of(
                   vstate.img,
                   [&](auto pixel)
                   {
                       const auto rc = RenderPixel{vstate.img.res(),
                                                   estate.threshold,
                                                   estate.max_depth,
                                                   estate.blur}(pixel);
                       progress += 1;
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

    vstate.update(estate);
    return ret;
}

bool
reference_mp(EventState& estate, ViewState& vstate) noexcept
{
    struct Shared
    {
        std::atomic_size_t counter{};
        std::atomic_size_t progress{};
        std::atomic_size_t ray_count{};
        std::atomic_flag   flag{};
    };

    PADDED_VALUE(Shared, scc);

    thread_pool<ncores> tp(
        [&,
         &c         = scc.counter,
         &s         = scc.flag,
         &progress  = scc.progress,
         &ray_count = scc.ray_count,
         res        = vstate.img.res(),
         is         = vstate.img.size()]()
        {
            constexpr std::size_t cs = 8;

            while(!s.test(std::memory_order_relaxed))
            {
                const std::size_t b =
                    c.fetch_add(cs, std::memory_order_relaxed);

                if(b >= is) return;

                std::ranges::for_each(
                    std::views::counted(std::ranges::begin(vstate.img) + b, cs),
                    [res,
                     threshold = estate.threshold,
                     max_depth = estate.max_depth,
                     blur      = estate.blur,
                     &progress,
                     &ray_count](auto pixel) noexcept
                    {
                        const auto rc =
                            RenderPixel{res, threshold, max_depth, blur}(pixel);
                        progress.fetch_add(1, std::memory_order_relaxed);
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
} // namespace loom
