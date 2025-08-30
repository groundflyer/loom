// -*- C++ -*-
// lang.cpp

#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Walign-mismatch"

#include "programs.hpp"
#include "programs_generators.hpp"
#include "stream_wrapper.hpp"
#include "vm_test_suite_utils.hpp"

#include <utils/test.hpp>
#include <utils/typeinfo.hpp>
#include <vm/bse.hpp>
#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <fmt/format.h>

#include <limits>

using namespace lucid;
using namespace lucid::test;

template <typename P, simd::STITCH stitch, bve::SORT sort_method, bool async>
struct StreamPair
{
    std::string
    get_name() const noexcept
    {
        return fmt::format("SIMD stream {} {}{}",
                           stitch_name<stitch>::chars,
                           sort_name<sort_method>::chars,
                           async ? " async" : "");
    }

    using stream_type = simd::stream<P, stitch, sort_method, async>;

    stream_type                stream;
    StreamWrapper<stream_type> wrapper{&stream};

    StreamPair(unsigned n) noexcept : stream(n) {}
};

template <typename P,
          bve::SORT __sort,
          bool      __async,
          simd::STITCH... __stitches>
struct stream_product_stitches
{
    using type = type_sequence<StreamPair<P, __stitches, __sort, __async>...>;
};

template <typename P, bool __async, bve::SORT... __sorts>
struct stream_product_sorts
{
    using type =
        type_sequence<typename stream_product_stitches<P,
                                                       __sorts,
                                                       __async,
                                                       simd::N_ORDER,
                                                       simd::MINIKERNEL,
                                                       simd::MASKED>::type...>;
};

template <typename P, bool... __asyncs>
struct stream_product_asyncs
{
    using type = type_sequence<
        typename stream_product_sorts<P,
                                      __asyncs,
                                      bve::INTROSORT,
                                      bve::COUNTING_SORT>::type...>;
};

template <typename P>
struct stream_product
{
    using streams =
        typelist::flatten<typename stream_product_asyncs<P, false, true>::type>;

    using tuple_type = typelist::to<streams, unique_tuple>;
};

template <typename P, bool complex>
struct StandardState;

template <typename P>
struct StandardState<P, false>
{
    bse::function<P>                       bse_func;
    bve::function<P, bve::SORT::INTROSORT> bve_func_is;

    simd::function<P, simd::N_ORDER, bve::INTROSORT>    simd_func_N_is;
    simd::function<P, simd::MINIKERNEL, bve::INTROSORT> simd_func_m_is;

    StandardState(unsigned n) noexcept :
        bve_func_is(n), simd_func_N_is(n), simd_func_m_is(n)
    {
    }
};

template <typename P>
struct StandardState<P, true> : StandardState<P, false>
{
    static constexpr unsigned d = 8; // divisor

    bve::function<P, bve::COUNTING_SORT> bve_func_cs;

    bve::stream<P, bve::INTROSORT>     bve_stream_is;
    bve::stream<P, bve::COUNTING_SORT> bve_stream_cs;

    StreamWrapper<bve::stream<P, bve::INTROSORT>> bve_stream_is_w{
        &bve_stream_is};
    StreamWrapper<bve::stream<P, bve::COUNTING_SORT>> bve_stream_cs_w{
        &bve_stream_cs};

    simd::function<P, simd::N_ORDER, bve::COUNTING_SORT>    simd_func_N_cs;
    simd::function<P, simd::MINIKERNEL, bve::COUNTING_SORT> simd_func_m_cs;

    using sp = stream_product<P>;

    typename sp::tuple_type simd_streams;

    using base = StandardState<P, false>;

    StandardState(unsigned n) noexcept :
        base(n), bve_func_cs(n), bve_stream_is(n / d), bve_stream_cs(n / d),
        simd_func_N_cs(n), simd_func_m_cs(n), simd_streams(uniform_init, n / d)
    {
    }
};

template <template <typename> typename P,
          template <typename> typename GM,
          typename V,
          StringLiteral name,
          bool          do_reset = false,
          bool          complex  = false>
struct StandardTestSuite
{
    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        using p_t          = P<T>;
        constexpr auto  ti = TypeInfo<T>::chars;
        constexpr GM<T> gm{};
        constexpr V     v{};

        const unsigned vector_size = tm.num_runs;

        StandardState<p_t, complex> state(vector_size);

        auto g = gm(rs);

        auto get_tfn = [&]<typename Test>(Test&& t) noexcept
        {
            if constexpr(std::is_same_v<std::remove_cvref_t<Test>,
                                        bve::function<p_t, bve::INTROSORT>> ||
                         std::is_same_v<std::remove_cvref_t<Test>,
                                        bve::function<p_t, bve::COUNTING_SORT>>)
                return tm.rng_with_scalar_ref(g,
                                              state.bve_func_is,
                                              state.bse_func);
            else
                return tm.rng_with_ref(g,
                                       std::forward<Test>(t),
                                       state.bve_func_is);
        };

        auto reset_bve = [&](auto&& t) noexcept
        {
            auto tfn = get_tfn(t);
            if constexpr(do_reset || complex)
                return [&state, tfn = std::move(tfn)]() noexcept
                {
                    bool r = tfn();
                    state.bve_func_is.reset();
                    return r;
                };
            else
                return tfn;
        };

