// -*- C++ -*-
// test_forbid.hpp
//

#pragma once

#include <vm/bse.hpp>
#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <common/test.hpp>
#include <common/tmp.hpp>
#include <common/typeinfo.hpp>

namespace loom::vm
{
template <typelist::List Types, typename Suite>
void
test_suite_types(Types               types,
                 Suite               suite,
                 test::Track&        track,
                 test::ParmTestMaker tm,
                 RandomState&        rs) noexcept
{
    typelist::for_each(types, suite, track, tm, rs);
}

template <typelist::List Types, typelist::List Suites>
void
test_suites_types(Types               types,
                  Suites              suites,
                  test::Track&        track,
                  test::ParmTestMaker tm,
                  RandomState&        rs) noexcept
{
    typelist::for_each(
        suites,
        []<typename Suite>(Types               types,
                           test::Track&        track,
                           test::ParmTestMaker tm,
                           RandomState&        rs) noexcept
        { test_suite_types(types, Suite{}, track, tm, rs); },
        types,
        track,
        tm,
        rs);
}

using all_ints =
    type_sequence<std::int8_t, std::int16_t, std::int32_t, std::int64_t>;
using all_uints =
    type_sequence<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>;
using fac_ints   = type_sequence<std::uint32_t, std::uint64_t>;
using all_floats = type_sequence<float, double>;
using fit4_uints =
    std::conditional_t<(simd::simd_width <= 16),
                       type_sequence<std::uint8_t, std::uint16_t>,
                       all_uints>;

template <auto>
struct sort_name;

template <>
struct sort_name<bve::INTROSORT>
{
    static constexpr char chars[] = "introsort";
};

template <>
struct sort_name<bve::COUNTING_SORT>
{
    static constexpr char chars[] = "counting sort";
};

template <>
struct sort_name<simd::bve_INTROSORT> : sort_name<bve::INTROSORT>
{
};

template <>
struct sort_name<simd::bve_COUNTING_SORT> : sort_name<bve::COUNTING_SORT>
{
};

template <>
struct sort_name<simd::COUNTING_SORT>
{
    static constexpr char chars[] = "vectorized counting sort";
};

template <simd::STITCH>
struct stitch_name;

template <>
struct stitch_name<simd::WAVEFRONT>
{
    static constexpr char chars[] = "N-order";
};

template <>
struct stitch_name<simd::MINIKERNEL>
{
    static constexpr char chars[] = "minikernel";
};

template <>
struct stitch_name<simd::MASKED>
{
    static constexpr char chars[] = "masked minikernel";
};

template <std::size_t _num_blocks,
          typename vars = type_sequence<Variable<float, CS(v)>>>
struct mock_kernel
{
    using index_t     = uint_of_size_t<sizeof(vartype<typelist::head<vars>>)>;
    using pc          = Variable<index_t, _pc_name>;
    using variables   = typelist::cons<pc, vars>;
    using variables1  = variables;
    using output_vars = variables;
    using reg_names   = typelist::map<varname, variables>;
    using reg_types   = typelist::map<vartype, variables>;

    static constexpr std::size_t num_blocks      = _num_blocks;
    static constexpr index_t     halt_pos        = static_cast<index_t>(-1);
    static constexpr bool        is_straightline = false;
};

template <unsigned _num_blocks, typename vars>
struct is_kernel<mock_kernel<_num_blocks, vars>> : std::true_type
{
};

template <typename T, std::size_t N>
simd::packed_vector<T, N>
a2v(const std::array<T, N>& a) noexcept
{
    simd::packed_vector<T, N> ret;
    for(std::size_t i = 0; i < N; ++i) ret[i] = a[i];
    return ret;
}

template <typename T, std::size_t N>
struct simd_test_helper
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
struct simd_test_helper<std::tuple<Ts...>, N>
{
    using array_type =
        std::tuple<typename simd_test_helper<Ts, N>::array_type...>;
    using vectors_t =
        type_sequence<typename simd_test_helper<Ts, N>::vector_type...>;
    using vector_type = typelist::to<vectors_t, std::tuple>;
    using helpers     = type_sequence<simd_test_helper<Ts, N>...>;

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
class simd_program_types
{
    using hi = simd_test_helper<input_type<K>, N>;
    using ho = simd_test_helper<output_type<K>, N>;

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

template <template <typename> typename K,
          template <typename> typename GM,
          typename V>
struct RefSIMDCompactSuite
{
    static constexpr V val{};

    template <typename T>
    bool
    operator()(test::Track&        track,
               test::ParmTestMaker tm,
               RandomState&        rs) const noexcept
    {
        constexpr std::size_t N = simd::native_vector_extent_v<T>;

        using k_t     = K<T>;
        using types   = simd_program_types<k_t, N>;
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

        simd::compact_function<k_t> test;
        return track.check(
            test::NamedTest{tm.with_ref(gen, test, ref, cmp),
                            fmt::format("{}: {}[{}]", nameview, ti, N)});
    }
};
} // namespace loom::vm
