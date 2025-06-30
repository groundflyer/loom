// -*- C++ -*-
// lang.cpp

#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Walign-mismatch"

#include "programs.hpp"

#include <lang/bse.hpp>
#include <lang/bve.hpp>
#include <lang/simd.hpp>
#include <utils/test.hpp>
#include <utils/typeinfo.hpp>

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
                                 fmt::format("[{}] BVE function introsort: {}",
                                             ti,
                                             name.view()));
            if constexpr(complex)
                return std::tuple{
                    std::move(one),
                    NamedTest(reset_bve(state.bve_func_cs),
                              fmt::format("[{}] BVE function counting sort: {}",
                                          ti,
                                          name.view())),
                    NamedTest(reset_bve(state.bve_stream_is_w),
                              fmt::format("[{}] BVE stream introsort: {}",
                                          ti,
                                          name.view())),
                    NamedTest(reset_bve(state.bve_stream_cs_w),
                              fmt::format("[{}] BVE stream counting sort: {}",
                                          ti,
                                          name.view()))};
            else
                return one;
        };

        auto get_simd_tests = [&]() noexcept
        {
            auto n = NamedTest(
                reset_bve(state.simd_func_N_is),
                fmt::format("[{}] SIMD function N-order introsort: {}",
                            ti,
                            name.view()));
            auto m = NamedTest(
                reset_bve(state.simd_func_m_is),
                fmt::format("[{}] SIMD function minikernel introsort: {}",
                            ti,
                            name.view()));

            if constexpr(complex)
                return std::tuple{
                    std::move(n),
                    std::move(m),
                    NamedTest(
                        reset_bve(state.simd_stream_N_is_w),
                        fmt::format("[{}] SIMD stream N-order introsort: {}",
                                    ti,
                                    name.view())),
                    NamedTest(
                        reset_bve(state.simd_stream_m_is_w),
                        fmt::format("[{}] SIMD stream minikernel introsort: {}",
                                    ti,
                                    name.view())),
                    NamedTest(reset_bve(state.simd_stream_N_cs_w),
                              fmt::format(
                                  "[{}] SIMD stream N-order counting_sort: {}",
                                  ti,
                                  name.view())),
                    NamedTest(
                        reset_bve(state.simd_stream_m_cs_w),
                        fmt::format(
                            "[{}] SIMD stream minikernel counting sort: {}",
                            ti,
                            name.view()))};
            else
                return std::pair{n, m};
        };

        const bool ret = track.dep_check(
            NamedTest(tm.with_ref(g, state.bse_func, v),
                      fmt::format("[{}] BSE: {}", ti, name.view())),
            get_bve_tests(),
            get_simd_tests());

        return ret;
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
        return [g_0u = gm_0u<T>(rs)]() mutable noexcept
        { return std::tuple{g_0u(), g_0u()}; };
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
    test_suites_types(all_floats{},
                      type_sequence<StandardTestSuite<simple_union_test,
                                                      GMCanon,
                                                      ::fn::val_sut_fn,
                                                      "simple union">,
                                    StandardTestSuite<complex_union_test,
                                                      GMCanon,
                                                      ::fn::val_cut_fn,
                                                      "complex union",
                                                      true>>{},
                      track,
                      tm,
                      rs);

