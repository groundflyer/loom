// -*- C++ -*-
// vm_embed.cpp
#include "programs.hpp"
#include "programs_generators.hpp"
#include "root_finding.hpp"

#include <utils/test.hpp>
#include <utils/typeinfo.hpp>
#include <vm/simd.hpp>

#include <array>
#include <set>

#include <fmt/core.h>

using namespace lucid;
using namespace lucid::test;

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

template <std::unsigned_integral Index, Index _num_cells>
struct mock_program
{
    using index_t = Index;

    static constexpr index_t num_cells = _num_cells;
    static constexpr index_t halt_pos  = static_cast<index_t>(-1);
};

template <typename P>
void
test_set(Track& track, ParmTestMaker tm, RandomState& rs) noexcept
{
    using index_t = P::index_t;

    constexpr index_t     num_cells = P::num_cells;
    constexpr std::size_t N         = simd::native_vector_extent_v<index_t>;
    constexpr auto        ti        = TypeInfo<index_t>::chars;

    using arr_t = std::array<index_t, N>;
    using vec_t = simd::native_vector<index_t>;
    using set_t = std::set<index_t>;

    auto gen = [g = rs.make_gen<index_t>(0, num_cells)]() mutable noexcept
    {
        arr_t ret;
        for(std::size_t i = 0; i < N; ++i) ret[i] = fit_cell_index<P>(g());
        return ret;
    };

    auto test = [](const arr_t& a) noexcept
    {
        const vec_t v = a2v(a);
        return simd::cell_set<P, N>{v};
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
                          fmt::format("cell set [{}]", ti)));
}

template <typename T, std::size_t N>
struct helper
{
    using array_type  = std::array<T, N>;
    using vector_type = simd::packed_vector<T, N>;

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
            return __builtin_reduce_and(lhs == rhs);
        }
    };
};

template <typename... Ts, std::size_t N>
struct helper<std::tuple<Ts...>, N>
{
    using array_type  = std::tuple<typename helper<Ts, N>::array_type...>;
    using vector_type = std::tuple<typename helper<Ts, N>::vector_type...>;

    template <typename V>
    static void
    set(V& v, const std::size_t i, const std::tuple<Ts...>& vals) noexcept
    {
        lucid::zip_with_for_each([i](auto& v, const auto& s) noexcept
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
            return fold_tuple(map_tuple(zip_tuples(lhs, rhs),
                                        [](const auto& lr) noexcept
                                        {
                                            const auto [l, r] = lr;
                                            return __builtin_reduce_and(l == r);
                                        }),
                              std::logical_and<>{});
        }
    };
};

template <typename P, std::size_t N>
class program_types
{
    using hi = helper<input_type<P>, N>;
    using ho = helper<output_type<P>, N>;

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
          typename V,
          StringLiteral name>
struct StandardTestSuite
{
    static constexpr V val{};

    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) const noexcept
    {
        constexpr std::size_t N = simd::native_vector_extent_v<T>;

        using p_t     = P<T>;
        using types   = program_types<p_t, N>;
        using in_vec  = types::input_simd;
        using out_vec = types::output_simd;
        using cmp_t   = types::cmp;

        constexpr auto  ti = TypeInfo<T>::chars;
        constexpr GM<T> gm{};
        constexpr cmp_t cmp{};

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
                                  maybe_apply(val, types::get_input(inp, i)));
            return ret;
        };

        simd::embedded_function<p_t> test;
        return track.check(
            NamedTest{tm.with_ref(gen, test, ref, cmp),
                      fmt::format("{}: {}[{}]", name.view(), ti, N)});
    }
};

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;

    test_set<mock_program<unsigned, 9>>(track, tm, rs);

    StandardTestSuite<increment, GM_Incr, ::fn::val_inc_fn, "increment">
                      test_inc{};
    test_inc.template operator()<std::uint64_t>(track, tm, rs);

    return track.ret_code();
}
