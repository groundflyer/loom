// -*- C++ -*-
// vm_simd.cpp

#include "generators.hpp"
#include "programs.hpp"
#include "vm_test_suite_utils.hpp"

#include <common/test.hpp>
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
    simd::print_info();
    Track         track;
    ParmTestMaker tm{align_up(parse_numruns(argc, argv), 32u)};
    RandomState   rs;

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

    test_suites_types(all_uints{},
                      type_sequence<SimpleSIMDSuite<sct_inv_test, GM_U3>,
                                    SimpleSIMDSuite<slt_inv_test, GM_U3>>{},
                      track,
                      tm,
                      rs);

    return track.ret_code();
}
