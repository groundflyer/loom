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

using C1     = constants<CS(40), CS(0.001), CS(64), CS(1), CS(20)>;
using kernel = pt_const<float, C1>;

template <bool               ts,
          bool               sd,
          simd::ReconvMethod rcnv,
          typename UTR = std::ratio<1, 16>>
using PTCompact = RenderSIMDCompact<kernel, C1, ts, sd, rcnv, UTR>;

template <bool ts, simd::ReconvMethod rcnv, typename UTRt, typename UTRh>
using PTFunction = RenderSIMDFunction<kernel, C1, ts, rcnv, UTRt, UTRh>;

template <bool ts, simd::ReconvMethod rcnv, typename UTRt, typename UTRh>
using PTStream = RenderSIMDStream<kernel, C1, ts, rcnv, UTRt, UTRh>;

template <typename R>
using mk_size =
    std::integral_constant<unsigned,
                           simd::native_vector_extent_v<float> *
                               simd::get_threshold_v<kernel::num_blocks, R>>;

constexpr unsigned stream_size25 = mk_size<std::ratio<2, 5>>::value;

using FavStream =
    PTStream<false, simd::STATIC_MAJOR, std::ratio<1, 16>, std::ratio<1, 16>>;

int
main(int argc, char* argv[])
{
    run({argc, argv},
        PTCompact<true, false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PTCompact<false, false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PTCompact<true, false, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        PTCompact<false, false, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        PTCompact<false, true, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        PTCompact<true, true, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        PTStream<false,
                 simd::HYBRID_MAJOR,
                 std::ratio<15, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTStream<false,
                 simd::STATIC_MAJOR,
                 std::ratio<15, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTStream<false,
                 simd::HYBRID_MAJOR,
                 std::ratio<1, 2>,
                 std::ratio<1, 16>>{stream_size25},
        PTStream<false,
                 simd::STATIC_MAJOR,
                 std::ratio<1, 16>,
                 std::ratio<1, 16>>{stream_size25},
        PTFunction<false,
                   simd::STATIC_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTFunction<false,
                   simd::HYBRID_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTStream<true, simd::STATIC_MAJOR, std::ratio<1, 2>, std::ratio<1, 16>>{
            stream_size25},
        PTStream<true,
                 simd::STATIC_MAJOR,
                 std::ratio<1, 16>,
                 std::ratio<1, 16>>{stream_size25},
        FavStream{stream_size25},
        FavStream{256},
        FavStream{512},
        FavStream{mk_size<std::ratio<1, 1>>::value},
        FavStream{mk_size<std::ratio<2, 1>>::value},
        FavStream{mk_size<std::ratio<1, 8>>::value});

    return 0;
}
