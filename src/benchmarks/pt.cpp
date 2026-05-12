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
} // namespace loom

using namespace loom;
using namespace vm;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;
using namespace std::literals::chrono_literals;

#if FMT_VERSION < 110000
template <typename Dur>
static double
to_format(const Dur& dur) noexcept
{
    return static_cast<double>(
               std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                   .count()) /
           1000;
}
#endif

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
run_benchmarks(Logger& logger, Bms... bms)
{
    auto decorate = [&](auto&& bm)
    {
        const std::string i{bm.info()};
        logger.info("Benchmarking {}...", i);
        const Dur dur = bm();
#if FMT_VERSION < 110000
        logger.info("Result {:.3f}s", to_format(dur));
#else
        logger.info("Result {:.3%S}s", dur);
#endif
        return bm_result<Dur>{std::move(i), dur};
    };

    return {decorate(bms)...};
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
#if FMT_VERSION < 110000
        fmt::print("{:<{}} | {:>6.03f}s | {:>3.0f}%\n",
                   id,
                   id_width,
                   to_format(dur),
                   (static_cast<double>(dur.count()) /
                    static_cast<double>(fastest_dur.count())) *
                       100);
#else
        fmt::print("{:<{}} | {:>6.3%S}s | {:>3.0f}%\n",
                   id,
                   id_width,
                   dur,
                   (static_cast<double>(dur.count()) /
                    static_cast<double>(fastest_dur.count())) *
                       100);
#endif
}

template <typename C>
Image
get_img() noexcept
{
    const unsigned w =
        read_number<unsigned>(cs2sv<typename C::WIDTH>).value();
    const unsigned h =
        read_number<unsigned>(cs2sv<typename C::HEIGHT>).value();
    return Image(Vec2u(w, h));
}

template <template <typename, typename> typename PTK,
          typename C,
          typename UTR,
          bool alloc_temp_storage,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct SIMDFunction
{
    using kernel = PTK<float, C>;

    static auto
    get_func(const unsigned size) noexcept
    {
        return simd::hybrid_function<kernel, alloc_temp_storage, UTR>(size);
    }

    std::string
    info() const noexcept
    {
        return fmt::format("full range|{}|UTR:{:.0f}%",
                           alloc_temp_storage ? "with ts" : "no ts",
                           ratio2percent<UTR>());
    }

    Clock::duration
    operator()() const noexcept
    {
        Image img = get_img<C>();

        auto f = get_func(img.size());

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

        std::ranges::copy(std::views::transform(result_buffer, convert_out_pix),
                          OutputAdapter<Image::iterator>(img.begin()));

        if constexpr(count_extra) e = timer.elapsed();

        return e;
    }
};

template <template <typename, typename> typename PTK,
          typename C,
          typename UTR,
          bool alloc_temp_storage,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct SIMDStream
{
    using kernel = PTK<float, C>;

    unsigned stream_size =
        simd::native_vector_extent_v<float> * (kernel::num_blocks + 1);

    static auto
    get_stream(unsigned stream_size) noexcept
    {
        return simd::hybrid_stream<kernel, alloc_temp_storage, UTR>(
            stream_size);
    }

    std::string
    info() const noexcept
    {
        return fmt::format("stream|{}|size:{}|UTR:{:.0f}%",
                           alloc_temp_storage ? "with ts" : "no ts",
                           stream_size,
                           ratio2percent<UTR>());
    }

    Clock::duration
    operator()() const noexcept
    {
        Image img = get_img<C>();

        auto s = get_stream(stream_size);

        std::vector<output_type<kernel>> result_buffer;
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

template <template <typename, typename> typename PTK,
          typename C,
          typename Clock = std::chrono::steady_clock>
struct BSE
{
    using kernel = PTK<float, C>;

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
                                  bse::function<kernel> f;

                                  auto [pos, pix] = val;

                                  const auto [r, g, b, _, __] =
                                      f(encode_pp(pos));

                                  pix = RGBA{r, g, b, 1};
                              });

        return timer.elapsed();
    }
};

template <template <typename, typename> typename PTK,
          typename C,
          bool                    alloc_temp_storage,
          simd::SelectionStrategy strategy,
          typename UTR   = std::ratio<1, 2>,
          typename Clock = std::chrono::steady_clock>
struct SIMDEmbedded
{
    using kernel                   = PTK<float, C>;
    static constexpr std::size_t N = simd::native_vector_extent_v<float>;

    static constexpr std::string_view
    strategy_string() noexcept
    {
        switch(strategy)
        {
        case(simd::GREEDY_FAIR): return "greedy fair";
        case(simd::HYBRID_MAJOR): return "hybrid major";
        case(simd::GREEDY_MAJOR): return "greedy major";
        case(simd::HYBRID_FAIR): return "hybrid fair";
        }
    }

    std::string
    info() const noexcept
    {
        return fmt::format("embedded|{}|{}|UTR:{}%",
                           alloc_temp_storage ? "with ts" : "no ts",
                           strategy_string(),
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
                simd::
                    embedded_function<kernel, alloc_temp_storage, strategy, UTR>
                        f;

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

using C1 = constants<CS(128), CS(0.1), CS(64)>;
using R1 = std::ratio<1, 2>;
using R2 = std::ratio<1, 5>;

int main(/*int argc, char* argv[]*/)
{
    Logger logger(Logger::INFO);
    logger.info("Using x86-64 vector extensions: {}", simd::info::value);

    print_results(run_benchmarks(
        logger,
        BSE<pt_const, C1>{},
        SIMDEmbedded<pt_const, C1, false, simd::GREEDY_MAJOR, R2>{},
        SIMDEmbedded<pt_const, C1, true, simd::HYBRID_MAJOR, R2>{},
        SIMDEmbedded<pt_const, C1, true, simd::GREEDY_MAJOR, R2>{},
        SIMDEmbedded<pt_const, C1, true, simd::HYBRID_FAIR, R2>{},
        SIMDStream<pt_const, C1, R2, false>{},
        SIMDStream<pt_const, C1, R2, true>{},
        SIMDFunction<pt_const, C1, R2, false>{},
        SIMDFunction<pt_const, C1, R2, true>{}));
    return 0;
}
