// -*- C++ -*-
// smallpt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "bm.hpp"
#include "render.hpp"

#include <string_view>

using namespace loom;
using namespace vm;

using C = constants<CS(40),
                    sl2cs<THRESHOLD_VALUE>,
                    CS(64),
                    sl2cs<BLUR_VALUE>,
                    sl2cs<DEPTH_VALUE>>;

using kernel = PT_KERNEL<float, C>;

template <bool tbd>
using PTScalar = RenderBSE<kernel, C, tbd>;

template <bool               ts,
          bool               tbd,
          simd::ReconvMethod rcnv,
          typename UTR = std::ratio<1, 16>>
using PTCompact = RenderSIMDCompact<kernel, C, ts, tbd, rcnv, UTR>;

template <bool               ts,
          bool               tbd,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh>
using PTFunction = RenderSIMDFunction<kernel, C, ts, tbd, rcnv, UTRt, UTRh>;

template <bool               ts,
          bool               tbd,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh>
using PTStream = RenderSIMDStream<kernel, C, ts, tbd, rcnv, UTRt, UTRh>;

template <typename R>
using mk_size =
    std::integral_constant<unsigned,
                           simd::native_vector_extent_v<float> *
                               simd::get_threshold_v<kernel::num_blocks, R>>;

constexpr unsigned stream_size25 = mk_size<std::ratio<2, 5>>::value;

using FavStream = PTStream<true,
                           true,
                           simd::SUCCESSOR_MAJOR,
                           std::ratio<1, 16>,
                           std::ratio<1, 16>>;

int
main(int argc, char* argv[])
{
    const auto pr     = argparse::parse(make_opts(), {argc, argv});
    Logger     logger = get_logger(pr);

    run(logger,
        get_file(logger, pr),
        get_num_runs(logger, pr),
        PTCompact<true, true, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PTStream<true,
                 true,
                 simd::SUCCESSOR_MAJOR,
                 std::ratio<1, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTFunction<true,
                   true,
                   simd::SUCCESSOR_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTScalar<true>{}
#ifdef BENCH_ALL
        ,
        FavStream{256},
        FavStream{512},
        FavStream{mk_size<std::ratio<1, 1>>::value},
        FavStream{mk_size<std::ratio<2, 1>>::value},
        FavStream{mk_size<std::ratio<1, 8>>::value},
        PTCompact<false, true, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PTCompact<true, false, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PTCompact<false, false, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PTCompact<true, false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PTCompact<false, false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PTStream<false,
                 false,
                 simd::HYBRID_MAJOR,
                 std::ratio<15, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTStream<false,
                 false,
                 simd::HYBRID_MAJOR,
                 std::ratio<1, 2>,
                 std::ratio<1, 16>>{stream_size25},
        PTStream<false,
                 false,
                 simd::SUCCESSOR_MAJOR,
                 std::ratio<1, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTFunction<false,
                   false,
                   simd::HYBRID_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTFunction<false,
                   true,
                   simd::SUCCESSOR_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTFunction<false,
                   false,
                   simd::SUCCESSOR_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTStream<true,
                 false,
                 simd::SUCCESSOR_MAJOR,
                 std::ratio<1, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTScalar<false>{}
#endif
    );

    return 0;
}
