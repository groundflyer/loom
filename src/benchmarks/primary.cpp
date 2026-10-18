// -*- C++ -*-
// smallpt.cpp
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "bm.hpp"
#include "render.hpp"

#include <common/argparse.hpp>

#include <string_view>

using namespace loom;
using namespace vm;

using C = constants<CS(256), CS(0.1), CS(64), sl2cs<BLUR_VALUE>>;

using kernel = primary_const<float, C>;

constexpr unsigned num_blocks = kernel::num_blocks;

template <bool tbd>
using PTScalar = RenderBSE<kernel, C, tbd>;

template <bool               ts,
          bool               tbd,
          simd::ReconvMethod rcnv,
          typename UTR = std::ratio<1, 16>>
using PrimaryCompact = RenderSIMDCompact<kernel, C, ts, tbd, rcnv, UTR>;

template <bool               ts,
          bool               tbd,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh>
using PrimaryFunction =
    RenderSIMDFunction<kernel, C, ts, tbd, rcnv, UTRt, UTRh>;

template <bool               ts,
          bool               tbd,
          simd::ReconvMethod rcnv,
          typename UTRt,
          typename UTRh>
using PrimaryStream = RenderSIMDStream<kernel, C, ts, tbd, rcnv, UTRt, UTRh>;

using FavStream = PrimaryStream<false,
                                true,
                                simd::SUCCESSOR_MAJOR,
                                std::ratio<1, 16>,
                                std::ratio<1, 16>>;

template <typename R>
using mk_size =
    std::integral_constant<unsigned,
                           simd::native_vector_extent_v<float> *
                               simd::get_threshold_v<num_blocks, R>>;

constexpr unsigned stream_size25 = mk_size<std::ratio<1, 6>>::value;

int
main(int argc, char* argv[])
{
    const auto pr     = argparse::parse(make_opts(), {argc, argv});
    Logger     logger = get_logger(pr);

    run(logger,
        get_file(logger, pr),
        get_num_runs(logger, pr),
        PrimaryCompact<true, true, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PrimaryStream<true,
                      true,
                      simd::SUCCESSOR_MAJOR,
                      std::ratio<1, 16>,
                      std::ratio<1, 16>>{stream_size25},
        PrimaryFunction<true,
                        true,
                        simd::SUCCESSOR_MAJOR,
                        std::ratio<1, 16>,
                        std::ratio<1, 16>>{},
        PTScalar<true>{}
#ifdef BENCH_ALL
        ,
        PrimaryStream<false,
                      false,
                      simd::HYBRID_MAJOR,
                      std::ratio<1, 16>,
                      std::ratio<1, 16>>{stream_size25},
        FavStream{stream_size25},
        PrimaryStream<false,
                      false,
                      simd::SUCCESSOR_MAJOR,
                      std::ratio<1, 16>,
                      std::ratio<1, 16>>{stream_size25},
        FavStream{mk_size<std::ratio<1, 2>>::value},
        FavStream{mk_size<std::ratio<1, 10>>::value},
        FavStream{mk_size<std::ratio<1, 1>>::value},
        PrimaryCompact<false, true, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PrimaryCompact<true, false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PrimaryCompact<false, false, simd::HYBRID_MAJOR, std::ratio<1, 16>>{},
        PrimaryCompact<true, false, simd::SUCCESSOR_MAJOR, std::ratio<1, 16>>{},
        PrimaryCompact<false,
                       false,
                       simd::SUCCESSOR_MAJOR,
                       std::ratio<1, 16>>{},
        PTScalar<false>{}
#endif
    );
    return 0;
}
