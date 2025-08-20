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

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;

    test_set<mock_program<unsigned, 9>>(track, tm, rs);

    return 0;
}
