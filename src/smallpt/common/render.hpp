// -*- C++ -*-
// render.hpp
//

#pragma once

#include "../vm/events.hpp"

#include <common/threadpool.hpp>
#include <common/timer.hpp>

#include <algorithm>

#ifdef DEMO_WRITE_PROGRESS
#include <cstdio>
#include <smallpt/common/io.hpp>
#endif

namespace loom
{
#ifdef DEMO_WRITE_PROGRESS
static void
write_image(const Image&     img,
            std::FILE*       concatfile,
            std::string_view prefix,
            std::size_t      progress,
            double           delay)
{
    const std::string img_filename =
        fmt::format("{}_{:08}.ppt", prefix, progress);
    write_ppm(img, img_filename);
    if(progress == 0ul)
        fmt::println(concatfile, "file '{}'", img_filename);
    else
        fmt::println(concatfile,
                     "duration {:05.4f}\nfile '{}'",
                     delay,
                     img_filename);
}
#endif

template <typename RenderPixel, typename ImgProj = std::identity>
static bool
scanline(EventState& estate,
         ViewState&  vstate,
         ImgProj     imgproj = std::identity{})
{
    ElapsedTimer<> img_timer, osd_timer, avg_timer;

    std::size_t progress{}, tmp_rc{}, all_rc{};

#ifdef DEMO_WRITE_PROGRESS
    std::FILE* concatfile = std::fopen("scanline_concatfile.txt", "a+");

    static constexpr std::string_view wpref = "scanline";

    write_image(vstate.img, concatfile, wpref, 0ul, .0);
#endif

    return std::ranges::none_of(
               imgproj(vstate.img),
               [&](auto elem)
               {
                   const auto [p, rc] = RenderPixel{vstate.img.res(),
                                                    estate.threshold,
                                                    estate.max_depth,
                                                    estate.blur}(elem);
                   progress += p;
                   tmp_rc += rc;

                   if(img_timer.has_expired(vstate.tf))
                   {
                       vstate.update(estate);
#ifdef DEMO_WRITE_PROGRESS
                       write_image(
                           vstate.img,
                           concatfile,
                           wpref,
                           progress,
                           std::chrono::duration<double>(img_timer.restart())
                               .count());
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
                           std::chrono::duration_cast<std::chrono::seconds>(
                               at));
                       tmp_rc = 0;
                   }

                   return progress == vstate.img.size() || estate.req_stop();
               }) ||
           [&]()
    {
        vstate.update(estate);
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

        return estate.req_stop();
    }();
}

template <typename RenderPixel, typename ImgProj = std::identity>
static bool
scanline_mp(EventState& estate,
            ViewState&  vstate,
            ImgProj     imgproj = std::identity{})
{
    struct Shared
    {
        std::atomic_size_t counter{};
        std::atomic_size_t progress{};
        std::atomic_size_t ray_count{};
        std::atomic_flag   flag{};
    };

    PADDED_VALUE(Shared, scc);

    decltype(auto) r = imgproj(vstate.img);

#ifdef DEMO_WRITE_PROGRESS
    std::FILE*     concatfile = std::fopen("scanline_mp_concatfile.txt", "a+");
    ElapsedTimer<> img_timer;

    static constexpr std::string_view wpref = "scanline_mp";

    write_image(vstate.img, concatfile, wpref, 0ul, .0);
#endif

    thread_pool<dynamic_size> tp(
        std::thread::hardware_concurrency(),
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
                            RenderPixel{res, threshold, max_depth, blur}(view);
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

#ifdef DEMO_WRITE_PROGRESS
        write_image(vstate.img,
                    concatfile,
                    wpref,
                    scc.progress.load(std::memory_order_relaxed),
                    std::chrono::duration<double>(img_timer.restart()).count());
#endif

        if(osd_timer.has_expired(osd_timeframe))
        {
            const std::size_t rc =
                scc.ray_count.exchange(0, std::memory_order_relaxed);
            all_rc += rc;
            vstate.update_stats(estate);
            const auto at = avg_timer.elapsed();
            vstate.update_osd(
                scc.progress.load(std::memory_order_relaxed),
                ray_traversal_rate(rc, osd_timer.restart()),
                ray_traversal_rate(all_rc, at),
                std::chrono::duration_cast<std::chrono::seconds>(at));
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
        const auto at = avg_timer.elapsed();
        vstate.update_osd(vstate.img.size(),
                          ray_traversal_rate(rc, osd_timer.elapsed()),
                          ray_traversal_rate(all_rc, at),
                          std::chrono::duration_cast<std::chrono::seconds>(at));
    }

#ifdef DEMO_WRITE_PROGRESS
    write_image(vstate.img,
                concatfile,
                wpref,
                scc.progress.load(std::memory_order_relaxed),
                std::chrono::duration<double>(img_timer.elapsed()).count());
    std::fclose(concatfile);
#endif

    return true;
}
} // namespace loom
