// -*- C++ -*-
// vm_simd.cpp

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

#include "generators.hpp"
#include "programs.hpp"
#include "util.hpp"
#include "vm_test_suite_utils.hpp"

#include <common/typeinfo.hpp>
#include <vm/simd.hpp>

using namespace loom;
using namespace vm;
using namespace test;

template <template <typename> typename KT, template <typename> typename GM>
struct SimpleSIMDSuite
{
    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        using k_t                           = KT<T>;
        constexpr auto             ti       = TypeInfo<T>::chars;
        constexpr std::string_view nameview = k_t::nameview;

        constexpr GM<T> gm{};
        auto            g = gm(rs);

        const unsigned vector_size = tm.num_runs;

        simd::function<k_t, simd::WAVEFRONT, simd::bve_INTROSORT> simd_N(
            vector_size);
        simd::function<k_t, simd::MINIKERNEL, simd::bve_INTROSORT> simd_m(
            vector_size);

        return track.check(NamedTest(
                   tm.rng_invariant(g, simd_N),
                   fmt::format("{}: SIMD N-order [{}]", nameview, ti))) &&
               track.check(NamedTest(
                   tm.rng_invariant(g, simd_m),
                   fmt::format("{}: SIMD minikernel [{}]", nameview, ti)));
    }
};

int
main(int argc, char* argv[])
{
    const auto [nr, seed] = parse_test_args(argc, argv);
    Track         track;
    ParmTestMaker tm{align_up(nr, 32u)};
    RandomState   rs{seed};

    fmt::print("Using x86-64 vector extensions: {}\n", simd::info::value);

    test_suites_types(
        all_floats{},
        type_sequence<SimpleSIMDSuite<abs_test, GMFloat>,
                      SimpleSIMDSuite<trunc_test, GMFloat>,
                      SimpleSIMDSuite<ceilfloor, GMFloat>,
                      SimpleSIMDSuite<minmax_test, GM_F2>,
                      SimpleSIMDSuite<arithmetic_test, GM_F3>,
                      SimpleSIMDSuite<sin_cos_test, GMTrig>,
                      SimpleSIMDSuite<sqrt_test, GMFloatPositive>,
                      SimpleSIMDSuite<rsqrt_test, GMFloatPositive>>{},
        track,
        tm,
        rs);

    test_suites_types(fit4_uints{},
                      type_sequence<SimpleSIMDSuite<sct_inv_test, GM_U3>,
                                    SimpleSIMDSuite<slt_inv_test, GM_U3>>{},
                      track,
                      tm,
                      rs);

    return track.ret_code();
}
