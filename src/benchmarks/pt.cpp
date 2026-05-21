// -*- C++ -*-
// smallpt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "bm.hpp"

#include <common/argparse.hpp>
#include <common/timer.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/typedefs.hpp>
#include <smallpt/vm/pt.hpp>
#include <smallpt/vm/utils.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/subrange.hpp>

#include <string_view>

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

template <typename C>
Image
get_img() noexcept
{
    const unsigned w = read_number<unsigned>(cs2sv<typename C::WIDTH>).value();
    const unsigned h = read_number<unsigned>(cs2sv<typename C::HEIGHT>).value();
    return Image(Vec2u(w, h));
}

using C      = constants<CS(128), CS(0.1), CS(64)>;
using kernel = pt_const<float, C>;

constexpr unsigned num_blocks = 44; // kernel::num_blocks;

template <simd::ReconvMethod rcnv>
constexpr std::string_view
rcnv_string() noexcept
{
    switch(rcnv)
    {
    case(simd::GREEDY_FAIR): return "greedy-fair";
    case(simd::HYBRID_MAJOR): return "hybrid-major";
    case(simd::GREEDY_MAJOR): return "greedy-major";
    case(simd::HYBRID_FAIR): return "hybrid-fair";
    case(simd::STATIC_FIRST): return "static-first";
    case(simd::STATIC_MAJOR): return "static-major";
    }
}

template <bool               ts,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh,
          bool count_extra = false,
          typename Clock   = std::chrono::steady_clock>
struct SIMDFunction
{
    static auto
    get_func(const unsigned size) noexcept
    {
        return simd::hybrid_function<kernel, ts, rcnv, UTRt, UTRh>(size);
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

template <bool               ts,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh,
          unsigned stream_size,
          bool     count_extra = false,
          typename Clock       = std::chrono::steady_clock>
struct SIMDStream
{
    static auto
    get_stream() noexcept
    {
        return simd::hybrid_stream<kernel, ts, rcnv, UTRt, UTRh>(stream_size);
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

template <typename Clock = std::chrono::steady_clock>
struct BSE
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
                                  bse::function<kernel> f;

                                  auto [pos, pix] = val;

                                  const auto [r, g, b, _, __] =
                                      f(encode_pp(pos));

                                  pix = RGBA{r, g, b, 1};
                              });

        return timer.elapsed();
    }
};

template <bool               ts,
          simd::ReconvMethod rcnv,
          typename UTR   = std::ratio<1, 16>,
          typename Clock = std::chrono::steady_clock>
struct SIMDEmbedded
{
    static constexpr std::size_t N = simd::native_vector_extent_v<float>;

    std::string
    info() const noexcept
    {
        return fmt::format("embedded|ts:{}|{}|UTR:{:.0f}%",
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
                simd::embedded_function<kernel, ts, rcnv, UTR> f;

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

template <typename R>
using mk_size =
    std::integral_constant<unsigned,
                           simd::native_vector_extent_v<float> *
                               simd::get_threshold_v<num_blocks, R>>;

constexpr unsigned stream_size25 = mk_size<std::ratio<2, 5>>::value;

int
main(int argc, char* argv[])
{
    auto parse_results = argparse::parse(
        std::tuple{
            argparse::flag<'v', "verbose">("verbose logging"),
            argparse::option<'l', "log">("path to benchmark logs", "FILE", "")},
        {argc, argv});
    Logger logger(parse_results.template get_opt<'v'>() ? Logger::DEBUG :
                                                          Logger::INFO);

    std::FILE* file = nullptr;

    const std::string_view path = parse_results.template get_opt<'l'>();

    if(!path.empty())
    {
        file = std::fopen(path.data(), "a+");
        if(!file)
            logger.error("Failed to open {} for read/write: {}",
                         path,
                         strerror(errno));
        else
            logger.info("Logging results into {}", path);
    }

    logger.info("CPU: {}", get_cpu_model());
    logger.info("Using x86-64 vector extensions: {}", simd::info::value);

    print_results(run_benchmarks(
        logger,
        file,
        BSE<>{},
        SIMDEmbedded<true, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        SIMDEmbedded<false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        SIMDEmbedded<true, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        SIMDEmbedded<false, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        SIMDStream<true,
                   simd::HYBRID_MAJOR,
                   std::ratio<15, 16>,
                   std::ratio<1, 16>,
                   stream_size25>{},
        SIMDStream<true,
                   simd::STATIC_MAJOR,
                   std::ratio<15, 16>,
                   std::ratio<1, 16>,
                   stream_size25>{},
        SIMDStream<true,
                   simd::HYBRID_MAJOR,
                   std::ratio<1, 2>,
                   std::ratio<1, 16>,
                   stream_size25>{},
        SIMDStream<true,
                   simd::STATIC_MAJOR,
                   std::ratio<1, 2>,
                   std::ratio<1, 16>,
                   stream_size25>{},
        SIMDStream<true,
                   simd::STATIC_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>,
                   stream_size25>{},
        SIMDFunction<true,
                     simd::STATIC_MAJOR,
                     std::ratio<1, 16>,
                     std::ratio<1, 16>>{},
        SIMDFunction<true,
                     simd::HYBRID_MAJOR,
                     std::ratio<1, 16>,
                     std::ratio<1, 16>>{}));

    if(file) std::fclose(file);

    return 0;
}
