// -*- C++ -*-
// vm.cpp

#include "generators.hpp"
#include "programs.hpp"
#include "stream_wrapper.hpp"
#include "util.hpp"
#include "vm_test_suite_utils.hpp"

#include <common/typeinfo.hpp>
#include <vm/bse.hpp>
#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <fmt/format.h>

#include <limits>

using namespace loom;
using namespace vm;
using namespace test;

template <Kernel K, simd::STITCH stitch, simd::SORT sort_method, bool async>
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

    using stream_type =
        simd::stream<K, stitch, sort_method, async, std::ratio<1, 2>>;

    stream_type                stream;
    StreamWrapper<stream_type> wrapper{&stream};

    StreamPair(unsigned n) noexcept : stream(n) {}
};

template <Kernel K, simd::SORT __sort, bool __async, simd::STITCH... __stitches>
struct stream_product_stitches
{
    using type = type_sequence<StreamPair<K, __stitches, __sort, __async>...>;
};

template <Kernel K, bool __async, simd::SORT... __sorts>
struct stream_product_sorts
{
    using type =
        type_sequence<typename stream_product_stitches<K,
                                                       __sorts,
                                                       __async,
                                                       simd::WAVEFRONT,
                                                       simd::MINIKERNEL,
                                                       simd::MASKED>::type...>;
};

template <Kernel K, bool... __asyncs>
struct stream_product_asyncs
{
    using type = type_sequence<
        typename stream_product_sorts<K,
                                      __asyncs,
                                      simd::bve_INTROSORT,
                                      simd::bve_COUNTING_SORT,
                                      simd::COUNTING_SORT>::type...>;
};

template <Kernel K>
struct stream_product
{
    using streams =
        typelist::flatten<typename stream_product_asyncs<K, false, true>::type>;

    using tuple_type = typelist::to<streams, unique_tuple>;
};

template <Kernel K, bool complex>
struct StandardState;

template <Kernel K>
struct StandardState<K, false>
{
    bse::function<K>                       bse_func;
    bve::function<K, bve::SORT::INTROSORT> bve_func_is;

    simd::function<K, simd::WAVEFRONT, simd::bve_INTROSORT>  simd_func_N_is;
    simd::function<K, simd::MINIKERNEL, simd::bve_INTROSORT> simd_func_m_is;

    StandardState(unsigned n) noexcept :
        bve_func_is(n), simd_func_N_is(n), simd_func_m_is(n)
    {
    }
};

template <Kernel K>
struct StandardState<K, true> : StandardState<K, false>
{
    static constexpr unsigned d = 8; // divisor

    bve::function<K, bve::COUNTING_SORT> bve_func_cs;

    bve::stream<K, bve::INTROSORT>     bve_stream_is;
    bve::stream<K, bve::COUNTING_SORT> bve_stream_cs;

    StreamWrapper<bve::stream<K, bve::INTROSORT>> bve_stream_is_w{
        &bve_stream_is};
    StreamWrapper<bve::stream<K, bve::COUNTING_SORT>> bve_stream_cs_w{
        &bve_stream_cs};

    simd::function<K, simd::WAVEFRONT, simd::COUNTING_SORT>    simd_func_N_cs;
    simd::function<K, simd::MINIKERNEL, simd::COUNTING_SORT> simd_func_m_cs;

    using sp = stream_product<K>;

    typename sp::tuple_type simd_streams;

    using base = StandardState<K, false>;

    StandardState(unsigned n) noexcept :
        base(n), bve_func_cs(n), bve_stream_is(n / d), bve_stream_cs(n / d),
        simd_func_N_cs(n), simd_func_m_cs(n), simd_streams(uniform_init, n / d)
    {
    }
};

template <template <typename> typename KT,
          template <typename> typename GM,
          typename V,
          bool do_reset = false,
          bool complex  = false>
struct StandardTestSuite
{
    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        using k_t                           = KT<T>;
        constexpr auto             ti       = TypeInfo<T>::chars;
        constexpr std::string_view nameview = k_t::nameview;
        constexpr GM<T>            gm{};
        constexpr V                v{};

        const unsigned vector_size = tm.num_runs;

        StandardState<k_t, complex> state(vector_size);

        auto g = gm(rs);