        auto get_bve_tests = [&]() noexcept
        {
            auto one = NamedTest(reset_bve(state.bve_func_is),
                                 fmt::format("{}: BVE function introsort [{}]",
                                             name.view(),
                                             ti));
            if constexpr(complex)
                return std::tuple{
                    std::move(one),
                    NamedTest(reset_bve(state.bve_func_cs),
                              fmt::format("{}: BVE function counting sort [{}]",

                                          name.view(),
                                          ti)),
                    NamedTest(reset_bve(state.bve_stream_is_w),
                              fmt::format("{}: BVE stream introsort [{}]",

                                          name.view(),
                                          ti)),
                    NamedTest(reset_bve(state.bve_stream_cs_w),
                              fmt::format("{}: BVE stream counting sort [{}]",

                                          name.view(),
                                          ti))};
            else
                return one;
        };

        auto get_simd_tests = [&]() noexcept
        {
            auto n = NamedTest(
                reset_bve(state.simd_func_N_is),
                fmt::format("{}: SIMD function N-order introsort [{}]",

                            name.view(),
                            ti));
            auto m = NamedTest(
                reset_bve(state.simd_func_m_is),
                fmt::format("{}: SIMD function minikernel introsort [{}]",

                            name.view(),
                            ti));

            if constexpr(complex)
                return std::tuple_cat(
                    std::tuple{std::move(n), std::move(m)},
                    lucid::map_tuple(state.simd_streams,
                                     [&](const auto& sp) noexcept
                                     {
                                         return NamedTest(
                                             reset_bve(sp.wrapper),
                                             fmt::format("{}: {} [{}]",
                                                         name.view(),
                                                         sp.get_name(),
                                                         ti));
                                     }));
            else
                return std::pair{n, m};
        };

        return track.dep_check(
            NamedTest(tm.with_ref(g, state.bse_func, v),
                      fmt::format("{}: BSE [{}]", name.view(), ti)),
            get_bve_tests(),
            get_simd_tests());
    }
};

template <template <typename> typename P,
          template <typename> typename GM,
          StringLiteral name>
struct SimpleSIMDSuite
{
    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        using p_t         = P<T>;
        constexpr auto ti = TypeInfo<T>::chars;

        constexpr GM<T> gm{};
        auto            g = gm(rs);

        const unsigned vector_size = tm.num_runs;

        simd::function<p_t, simd::N_ORDER, bve::INTROSORT> simd_N(vector_size);
        simd::function<p_t, simd::MINIKERNEL, bve::INTROSORT> simd_m(
            vector_size);

        return track.check(NamedTest(
                   tm.rng_invariant(g, simd_N),
                   fmt::format("{}: SIMD N-order [{}]", name.view(), ti))) &&
               track.check(NamedTest(
                   tm.rng_invariant(g, simd_m),
                   fmt::format("{}: SIMD minikernel [{}]", name.view(), ti)));
    }
};

int
main(int argc, char* argv[])
{
    simd::print_info();
    Track         track;
    ParmTestMaker tm{align_top(parse_numruns(argc, argv), 32u)};
    RandomState   rs;

    test_suite_types(
        typelist::join<all_ints, all_uints>{},
        StandardTestSuite<increment, GM_Incr, ::fn::val_inc_fn, "increment">{},
        track,
        tm,
        rs);
    test_suite_types(fac_ints{},
                     StandardTestSuite<factorial_int,
                                       GM_2_10,
                                       ::fn::val_fac_fn,
                                       "factorial",
                                       true,
                                       true>{},
                     track,
                     tm,
                     rs);
    {
        StandardTestSuite<fib,
                          GM_FIB,
                          ::fn::val_fib_fn,
                          "fibonacci",
                          true,
                          true>
                      test;
        test.template operator()<std::uint64_t>(track, tm, rs);
    }
    test_suites_types(
        all_floats{},
        type_sequence<StandardTestSuite<simple_union_test,
                                        GMCanon,
                                        ::fn::val_sut_fn,
                                        "simple union">,
                      StandardTestSuite<complex_union_test,
                                        GMCanon,
                                        ::fn::val_cut_fn,
                                        "complex union",
                                        true>,
                      StandardTestSuite<jmp4_test,
                                        GM_J4,
                                        ::fn::val_jmp4_fn,
                                        "multi-way (4) jump",
                                        true>,
                      StandardTestSuite<choice_test,
                                        GM_J4,
                                        ::fn::val_choice_fn,
                                        "choice (conditional move)">>{},
        track,
        tm,
        rs);
    test_suite_types(all_uints{},
                     StandardTestSuite<bit_test,
                                       GM_Bit,
                                       ::fn::val_bit_fn,
                                       "bit (and, or, xor, not)",
                                       true>{},
                     track,
                     tm,
                     rs);
    test_suites_types(
        type_sequence<std::uint16_t, std::uint32_t, std::uint64_t>{},
        type_sequence<StandardTestSuite<lshift_test,
                                        GM_Shift,
                                        ::fn::val_lshift_fn,
                                        "left shift">,
                      StandardTestSuite<rshift_test,
                                        GM_Shift,
                                        ::fn::val_rshift_fn,
                                        "right shift">>{},
        track,
        tm,
        rs);
    test_suites_types(
        all_floats{},
        type_sequence<SimpleSIMDSuite<abs_test, GMFloat, "abs">,
                      SimpleSIMDSuite<trunc_test, GMFloat, "trunc">,
                      SimpleSIMDSuite<ceilfloor, GMFloat, "ceil/floor">,
                      SimpleSIMDSuite<minmax_test, GM_F2, "min/max">>{},
        track,
        tm,
        rs);

    return track.ret_code();
}
