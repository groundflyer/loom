// -*- C++ -*-
// render.hpp
//

#pragma once

#include "utils.hpp"

#include <common/timer.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/typedefs.hpp>
#include <smallpt/vm/pt.hpp>
#include <smallpt/vm/utils.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/subrange.hpp>

#include <fmt/core.h>

#include <vector>

namespace loom
{
namespace fn
{
template <typename O>
static RGBA<>
convert_out_pix(const O& o) noexcept
{
    const auto [r, g, b, _, __] = o;
    return RGBA{r, g, b, 1};
}
} // namespace fn

MK_NIEBLOID(convert_out_pix)

template <typename C>
Image
get_img() noexcept
{
    const unsigned w = read_number<unsigned>(cs2sv<typename C::WIDTH>).value();
    const unsigned h = read_number<unsigned>(cs2sv<typename C::HEIGHT>).value();
    return Image(Vec2u(w, h));
}

template <typename K, typename C, typename Clock = std::chrono::steady_clock>
struct RenderBSE
{
    std::string_view
    info() const noexcept
    {
        return "BSE";
    }

    Clock::duration
    operator()() const noexcept
    {
        Image img = get_img<C>();

        ElapsedTimer<Clock> timer;

        std::ranges::for_each(img,
                              [](auto val) noexcept
                              {
                                  vm::bse::function<K> f;

                                  auto [pos, pix] = val;

                                  const auto [r, g, b, _, __] =
                                      f(encode_pp(pos));

                                  pix = RGBA{r, g, b, 1};
                              });

        return timer.elapsed();
    }
};

template <typename K,
          typename C,
          bool                   ts,
          vm::simd::ReconvMethod rcnv,
          typename UTR   = std::ratio<1, 16>,
          typename Clock = std::chrono::steady_clock>
struct RenderSIMDCompact
{
    static constexpr std::size_t N = vm::simd::native_vector_extent_v<float>;

    std::string
    info() const noexcept
    {
        return fmt::format("compact|ts:{}|{}|UTR:{:.0f}%",
                           ts,
                           rcnv_string<rcnv>(),
                           ratio2percent<UTR>());
    }

    Clock::duration
    operator()() const noexcept
    {
        Image img = get_img<C>();

        ElapsedTimer<Clock> timer;

        std::ranges::for_each(
            ranges::views::chunk(img, N),
            [](auto&& chunk) noexcept
            {
                vm::simd::compact_function<K, ts, rcnv, UTR> f;

                vm::simd::native_vector<std::uint32_t> vpp;

                const std::size_t vs = std::ranges::size(chunk);

                const bool        is_tail   = N != vs;
                const std::size_t tail_size = N - vs;

                if(is_tail)
                {
                    for(std::size_t i = 0; i < vs; ++i)
                    {
                        const auto [pos, _] = chunk[i];
                        vpp[i]              = encode_pp(pos);
                    }

                    // guarantee tail elems to complete with
                    // the first element
                    for(std::size_t i = tail_size; i < N; ++i) vpp[i] = vpp[0];
                }
                else
                {
                    for(std::size_t i = 0; i < N; ++i)
                        vpp[i] = encode_pp(chunk[i].first);
                }

                const auto [vr, vg, vb, _, __] = f(vpp);

                for(std::size_t i = 0; i < vs; ++i)
                    chunk[i].second = RGBA{vr[i], vg[i], vb[i], 1};
            });

        return timer.elapsed();
    }
};

template <typename K,
          typename C,
          bool                   ts,
          vm::simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct RenderSIMDFunction
{
    static auto
    get_func(const unsigned size) noexcept
    {
        return vm::simd::hybrid_function<K, ts, rcnv, UTRt, UTRh>(size);
    }

    std::string
    info() const noexcept
    {
        return fmt::format("full-range|ts:{}|{}|UTRt:{:.0f}%|UTRh:{:.0f}%",
                           ts,
                           rcnv_string<rcnv>(),
                           ratio2percent<UTRt>(),
                           ratio2percent<UTRh>());
    }

    Clock::duration
    operator()() const noexcept
    {
        Image img = get_img<C>();

        auto f = get_func(img.size());

        std::vector<vm::output_type<K>> result_buffer;
        result_buffer.reserve(img.size());

        ElapsedTimer<Clock> timer;

        const auto r = f.template operator()<true>(
            std::views::transform(img, encode_pp | first));

        auto e = timer.elapsed();

        std::ranges::copy(r, std::back_inserter(result_buffer));
        std::ranges::sort(
            result_buffer,
            std::less<unsigned>{},
            [width = img.width()](const auto& t) noexcept
            { return pixel_offset(decode_pp(std::get<4>(t)), width, 4); });

        std::ranges::copy(std::views::transform(result_buffer, convert_out_pix),
                          OutputAdapter<Image::iterator>(img.begin()));

        if constexpr(count_extra) e = timer.elapsed();

        return e;
    }
};

template <typename K,
          typename C,
          bool                   ts,
          vm::simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct RenderSIMDStream
{
    unsigned stream_size = 512;

    auto
    get_stream() const noexcept
    {
        return vm::simd::hybrid_stream<K, ts, rcnv, UTRt, UTRh>(stream_size);
    }

    std::string
    info() const noexcept
    {
        return fmt::format("stream|ts:{}|{}|size:{}|UTRt:{:.0f}%|UTRh:{:.0f}%",
                           ts,
                           rcnv_string<rcnv>(),
                           stream_size,
                           ratio2percent<UTRt>(),
                           ratio2percent<UTRh>());
    }

    Clock::duration
    operator()() const noexcept
    {
        Image img = get_img<C>();

        auto s = get_stream();

        std::vector<vm::output_type<K>> result_buffer;
        result_buffer.reserve(img.size());
        auto inserter = std::back_inserter(result_buffer);

        auto view = std::views::transform(img, encode_pp | first);

        ElapsedTimer<Clock> timer;

        auto it = s.set_inputs(view);

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

        auto e = timer.elapsed();

        std::ranges::copy(s.get_output_zipped(), inserter);

        std::ranges::sort(
            result_buffer,
            std::less<unsigned>{},
            [width = img.width()](const auto& t) noexcept
            { return pixel_offset(decode_pp(std::get<4>(t)), width, 4); });

        std::ranges::copy(std::views::transform(result_buffer, convert_out_pix),
                          OutputAdapter<Image::iterator>(img.begin()));

        if constexpr(count_extra) e = timer.elapsed();

        return e;
    }
};
} // namespace loom