        auto get_tfn = [&]<typename Test>(Test&& t) noexcept
        {
            if constexpr(std::is_same_v<std::remove_cvref_t<Test>,
                                        bve::function<k_t, bve::INTROSORT>> ||
                         std::is_same_v<std::remove_cvref_t<Test>,
                                        bve::function<k_t, bve::COUNTING_SORT>>)
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
            auto one = NamedTest(
                reset_bve(state.bve_func_is),
                fmt::format("{}: BVE function introsort [{}]", nameview, ti));
            if constexpr(complex)
                return std::tuple{
                    std::move(one),
                    NamedTest(reset_bve(state.bve_func_cs),
                              fmt::format("{}: BVE function counting sort [{}]",

                                          nameview,
                                          ti)),
                    NamedTest(reset_bve(state.bve_stream_is_w),
                              fmt::format("{}: BVE stream introsort [{}]",

                                          nameview,
                                          ti)),
                    NamedTest(reset_bve(state.bve_stream_cs_w),
                              fmt::format("{}: BVE stream counting sort [{}]",

                                          nameview,
                                          ti))};
            else
                return one;
        };

        auto get_simd_tests = [&]() noexcept
        {
            auto n = NamedTest(
                reset_bve(state.simd_func_N_is),
                fmt::format("{}: SIMD function N-order introsort [{}]",

                            nameview,
                            ti));
            auto m = NamedTest(
                reset_bve(state.simd_func_m_is),
                fmt::format("{}: SIMD function minikernel introsort [{}]",

                            nameview,
                            ti));

            if constexpr(complex)
                return std::tuple_cat(
                    std::tuple{std::move(n), std::move(m)},
                    loom::map_tuple(state.simd_streams,
                                    [&](const auto& sp) noexcept
                                    {
                                        return NamedTest(
                                            reset_bve(sp.wrapper),
                                            fmt::format("{}: {} [{}]",
                                                        nameview,
                                                        sp.get_name(),
                                                        ti));
                                    }));
            else
                return std::pair{n, m};
        };

        return track.dep_check(
            NamedTest(tm.with_ref(g, state.bse_func, v),
                      fmt::format("{}: BSE [{}]", nameview, ti)),
            get_bve_tests(),
            get_simd_tests());
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

    test_suite_types(typelist::join<all_ints, all_uints>{},
                     StandardTestSuite<increment, GM_Incr, ::fn::val_inc_fn>{},
                     track,
                     tm,
                     rs);
    test_suite_types(
        fac_ints{},
        StandardTestSuite<fac_test, GM_FAC, ::fn::val_fac_fn, true, true>{},
        track,
        tm,
        rs);
    {
        StandardTestSuite<fib_test, GM_FIB, ::fn::val_fib_fn, true, true> test;
        test.template operator()<std::uint64_t>(track, tm, rs);
    }
    test_suites_types(
        all_floats{},
        type_sequence<
            StandardTestSuite<union_test, GMCanon, ::fn::val_union1_fn>,
            StandardTestSuite<union_cmov_test, GMCanon, ::fn::val_union2_fn>>{},
        track,
        tm,
        rs);

    test_suites_types(
        fit4_uints{},
        type_sequence<
            StandardTestSuite<jmp4_test, GM_U3, ::fn::val_jmp4_fn, true>,
            StandardTestSuite<bit_and_test, GM_Bit_Bin, std::bit_and<>>,
            StandardTestSuite<bit_or_test, GM_Bit_Bin, std::bit_or<>>,
            StandardTestSuite<bit_xor_test, GM_Bit_Bin, std::bit_xor<>>,
            StandardTestSuite<
                bit_not_test,
                GM_0U,
                /* std::bit_not<> libc++ bug */ ::fn::val_bit_not_fn>,
            StandardTestSuite<bool_and_test, GM_Bool_Bin, std::logical_and<>>,
            StandardTestSuite<bool_or_test, GM_Bool_Bin, std::logical_or<>>,
            StandardTestSuite<bool_not_test, GM_Bool, std::logical_not<>>>{},
        track,
        tm,
        rs);

    test_suites_types(
        type_sequence<std::uint16_t, std::uint32_t, std::uint64_t>{},
        type_sequence<
            StandardTestSuite<lshift_test, GM_Shift, ::fn::val_lshift_fn>,
            StandardTestSuite<rshift_test, GM_Shift, ::fn::val_rshift_fn>>{},
        track,
        tm,
        rs);

    return track.ret_code();
}
