// -*- C++ -*-
// vm_embed.cpp
#include "generators.hpp"
#include "programs.hpp"
#include "root_finding.hpp"
#include "util.hpp"
#include "vm_test_suite_utils.hpp"

#include <array>
#include <set>

#include <fmt/core.h>

using namespace loom;
using namespace vm;
using namespace test;

template <Kernel K>
void
test_set(Track& track, ParmTestMaker tm, RandomState& rs) noexcept
{
    using index_t = K::index_t;

    constexpr index_t     num_blocks = K::num_blocks;
    constexpr std::size_t N          = simd::native_vector_extent_v<index_t>;
    constexpr auto        ti         = TypeInfo<index_t>::chars;

    using arr_t = std::array<index_t, N>;
    using vec_t = simd::native_vector<index_t>;
    using set_t = std::set<index_t>;

    auto gen = [g = rs.make_gen<index_t>(0, num_blocks)]() mutable noexcept
    {
        arr_t ret;
        for(std::size_t i = 0; i < N; ++i) ret[i] = fit_block_index<K>(g());
        return ret;
    };

    auto test = [](const arr_t& a) noexcept
    {
        const vec_t v = a2v(a);
        return simd::greedy_groups<K, N, false>{v};
    };

    track.check(NamedTest(tm.test(gen,
                                  test,
                                  [](const auto& r, const arr_t& a) noexcept
                                  {
                                      set_t sa{a.cbegin(), a.cend()};
                                      set_t sr;
                                      for(const index_t v: r)
                                          sr.insert(fit_block_index<K>(v));
                                      return sa == sr;
                                  }),
                          fmt::format("block set: {}[{}]", ti, N)));
}

struct RootFindingSuite
{
    template <std::floating_point T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        using cnsts        = constants<T>;
        constexpr T lbound = cnsts::lbound;
        constexpr T rbound = cnsts::rbound;
        using errt         = cnsts::errt;

        constexpr std::size_t N = simd::native_vector_extent_v<T>;

        using k_t     = bisection<T, errt>;
        using types   = simd_program_types<k_t, N>;
        using in_vec  = types::input_simd;
        using out_vec = types::output_simd;
        using cmp_t   = types::cmp;

        constexpr auto  ti = TypeInfo<T>::chars;
        constexpr cmp_t cmp{};

        auto g_f  = rs.make_gen<T>(lbound, rbound);
        auto g_b  = rs.make_gen<bool>(0.5);
        auto g_a  = rs.make_gen<T>(1, rbound);
        auto g_a1 = [&]() noexcept { return g_a() * (g_b() ? T(-1) : T(1)); };
        auto g_c4 = [&]() noexcept
        { return std::tuple{g_a1(), g_f(), g_f(), g_f()}; };

        auto gen = [&]() noexcept
        {
            in_vec ret;
            for(std::size_t i = 0; i < N; ++i) types::set_input(ret, i, g_c4());
            return ret;
        };

        auto ref = [](const auto& inp) noexcept
        {
            out_vec ret;
            for(std::size_t i = 0; i < N; ++i)
                types::set_output(
                    ret,
                    i,
                    invopply(val_cubic_fz, types::get_input(inp, i)));
            return ret;
        };

        simd::compact_function<k_t> test;
        return track.check(
            NamedFuzzyTest{tm.fuzzy_with_ref(gen, test, ref, cmp),
                           fmt::format("root finding: {}[{}]", ti, N),
                           0.1});
    }
};

template <template <typename> typename P, template <typename> typename GM>
struct InvariantSIMDCompactSuite
{
    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        constexpr std::size_t N = simd::native_vector_extent_v<T>;

        using k_t    = P<T>;
        using types  = simd_program_types<k_t, N>;
        using in_vec = types::input_simd;

        constexpr std::string_view nameview = k_t::nameview;
        constexpr auto             ti       = TypeInfo<T>::chars;
        constexpr GM<T>            gm{};

        auto gs = gm(rs);

