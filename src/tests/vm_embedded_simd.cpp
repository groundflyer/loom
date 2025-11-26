// -*- C++ -*-
// vm_embed.cpp
#include "programs.hpp"
#include "programs_generators.hpp"
#include "root_finding.hpp"
#include "vm_test_suite_utils.hpp"

#include <utils/test.hpp>
#include <utils/typeinfo.hpp>
#include <vm/simd.hpp>

#include <array>
#include <set>

#include <fmt/core.h>

using namespace loom;
using namespace vm;
using namespace test;

using all_uints =
    type_sequence<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>;

template <typename T, std::size_t N>
simd::packed_vector<T, N>
a2v(const std::array<T, N>& a) noexcept
{
    simd::packed_vector<T, N> ret;
    for(std::size_t i = 0; i < N; ++i) ret[i] = a[i];
    return ret;
}

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
        return simd::block_set<K, N>{v};
    };

    track.check(NamedTest(tm.test(gen,
                                  test,
                                  [](const auto& r, const arr_t& a) noexcept
                                  {
                                      set_t sa{a.cbegin(), a.cend()};
                                      set_t sr;
                                      for(const auto [v, _]: r) sr.insert(v);
                                      return sa == sr;
                                  }),
                          fmt::format("block set: {}[{}]", ti, N)));
}

template <typename T, std::size_t N>
struct helper
{
    using index_t     = uint_of_size_t<sizeof(T)>;
    using array_type  = std::array<T, N>;
    using vector_type = simd::packed_vector<T, N>;
    using vcmp        = simd::cmp_intrinsics_wrangler<T, N>;
    using lrh         = simd::logical_reduce_helper<index_t, N>;

    template <typename V>
    static void
    set(V& v, const std::size_t i, const T val) noexcept
    {
        v[i] = val;
    }

    template <typename V>
    [[nodiscard]] static auto
    get(const V& v, const std::size_t i) noexcept
    {
        return v[i];
    }

    struct cmp
    {
        bool
        operator()(const vector_type lhs, const vector_type rhs) const noexcept
        {
            return lrh::land(vcmp::eq(lhs, rhs));
        }
    };
};

template <typename... Ts, std::size_t N>
struct helper<std::tuple<Ts...>, N>
{
    using array_type  = std::tuple<typename helper<Ts, N>::array_type...>;
    using vectors_t   = type_sequence<typename helper<Ts, N>::vector_type...>;
    using vector_type = typelist::to<vectors_t, std::tuple>;
    using helpers     = type_sequence<helper<Ts, N>...>;

    template <typename V>
    static void
    set(V& v, const std::size_t i, const std::tuple<Ts...>& vals) noexcept
    {
        loom::zip_with_for_each([i](auto& v, const auto& s) noexcept
                                { v[i] = s; },
                                v,
                                vals);
    }

    template <typename V>
    [[nodiscard]] static auto
    get(const V& v, const std::size_t i) noexcept
    {
        return map_tuple(v, [i](const auto v) noexcept { return v[i]; });
    }

    struct cmp
    {
        bool
        operator()(const vector_type& lhs,
                   const vector_type& rhs) const noexcept
        {
            return fold_tuple(
                map_tuple(zip_tuples(lhs, rhs),
                          [](const auto& lr) noexcept
                          {
                              auto [l, r]  = lr;
                              using helper = typelist::at<
                                  typelist::index<decltype(l), vectors_t>,
                                  helpers>;
                              return helper::lrh::land(helper::vcmp::eq(l, r));
                          }),
                std::logical_and<>{});
        }
    };
};

template <Kernel K, std::size_t N>
class program_types
{
    using hi = helper<input_type<K>, N>;
    using ho = helper<output_type<K>, N>;

  public:
    using input_array  = hi::array_type;
    using output_array = ho::array_type;

    using input_simd  = hi::vector_type;
    using output_simd = ho::vector_type;

    template <typename... Ts>
    static void
    set_input(Ts&&... args) noexcept
    {
        hi::set(std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    [[nodiscard]] static auto
    get_input(Ts&&... args) noexcept
    {
        return hi::get(std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    static void
    set_output(Ts&&... args) noexcept
    {
        ho::set(std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    [[nodiscard]] static auto
    get_output(Ts&&... args) noexcept
    {
        return ho::get(std::forward<Ts>(args)...);
    }

    using cmp = ho::cmp;
};

template <template <typename> typename P,
          template <typename> typename GM,
          typename V>
struct StandardTestSuite
{
    static constexpr V val{};

    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        constexpr std::size_t N = simd::native_vector_extent_v<T>;

        using k_t     = P<T>;
        using types   = program_types<k_t, N>;
        using in_vec  = types::input_simd;
        using out_vec = types::output_simd;
        using cmp_t   = types::cmp;

        constexpr std::string_view nameview = k_t::nameview;
        constexpr auto             ti       = TypeInfo<T>::chars;
        constexpr GM<T>            gm{};
        constexpr cmp_t            cmp{};

        auto gs = gm(rs);

        auto gen = [&gs]() noexcept
        {
            in_vec ret;
            for(std::size_t i = 0; i < N; ++i) types::set_input(ret, i, gs());
            return ret;
        };

        auto ref = [](const auto& inp) noexcept
        {
            out_vec ret;
            for(std::size_t i = 0; i < N; ++i)
                types::set_output(ret,
                                  i,
                                  invopply(val, types::get_input(inp, i)));
            return ret;
        };

        simd::embedded_function<k_t> test;
        return track.check(
            NamedTest{tm.with_ref(gen, test, ref, cmp),
                      fmt::format("{}: {}[{}]", nameview, ti, N)});
    }
};

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
        using types   = program_types<k_t, N>;
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

        simd::embedded_function<k_t> test;
        return track.check(
            NamedFuzzyTest{tm.fuzzy_with_ref(gen, test, ref, cmp),
                           fmt::format("root finding: {}[{}]", ti, N),
                           0.1});
    }
};

int
main(int argc, char* argv[])
{
    simd::print_info();
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;

    test_set<mock_kernel<9>>(track, tm, rs);

    test_suite_types(typelist::join<all_ints, all_uints>{},
                     StandardTestSuite<increment, GM_Incr, ::fn::val_inc_fn>{},
                     track,
                     tm,
                     rs);

    test_suite_types(fac_ints{},
                     StandardTestSuite<fac_test, GM_FAC, ::fn::val_fac_fn>{},
                     track,
                     tm,
                     rs);

    {
        constexpr StandardTestSuite<fib_test, GM_FIB, ::fn::val_fib_fn> test;
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
        all_uints{},
        type_sequence<
            StandardTestSuite<jmp4_test, GM_U3, ::fn::val_jmp4_fn>,
            StandardTestSuite<bit_and_test, GM_Bit_Bin, std::bit_and<>>,
            StandardTestSuite<bit_or_test, GM_Bit_Bin, std::bit_or<>>,
            StandardTestSuite<bit_xor_test, GM_Bit_Bin, std::bit_xor<>>,
            StandardTestSuite<bit_not_test, GM_0U, std::bit_not<>>>{},
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

    test_suite_types(all_floats{}, RootFindingSuite{}, track, tm, rs);

    return track.ret_code();
}
