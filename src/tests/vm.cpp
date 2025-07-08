// -*- C++ -*-
// lang.cpp

#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Walign-mismatch"

#include "programs.hpp"

#include <utils/test.hpp>
#include <utils/typeinfo.hpp>
#include <vm/bse.hpp>
#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <fmt/format.h>

#include <limits>

using namespace lucid;
using namespace lucid::test;

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

    simd::function<P, simd::N_ORDER, bve::COUNTING_SORT>    simd_func_N_cs;
    simd::function<P, simd::MINIKERNEL, bve::COUNTING_SORT> simd_func_m_cs;

    simd::stream<P, simd::N_ORDER, bve::INTROSORT>    simd_stream_N_is;
    simd::stream<P, simd::MINIKERNEL, bve::INTROSORT> simd_stream_m_is;

    simd::stream<P, simd::N_ORDER, bve::COUNTING_SORT>    simd_stream_N_cs;
    simd::stream<P, simd::MINIKERNEL, bve::COUNTING_SORT> simd_stream_m_cs;

    StreamWrapper<bve::stream<P, bve::INTROSORT>> bve_stream_is_w{
        &bve_stream_is};
    StreamWrapper<bve::stream<P, bve::COUNTING_SORT>> bve_stream_cs_w{
        &bve_stream_cs};

    StreamWrapper<simd::stream<P, simd::N_ORDER, bve::INTROSORT>>
        simd_stream_N_is_w{&simd_stream_N_is};
    StreamWrapper<simd::stream<P, simd::MINIKERNEL, bve::INTROSORT>>
        simd_stream_m_is_w{&simd_stream_m_is};

    StreamWrapper<simd::stream<P, simd::N_ORDER, bve::COUNTING_SORT>>
        simd_stream_N_cs_w{&simd_stream_N_cs};
    StreamWrapper<simd::stream<P, simd::MINIKERNEL, bve::COUNTING_SORT>>
        simd_stream_m_cs_w{&simd_stream_m_cs};

    using base = StandardState<P, false>;

    StandardState(unsigned n) noexcept :
        base(n), bve_func_cs(n), bve_stream_is(n / d), bve_stream_cs(n / d),
        simd_func_N_cs(n), simd_func_m_cs(n), simd_stream_N_is(n / d),
        simd_stream_m_is(n / d), simd_stream_N_cs(n / d),
        simd_stream_m_cs(n / d)
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
                return std::tuple{
                    std::move(n),
                    std::move(m),
                    NamedTest(
                        reset_bve(state.simd_stream_N_is_w),
                        fmt::format("{}: SIMD stream N-order introsort [{}]",

                                    name.view(),
                                    ti)),
                    NamedTest(
                        reset_bve(state.simd_stream_m_is_w),
                        fmt::format("{}: SIMD stream minikernel introsort [{}]",

                                    name.view(),
                                    ti)),
                    NamedTest(reset_bve(state.simd_stream_N_cs_w),
                              fmt::format(
                                  "{}: SIMD stream N-order counting_sort [{}]",

                                  name.view(),
                                  ti)),
                    NamedTest(
                        reset_bve(state.simd_stream_m_cs_w),
                        fmt::format(
                            "{}: SIMD stream minikernel counting sort [{}]",

                            name.view(),
                            ti))};
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

template <typelist::List Types, typename Suite>
void
test_suite_types(Types         types,
                 Suite         suite,
                 Track&        track,
                 ParmTestMaker tm,
                 RandomState&  rs) noexcept
{
    typelist::for_each(types, suite, track, tm, rs);
}

template <typelist::List Types, typelist::List Suites>
void
test_suites_types(Types         types,
                  Suites        suites,
                  Track&        track,
                  ParmTestMaker tm,
                  RandomState&  rs) noexcept
{
    typelist::for_each(
        suites,
        []<typename Suite>(Types         types,
                           Track&        track,
                           ParmTestMaker tm,
                           RandomState&  rs) noexcept
        { test_suite_types(types, Suite{}, track, tm, rs); },
        types,
        track,
        tm,
        rs);
}

template <std::integral T, T min, T max>
struct GMIntRange
{
    auto
    operator()(RandomState& rs) const noexcept
    {
        return rs.make_gen<T>(min, max);
    }
};

template <std::floating_point T>
struct GMCanon
{
    auto
    operator()(RandomState& rs) const noexcept
    {
        return rs.make_gen<T>();
    }
};

template <std::floating_point T>
struct GMFloat
{
    static constexpr T l = 1e-6;
    static constexpr T h = 1e6;

    auto
    operator()(RandomState& rs) const noexcept
    {
        return rs.make_gen<T>(l, h);
    }
};

template <std::integral T>
using GM_2_10 = GMIntRange<T, 2, 10>;

template <std::integral T>
using GM_Incr = GMIntRange<T,
                           std::numeric_limits<T>::lowest(),
                           std::numeric_limits<T>::max() - 1>;

template <std::unsigned_integral T>
using GM_0U = GMIntRange<T, 0, static_cast<T>(-1)>;

template <std::unsigned_integral T>
using GM_U3 = GMIntRange<T, 0, 3>;

template <std::unsigned_integral T>
    requires(::fn::val_fib(T(80)) < std::numeric_limits<T>::max())
using GM_FIB = GMIntRange<T, 0, 80>;

template <std::floating_point T>
inline constexpr GMCanon<T> gm_canon{};

template <std::floating_point T>
inline constexpr GMFloat<T> gm_float{};

template <std::integral T>
inline constexpr GM_2_10<T> gm_2_10{};

template <std::unsigned_integral T>
inline constexpr GM_0U<T> gm_0u{};

template <std::unsigned_integral T>
inline constexpr GM_U3<T> gm_u3{};

template <std::floating_point T>
struct GM_J4
{
    auto
    operator()(RandomState& rs) const noexcept
    {
        return [g_u3    = gm_u3<lucid::uint_of_size_t<sizeof(T)>>(rs),
                g_canon = gm_canon<T>(rs)]() mutable noexcept
        { return std::pair{g_u3(), g_canon()}; };
    }
};

template <std::integral T>
struct GM_Bit
{
    using U = lucid::uint_of_size_t<sizeof(T)>;

    auto
    operator()(RandomState& rs) const noexcept
    {
        return [g_u3 = gm_u3<U>(rs), g_0u = gm_0u<U>(rs)]() mutable noexcept
        { return std::tuple{g_u3(), g_0u(), g_0u()}; };
    }
};

template <std::unsigned_integral T>
struct GM_Shift
{
    auto
    operator()(RandomState& rs) const noexcept
    {
        return [g = gm_2_10<T>(rs)]() mutable noexcept
        { return std::tuple{g(), g()}; };
    }
};

template <std::floating_point T>
struct GM_F2
{
    auto
    operator()(RandomState& rs) const noexcept
    {
        return [g_f = gm_float<T>(rs)]() mutable noexcept
        { return std::pair{g_f(), g_f()}; };
    }
};

using all_ints =
    type_sequence<std::int8_t, std::int16_t, std::int32_t, std::int64_t>;
using all_uints =
    type_sequence<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>;
using fac_ints =
    type_sequence<std::int32_t, std::int64_t, std::uint32_t, std::uint64_t>;
using all_floats = type_sequence<float, double>;

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
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