#if 0
    auto g_int   = rs.make_gen<int>(2, 10);
    auto g_ui    = rs.make_gen<unsigned>(2, 10);
    auto g_uim   = rs.make_gen<unsigned>(0, static_cast<unsigned>(-1));
    auto g_canon = rs.make_gen<float>();
    auto g_float = rs.make_gen<float>(-1e6f, 1e6f);
    auto g_u3    = rs.make_gen<unsigned>(0, 3);
    auto g_j4    = [&]() noexcept { return std::pair{g_u3(), g_canon()}; };
    auto g_bit   = [&]() noexcept
    { return std::tuple{g_u3(), g_uim(), g_uim()}; };
    auto g_shift = [&]() noexcept { return std::pair(g_ui(), g_ui()); };
    auto g_f2    = [&]() noexcept { return std::pair{g_float(), g_float()}; };

    const unsigned vector_size = tm.num_runs;
    const unsigned stream_size = vector_size / 8;

    bse::function<incr_int>     bse_ii;
    bse::function<fac_unsigned> bse_fu;
    bse::function<fac_int>      bse_fi;
    bse::function<sut>          bse_sut;
    bse::function<cut>          bse_cut;
    bse::function<jmp4_float>   bse_jmp4;
    bse::function<choice_float> bse_choice;
    bse::function<bit_u>        bse_bit;
    bse::function<lshift_u>     bse_lshift;
    bse::function<rshift_u>     bse_rshift;

    bve::function<incr_int>     bve_ii(vector_size);
    bve::function<fac_unsigned> bve_fu(vector_size);
    bve::function<fac_int>      bve_fi(vector_size);
    bve::function<sut>          bve_sut(vector_size);
    bve::function<cut>          bve_cut(vector_size);
    bve::function<jmp4_float>   bve_jmp4(vector_size);
    bve::function<choice_float> bve_choice(vector_size);
    bve::function<bit_u>        bve_bit(vector_size);
    bve::function<lshift_u>     bve_lshift(vector_size);
    bve::function<rshift_u>     bve_rshift(vector_size);

    bve::stream<fac_unsigned> bvs_fu(stream_size);

    // counting sort bve
    bve::function<fac_unsigned, true> bve_cs_fu(vector_size);
    bve::stream<fac_unsigned, true>   bvs_cs_fu(stream_size);

    // vertical SIMD
    simd::function<incr_int>     simd_v_ii(vector_size);
    simd::function<fac_unsigned> simd_v_fu(vector_size);
    simd::function<fac_int>      simd_v_fi(vector_size);
    simd::function<sut>          simd_v_sut(vector_size);
    simd::function<cut>          simd_v_cut(vector_size);
    simd::function<jmp4_float>   simd_v_jmp4(vector_size);
    simd::function<choice_float> simd_v_choice(vector_size);
    simd::function<bit_u>        simd_v_bit(vector_size);
    simd::function<lshift_u>     simd_v_lshift(vector_size);
    simd::function<rshift_u>     simd_v_rshift(vector_size);
    simd::function<abs_float>    simd_v_abs(vector_size);
    simd::function<trunc_float>  simd_v_trunc(vector_size);
    simd::function<ceilfloor_f>  simd_v_ceilfloor(vector_size);
    simd::function<minmax_float> simd_v_minmax(vector_size);

    simd::stream<fac_unsigned> simds_v_fu(stream_size);

    // minikernel SIMD
    simd::function<incr_int, true>     simd_k_ii(vector_size);
    simd::function<fac_unsigned, true> simd_k_fu(vector_size);
    simd::function<fac_int, true>      simd_k_fi(vector_size);
    simd::function<sut, true>          simd_k_sut(vector_size);
    simd::function<cut, true>          simd_k_cut(vector_size);
    simd::function<jmp4_float, true>   simd_k_jmp4(vector_size);
    simd::function<choice_float, true> simd_k_choice(vector_size);
    simd::function<bit_u, true>        simd_k_bit(vector_size);
    simd::function<lshift_u, true>     simd_k_lshift(vector_size);
    simd::function<rshift_u, true>     simd_k_rshift(vector_size);
    simd::function<abs_float, true>    simd_k_abs(vector_size);
    simd::function<trunc_float, true>  simd_k_trunc(vector_size);
    simd::function<ceilfloor_f, true>  simd_k_ceilfloor(vector_size);
    simd::function<minmax_float, true> simd_k_minmax(vector_size);

    simd::stream<fac_unsigned, true> simds_k_fu(stream_size);

    // counting sort
    simd::function<fac_unsigned, false, true> simd_v_cs_fu(vector_size);
    simd::function<fac_unsigned, true, true>  simd_k_cs_fu(vector_size);
    simd::stream<fac_unsigned, false, true>   simds_v_cs_fu(stream_size);
    simd::stream<fac_unsigned, true, true>    simds_k_cs_fu(stream_size);

#define TEST_RESET_T(G, T, V)                       \
    [&]() noexcept                                  \
    {                                               \
        bool r = tm.rng_with_scalar_ref(G, T, V)(); \
        T.reset();                                  \
        return r;                                   \
    }