        auto gen = [&gs]() noexcept
        {
            in_vec ret;
            for(std::size_t i = 0; i < N; ++i) types::set_input(ret, i, gs());
            return ret;
        };

        simd::compact_function<k_t> test;
        return track.check(
            NamedTest{tm.invariant(gen,
                                   test,
                                   [](const auto v) noexcept -> bool
                                   { return __builtin_reduce_and(v); }),
                      fmt::format("{}: {}[{}]", nameview, ti, N)});
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

    test_set<mock_kernel<9>>(track, tm, rs);

    test_suite_types(
        typelist::join<all_ints, all_uints>{},
        RefSIMDCompactSuite<increment, GM_Incr, ::fn::val_inc_fn>{},
        track,
        tm,
        rs);

    test_suite_types(fac_ints{},
                     RefSIMDCompactSuite<fac_test, GM_FAC, ::fn::val_fac_fn>{},
                     track,
                     tm,
                     rs);

    {
        constexpr RefSIMDCompactSuite<fib_test, GM_FIB, ::fn::val_fib_fn> test;
        test.template operator()<std::uint64_t>(track, tm, rs);
    }

    test_suites_types(
        all_floats{},
        type_sequence<
            RefSIMDCompactSuite<union_test, GMCanon, ::fn::val_union1_fn>,
            RefSIMDCompactSuite<union_cmov_test,
                                GMCanon,
                                ::fn::val_union2_fn>>{},
        track,
        tm,
        rs);

    test_suites_types(
        fit4_uints{},
        type_sequence<
            RefSIMDCompactSuite<jmp4_test, GM_U3, ::fn::val_jmp4_fn>,
            RefSIMDCompactSuite<bit_and_test, GM_Bit_Bin, std::bit_and<>>,
            RefSIMDCompactSuite<bit_or_test, GM_Bit_Bin, std::bit_or<>>,
            RefSIMDCompactSuite<bit_xor_test, GM_Bit_Bin, std::bit_xor<>>,
            RefSIMDCompactSuite<bit_not_test, GM_0U, std::bit_not<>>,
            RefSIMDCompactSuite<bool_and_test, GM_Bool_Bin, std::logical_and<>>,
            RefSIMDCompactSuite<bool_or_test, GM_Bool_Bin, std::logical_or<>>,
            RefSIMDCompactSuite<bool_not_test, GM_Bool, std::logical_not<>>>{},
        track,
        tm,
        rs);

    test_suites_types(
        type_sequence<std::uint16_t, std::uint32_t, std::uint64_t>{},
        type_sequence<
            RefSIMDCompactSuite<lshift_test, GM_Shift, ::fn::val_lshift_fn>,
            RefSIMDCompactSuite<rshift_test, GM_Shift, ::fn::val_rshift_fn>>{},
        track,
        tm,
        rs);

    test_suite_types(all_floats{}, RootFindingSuite{}, track, tm, rs);

    test_suites_types(
        all_floats{},
        type_sequence<InvariantSIMDCompactSuite<abs_test, GMFloat>,
                      InvariantSIMDCompactSuite<trunc_test, GMFloat>,
                      InvariantSIMDCompactSuite<ceilfloor, GMFloat>,
                      InvariantSIMDCompactSuite<minmax_test, GM_F2>,
                      InvariantSIMDCompactSuite<arithmetic_test, GM_F3>,
                      InvariantSIMDCompactSuite<sin_cos_test, GMTrig>,
                      InvariantSIMDCompactSuite<sqrt_test, GMFloatPositive>,
                      InvariantSIMDCompactSuite<rsqrt_test, GMFloatPositive>>{},
        track,
        tm,
        rs);

    test_suites_types(
        fit4_uints{},
        type_sequence<InvariantSIMDCompactSuite<sct_inv_test, GM_U3>,
                      InvariantSIMDCompactSuite<slt_inv_test, GM_U3>>{},
        track,
        tm,
        rs);

    return track.ret_code();
}
