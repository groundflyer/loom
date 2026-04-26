// -*- C++ -*-
// smallpt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include <common/logging.hpp>
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
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <string_view>

using namespace loom;
using namespace vm;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;

template <typename R>
static constexpr double
ratio2percent() noexcept
{
    return static_cast<double>(R::num) / static_cast<double>(R::den) * 100;
}

template <typename Dur = std::chrono::steady_clock::duration>
using bm_result = std::pair<std::string, Dur>;

template <typename Dur = std::chrono::steady_clock::duration, typename... Bms>
static std::array<bm_result<Dur>, sizeof...(Bms)>
run_benchmarks(Bms... bms)
{
    return {bms()...};
}

template <typename Dur, std::size_t N>
static void
print_results(std::array<bm_result<Dur>, N>&& results) noexcept
{
    std::ranges::sort(results,
                      std::less<std::size_t>{},
                      [](const auto& e) noexcept { return e.second.count(); });

    const Dur fastest_dur = results[0].second;

    std::size_t id_width = 10;

    for(const auto& [id, _]: results) id_width = std::max(id_width, id.size());

    fmt::print("{:<{}} | {:^7} | {:>3}\n",
               "Benchmark",
               id_width,
               "Time",
               "Slowdown");

    fmt::print("{0:-^{1}}{2}{0:-^9}{2}{0:-^9}\n", "", id_width + 1, "\u253c");

    for(const auto& [id, dur]: results)
        fmt::print("{:<{}} | {:^6.3%S}s | {:>3.0f}%\n",
                   id,
                   id_width,
                   dur,
                   (static_cast<double>(dur.count()) /
                    static_cast<double>(fastest_dur.count())) *
                       100);
}

template <typename C>
Image
get_img() noexcept
{
    constexpr unsigned w =
        read_number<unsigned>(cs2sv<typename C::WIDTH>).value();
    constexpr unsigned h =
        read_number<unsigned>(cs2sv<typename C::HEIGHT>).value();
    return Image(Vec2u(w, h));
}

template <template <typename, typename> typename PTK,
          typename C,
          typename R,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct SIMDFunction
{
    using kernel = PTK<float, C>;

    bm_result<typename Clock::duration>
    operator()() const noexcept
    {
        Image img = get_img<C>();

        simd::function<kernel, simd::MASKED, simd::COUNTING_SORT, R> f(
            img.size());

        std::vector<output_type<kernel>> result_buffer;
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
        std::size_t rc = 0;

        std::ranges::copy(
            std::views::transform(result_buffer,
                                  OutputPixelConverter<kernel>(rc)),
            OutputAdapter<Image::iterator>(img.begin()));

        if constexpr(count_extra) e = timer.elapsed();

        return {fmt::format("{} SIMD function|{}|UTR:{:.0f}%|rc:{}",
                            simd::info::value,
                            img.size(),
                            ratio2percent<R>(),
                            rc),
                e};
    }
};

template <template <typename, typename> typename PTK,
          typename C,
          typename R,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct SIMDStream
{
    using kernel = PTK<float, C>;

    unsigned stream_size = 64;

    bm_result<typename Clock::duration>
    operator()() const noexcept
    {
        Image img = get_img<C>();

        simd::stream<kernel, simd::MASKED, simd::COUNTING_SORT, false, R> s(
            stream_size);

        std::vector<output_type<kernel>> result_buffer;
        result_buffer.reserve(img.size());
        auto        inserter = std::back_inserter(result_buffer);
        std::size_t rc       = 0;

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

        std::ranges::copy(
            std::views::transform(result_buffer,
                                  OutputPixelConverter<kernel>(rc)),
            OutputAdapter<Image::iterator>(img.begin()));

        if constexpr(count_extra) e = timer.elapsed();

        return {fmt::format("{} SIMD stream|{}|UTR:{:.0f}%|rc:{}",
                            simd::info::value,
                            stream_size,
                            ratio2percent<R>(),
                            rc),
                e};
    }
};

template <template <typename, typename> typename PTK,
          typename C,
          typename Clock = std::chrono::steady_clock>
struct BSE
{
    using kernel = PTK<float, C>;

    bm_result<typename Clock::duration>
    operator()() const noexcept
    {
        Image       img = get_img<C>();
        std::size_t rc  = 0;

        ElapsedTimer<Clock> timer;

        std::ranges::for_each(img,
                              [&rc](auto val) noexcept
                              {
                                  bse::function<kernel> f;

                                  auto [pos, pix] = val;

                                  const auto [r, g, b, lrc, _] =
                                      f(encode_pp(pos));

                                  pix = RGBA{r, g, b, 1};

                                  rc += lrc;
                              });

        auto e = timer.elapsed();

        return {fmt::format("BSE|rc:{}", rc), e};
    }
};

template <template <typename, typename> typename PTK,
          typename C,
          typename Clock = std::chrono::steady_clock>
struct SIMDPredicated
{
    using kernel                   = PTK<float, C>;
    static constexpr std::size_t N = simd::native_vector_extent_v<float>;

    bm_result<typename Clock::duration>
    operator()() const noexcept
    {
        Image       img = get_img<C>();
        std::size_t rc  = 0;

        ElapsedTimer<Clock> timer;

        std::ranges::for_each(
            ranges::views::chunk(img, N),
            [&rc](auto&& chunk) noexcept
            {
                simd::embedded_function<kernel> f;

                simd::native_vector<std::uint32_t> vpp;

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

                    // guarantee tail elems to complete with the first element
                    for(std::size_t i = tail_size; i < N; ++i) vpp[i] = vpp[0];
                }
                else
                {
                    for(std::size_t i = 0; i < N; ++i)
                        vpp[i] = encode_pp(chunk[i].first);
                }

                const auto [vr, vg, vb, vrc, _] = f(vpp);

                for(std::size_t i = 0; i < vs; ++i)
                    chunk[i].second = RGBA{vr[i], vg[i], vb[i], 1};

                rc += is_tail ? masked_ray_count(vrc, tail_size) :
                                __builtin_reduce_add(vrc);
            });

        auto e = timer.elapsed();

        return {fmt::format("{} SIMD predicated|rc:{}", simd::info::value, rc),
                e};
    }
};

using C1 = constants<CS(128), CS(0.1), CS(64)>;
using R1 = std::ratio<1, 2>;

int main(/*int argc, char* argv[]*/)
{
    print_results(run_benchmarks(BSE<pt_const, C1>{},
                                 SIMDPredicated<pt_const, C1>{},
                                 SIMDStream<pt_const, C1, R1>{},
                                 SIMDFunction<pt_const, C1, R1>{}));
    return 0;
}
