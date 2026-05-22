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

using C128   = constants<CS(128), CS(0.1), CS(64)>;
using kernel = pt_const<float, C128>;

constexpr unsigned num_blocks = 44; // kernel::num_blocks;

using PTBSE = RenderBSE<kernel, C128>;

template <bool ts, simd::ReconvMethod rcnv, typename UTR = std::ratio<1, 16>>
using PTEmbedded = RenderSIMDEmbedded<kernel, C128, ts, rcnv, UTR>;

template <bool ts, simd::ReconvMethod rcnv, typename UTRt, typename UTRh>
using PTFunction = RenderSIMDFunction<kernel, C128, ts, rcnv, UTRt, UTRh>;

template <bool               ts,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh,
          unsigned stream_size>
using PTStream =
    RenderSIMDStream<kernel, C128, ts, rcnv, UTRt, UTRh, stream_size>;

template <typename R>
using mk_size =
    std::integral_constant<unsigned,
                           simd::native_vector_extent_v<float> *
                               simd::get_threshold_v<num_blocks, R>>;

constexpr unsigned stream_size25 = mk_size<std::ratio<2, 5>>::value;

int
main(int argc, char* argv[])
{
    run({argc, argv},
        PTBSE{},
        PTEmbedded<true, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PTEmbedded<false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PTEmbedded<true, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        PTEmbedded<false, simd::STATIC_MAJOR, std::ratio<1, 16>>{},
        PTStream<true,
                 simd::HYBRID_MAJOR,
                 std::ratio<15, 16>,
                 std::ratio<1, 16>,
                 stream_size25>{},
        PTStream<true,
                 simd::STATIC_MAJOR,
                 std::ratio<15, 16>,
                 std::ratio<1, 16>,
                 stream_size25>{},
        PTStream<true,
                 simd::HYBRID_MAJOR,
                 std::ratio<1, 2>,
                 std::ratio<1, 16>,
                 stream_size25>{},
        PTStream<true,
                 simd::STATIC_MAJOR,
                 std::ratio<1, 2>,
                 std::ratio<1, 16>,
                 stream_size25>{},
        PTStream<true,
                 simd::STATIC_MAJOR,
                 std::ratio<1, 16>,
                 std::ratio<1, 16>,
                 stream_size25>{},
        PTFunction<true,
                   simd::STATIC_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{},
        PTFunction<true,
                   simd::HYBRID_MAJOR,
                   std::ratio<1, 16>,
                   std::ratio<1, 16>>{});

    return 0;
}
