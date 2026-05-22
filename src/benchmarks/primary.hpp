// -*- C++ -*-
// primary.hpp
//

#pragma once

#define BM_PRIMARY(C)                                                         \
    using kernel                  = primary_const<float, C>;                  \
    constexpr unsigned num_blocks = kernel::num_blocks;                       \
    using PrimaryBSE              = RenderBSE<kernel, C>;                     \
    template <bool               ts,                                          \
              simd::ReconvMethod rcnv,                                        \
              typename UTR = std::ratio<1, 16>>                               \
    using PrimaryEmbedded = RenderSIMDEmbedded<kernel, C, ts, rcnv, UTR>;     \
    template <bool ts, simd::ReconvMethod rcnv, typename UTRt, typename UTRh> \
    using PrimaryFunction =                                                   \
        RenderSIMDFunction<kernel, C, ts, rcnv, UTRt, UTRh>;                  \
    template <bool               ts,                                          \
              simd::ReconvMethod rcnv,                                        \
              typename UTRt,                                                  \
              typename UTRh,                                                  \
              unsigned stream_size>                                           \
    using PrimaryStream =                                                     \
        RenderSIMDStream<kernel, C, ts, rcnv, UTRt, UTRh, stream_size>;       \
    template <typename R>                                                     \
    using mk_size =                                                           \
        std::integral_constant<unsigned,                                      \
                               simd::native_vector_extent_v<float> *          \
                                   simd::get_threshold_v<num_blocks, R>>;     \
    constexpr unsigned stream_size25 = mk_size<std::ratio<1, 6>>::value;      \
    int                main(int argc, char* argv[])                           \
    {                                                                         \
        run({argc, argv},                                                     \
            PrimaryStream<false,                                              \
                          simd::HYBRID_MAJOR,                                 \
                          std::ratio<15, 16>,                                 \
                          std::ratio<1, 16>,                                  \
                          stream_size25>{},                                   \
            PrimaryStream<false,                                              \
                          simd::STATIC_MAJOR,                                 \
                          std::ratio<15, 16>,                                 \
                          std::ratio<1, 16>,                                  \
                          stream_size25>{},                                   \
            PrimaryStream<false,                                              \
                          simd::HYBRID_MAJOR,                                 \
                          std::ratio<1, 2>,                                   \
                          std::ratio<1, 16>,                                  \
                          stream_size25>{},                                   \
            PrimaryStream<false,                                              \
                          simd::STATIC_MAJOR,                                 \
                          std::ratio<1, 2>,                                   \
                          std::ratio<1, 16>,                                  \
                          stream_size25>{},                                   \
            PrimaryStream<false,                                              \
                          simd::STATIC_MAJOR,                                 \
                          std::ratio<1, 16>,                                  \
                          std::ratio<1, 16>,                                  \
                          stream_size25>{},                                   \
            PrimaryBSE{},                                                     \
            PrimaryEmbedded<true, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},   \
            PrimaryEmbedded<false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},  \
            PrimaryEmbedded<true, simd::STATIC_MAJOR, std::ratio<1, 16>>{},   \
            PrimaryEmbedded<false, simd::STATIC_MAJOR, std::ratio<1, 16>>{}); \
        return 0;                                                             \
    }