#define TEST_RESET_V(G, T, V)                \
    [&]() noexcept                           \
    {                                        \
        bool r = tm.rng_with_ref(G, T, V)(); \
        V.reset();                           \
        return r;                            \
    }

    // tsv - test simd vertical
    // tsk - test simd miniKernel
    auto tsv_ii = NamedTest(tm.rng_with_ref(g_int, simd_v_ii, bve_ii),
                            "SIMD Vertical increment int");
    auto tsk_ii = NamedTest(tm.rng_with_ref(g_int, simd_k_ii, bve_ii),
                            "SIMD Minikernel increment int");
    auto tsv_fu = NamedTest(TEST_RESET_V(g_ui, simd_v_fu, bve_fu),
                            "SIMD Vertical factorial unsigned");
    auto tsk_fu = NamedTest(TEST_RESET_V(g_ui, simd_k_fu, bve_fu),
                            "SIMD Minikernel factorial unsigned");
    auto tsv_cs_fu =
        NamedTest(TEST_RESET_V(g_ui, simd_v_cs_fu, bve_fu),
                  "SIMD Vertical Counting Sort factorial unsigned");
    auto tsk_cs_fu =
        NamedTest(TEST_RESET_V(g_ui, simd_k_cs_fu, bve_fu),
                  "SIMD Minikernel Counting Sort factorial unsigned");
    auto tsv_su = NamedTest(tm.rng_with_ref(g_canon, simd_v_sut, bve_sut),
                            "SIMD Vertical simple union");
    auto tsk_su = NamedTest(tm.rng_with_ref(g_canon, simd_k_sut, bve_sut),
                            "SIMD Vertical simple union");
    auto tsv_cu = NamedTest(TEST_RESET_V(g_canon, simd_v_cut, bve_cut),
                            "SIMD Vertical complex union");
    auto tsk_cu = NamedTest(TEST_RESET_V(g_canon, simd_k_cut, bve_cut),
                            "SIMD Minikernel complex union");
    auto tsv_j4 = NamedTest(TEST_RESET_V(g_j4, simd_v_jmp4, bve_jmp4),
                            "SIMD Vertical jump4");
    auto tsk_j4 = NamedTest(TEST_RESET_V(g_j4, simd_k_jmp4, bve_jmp4),
                            "SIMD Minikernel jump4");
    auto tsv_ch = NamedTest(tm.rng_with_ref(g_j4, simd_v_choice, bve_choice),
                            "SIMD Vertical choice");
    auto tsk_ch = NamedTest(tm.rng_with_ref(g_j4, simd_k_choice, bve_choice),
                            "SIMD Minikernel choice");
    auto tsv_bt = NamedTest(TEST_RESET_V(g_bit, simd_v_bit, bve_bit),
                            "SIMD Vertical bitwise operations");
    auto tsk_bt = NamedTest(TEST_RESET_V(g_bit, simd_k_bit, bve_bit),
                            "SIMD Minikernel bitwise operations");
    auto tsv_ls = NamedTest(tm.rng_with_ref(g_shift, simd_v_lshift, bve_lshift),
                            "SIMD Vertical left shift");
    auto tsk_ls = NamedTest(tm.rng_with_ref(g_shift, simd_k_lshift, bve_lshift),
                            "SIMD Minikernel left shift");
    auto tsv_rs = NamedTest(tm.rng_with_ref(g_shift, simd_v_rshift, bve_rshift),
                            "SIMD Vertical right shift");
    auto tsk_rs = NamedTest(tm.rng_with_ref(g_shift, simd_k_rshift, bve_rshift),
                            "SIMD Minikernel right shift");

    auto tsv_abs =
        NamedTest(tm.rng_invariant(g_float, simd_v_abs, std::identity{}),
                  "SIMD Vertical abs");
    auto tsk_abs =
        NamedTest(tm.rng_invariant(g_float, simd_k_abs, std::identity{}),
                  "SIMD Minikernel abs");

    auto tsv_trunc =
        NamedTest(tm.rng_invariant(g_float, simd_v_trunc, std::identity{}),
                  "SIMD Vertical trunc");
    auto tsk_trunc =
        NamedTest(tm.rng_invariant(g_float, simd_k_trunc, std::identity{}),
                  "SIMD Minikernel trunc");

    auto tsv_ceilfloor =
        NamedTest(tm.rng_invariant(g_float, simd_v_ceilfloor, std::identity{}),
                  "SIMD Vertical ceil/floor");
    auto tsk_ceilfloor =
        NamedTest(tm.rng_invariant(g_float, simd_k_ceilfloor, std::identity{}),
                  "SIMD Minikernel ceil/floor");

    auto tsv_minmax =
        NamedTest(tm.rng_invariant(g_f2, simd_v_minmaxa, std::identity{}),
                  "SIMD Vertical min/max");
    auto tsk_minmax =
        NamedTest(tm.rng_invariant(g_f2, simd_k_minmax, std::identity{}),
                  "SIMD Minikernel min/max");

    // tssv - test stream simd vertical
    // tssk - test stream simd minikernel
    auto tssv_fu =
        NamedTest(TEST_RESET_V(g_ui, wrap_stream(simds_v_fu), bve_fu),
                  "SIMD Vertical stream factorial unsigned");
    auto tssk_fu =
        NamedTest(TEST_RESET_V(g_ui, wrap_stream(simds_k_fu), bve_fu),
                  "SIMD Minikernel stream factorial unsigned");
    auto tssv_cs_fu =
        NamedTest(TEST_RESET_V(g_ui, wrap_stream(simds_v_cs_fu), bve_fu),
                  "SIMD Vertical Counting Sort stream factorial unsigned");
    auto tssk_cs_fu =
        NamedTest(TEST_RESET_V(g_ui, wrap_stream(simds_k_cs_fu), bve_fu),
                  "SIMD Minikernel Counting Sort stream factorial unsigned");

    auto ft_ii = std::pair{tsv_ii, tsk_ii};
    auto ft_fu = std::tuple{tsv_fu,
                            tsk_fu,
                            tssv_fu,
                            tssk_fu,
                            tsv_cs_fu,
                            tsk_cs_fu,
                            tssv_cs_fu,
                            tssk_cs_fu};
    auto ft_su = std::pair{tsv_su, tsk_su};
    auto ft_cu = std::pair{tsv_cu, tsk_cu};
    auto ft_j4 = std::pair{tsv_j4, tsk_j4};
    auto ft_ch = std::pair{tsv_ch, tsk_ch};
    auto ft_bt = std::pair{tsv_bt, tsk_bt};
    auto ft_ls = std::pair{tsv_ls, tsk_ls};
    auto ft_rs = std::pair{tsv_rs, tsk_rs};

    track.dep_check(
        NamedTest(tm.with_ref(g_int, bse_ii, val_inc), "BSE increment int"),
        NamedTest(tm.rng_with_scalar_ref(g_int, bve_ii, bse_ii),
                  "BVE increment int"),
        ft_ii);

    track.dep_check(
        NamedTest(tm.with_ref(g_shift, bse_lshift, val_lshift),
                  "BSE left shift"),
        NamedTest(tm.rng_with_scalar_ref(g_shift, bve_lshift, bse_lshift),
                  "BVE left shift"),
        ft_ls);

    track.dep_check(
        NamedTest(tm.with_ref(g_shift, bse_rshift, val_rshift),
                  "BSE right shift"),
        NamedTest(tm.rng_with_scalar_ref(g_shift, bve_rshift, bse_rshift),
                  "BVE right shift"),
        ft_rs);

    track.dep_check(
        std::pair{
            NamedTest(tm.with_ref(g_int, bse_fi, val_fac), "BSE factorial int"),
            NamedTest(tm.with_ref(g_ui, bse_fu, val_fac),
                      "BSE factorial unsigned")},
        std::tuple{NamedTest(TEST_RESET_T(g_ui, bve_fu, bse_fu),
                             "BVE factorial unsigned"),
                   NamedTest(TEST_RESET_V(g_ui, wrap_stream(bvs_fu), bve_fu),
                             "BVE Stream factorial unsigned"),
                   NamedTest(TEST_RESET_T(g_ui, bve_cs_fu, bse_fu),
                             "BVE Counting Sort factorial unsigned"),
                   NamedTest(TEST_RESET_V(g_ui, wrap_stream(bvs_cs_fu), bve_fu),
                             "BVE Stream Counting Sort factorial unsigned")},
        ft_fu);

    track.dep_check(
        NamedTest(tm.with_ref(g_canon, bse_sut, val_sut), "BSE simple union"),
        NamedTest(tm.rng_with_scalar_ref(g_canon, bve_sut, bse_sut),
                  "BVE simple union"),
        ft_su);

    track.dep_check(
        NamedTest(tm.with_ref(g_canon, bse_cut, val_cut), "BSE complex union"),
        NamedTest(TEST_RESET_T(g_canon, bve_cut, bse_cut), "BVE complex union"),
        ft_cu);

    track.dep_check(
        NamedTest(tm.with_ref(g_j4, bse_jmp4, val_jmp4), "BSE jump4"),
        NamedTest(TEST_RESET_T(g_j4, bve_jmp4, bse_jmp4), "BVE jump4"),
        ft_j4);

    track.dep_check(
        NamedTest(tm.with_ref(g_j4, bse_choice, val_choice), "BSE choice"),
        NamedTest(tm.rng_with_scalar_ref(g_j4, bve_choice, bse_choice),
                  "BVE choice"),
        ft_ch);

    track.dep_check(
        NamedTest(tm.with_ref(g_bit, bse_bit, val_bit),
                  "BSE bitwise operations"),
        NamedTest(
            [&]() noexcept
            {
                bool r = tm.rng_with_scalar_ref(g_bit, bve_bit, bse_bit)();
                bve_bit.reset();
                return r;
            },
            "BVE bitwise operations"),
        ft_bt);

    track.check(tsv_abs);
    track.check(tsk_abs);
    track.check(tsv_trunc);
    track.check(tsk_trunc);
    track.check(tsv_ceilfloor);
    track.check(tsk_ceilfloor);
    track.check(tsv_minmax);
    track.check(tsk_minmax);

#endif

    return track.ret_code();
}
