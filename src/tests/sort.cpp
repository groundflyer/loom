// -*- C++ -*-
// sort.cpp
#include "vm_test_suite_utils.hpp"

#include <utils/test.hpp>

#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <algorithm>
#include <vector>

using namespace loom;
using namespace vm;
using namespace test;

template <std::unsigned_integral T>
constexpr T halt = static_cast<T>(-1);

constexpr unsigned    num_blocks   = 20;
constexpr std::size_t min_vec_size = 100;
constexpr std::size_t max_vec_size = 1000;

template <typename _Sorter>
struct sorter_name;

template <typename... Ts>
struct sorter_name<bve::CountingSorter<Ts...>>
{
    static constexpr char chars[] = "bve counting sort";
};

template <typename... Ts>
struct sorter_name<simd::CountingSorter<Ts...>>
{
    static constexpr char chars[] = "simd counting sort";
};

template <std::unsigned_integral Index>
auto
make_bp_gen(RandomState& rs, const double p = 0.9) noexcept
{
    auto bp_gen   = rs.make_gen<Index>(static_cast<Index>(0), num_blocks - 1);
    auto halt_gen = [bp_gen, gb = rs.make_gen<bool>(p)]() mutable noexcept
    { return gb() ? bp_gen() : halt<Index>; };
    return std::pair{bp_gen, halt_gen};
}

template <template <typename, typename> typename _Sorter, typename V = float>
struct CountingSortTestSuite
{
    using val_var   = Variable<V, CS(v)>;
    using K         = mock_kernel<num_blocks, type_sequence<val_var>>;
    using reg_names = K::reg_names;
    using reg_types = K::reg_types;
    using bp        = K::bp;
    using index_t   = K::index_t;

    static constexpr index_t      halt_pos   = K::halt_pos;
    static constexpr std::size_t  num_blocks = K::num_blocks;
    static constexpr std::hash<V> hash_v{};

    using memory = bve::Memory<reg_types,
                               reg_names,
                               bp,
                               std::vector,
                               CacheLineAlignedAllocator>;
    using sorter = _Sorter<K, memory>;

    static std::size_t
    chash(memory& m) noexcept
    {
        return ranges::accumulate(
            std::views::transform(m,
                                  [](auto r) noexcept
                                  {
                                      return hash_v(r.template get<val_var>()) +
                                             static_cast<std::size_t>(r.bp());
                                  }),
            0ul);
    }

    void
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) noexcept
    {
        auto [_, bpgen] = make_bp_gen<index_t>(rs);
        auto sgen =
            [ugen = rs.make_gen<unsigned>(100u, 10000u)]() mutable noexcept
        { return align_top(ugen(), 32u); };
        auto valgen =
            rs.make_gen<V>(static_cast<V>(-100000), static_cast<V>(100000));

        auto g = [&]() noexcept
        {
            const unsigned s = sgen();
            sorter         ret(s);
            for(unsigned i = 0; i < s; ++i)
            {
                ret.mem().bp()[i]                    = bpgen();
                ret.mem().template get<val_var>()[i] = valgen();
            }
            return ret;
        };
        auto t = [](sorter s) noexcept
        {
            s.sort();
            return s;
        };
        auto v = [](sorter tr, sorter gr) noexcept
        {
            return std::ranges::is_sorted(tr.mem().bp()) &&
                   chash(tr.mem()) == chash(gr.mem());
        };

        track.check(NamedTest(tm.test(g, t, v), sorter_name<sorter>::chars));
    }
};

int
main(int argc, char* argv[])
{
    Track         track;
    RandomState   rs;
    ParmTestMaker tm{parse_numruns(argc, argv)};

    using vec_type = std::vector<unsigned>;

    auto [bp_gen, halt_gen] = make_bp_gen<unsigned>(rs);
    auto vec_gen            = [&halt_gen,
                    gn = rs.make_gen<unsigned>(min_vec_size,
                                               max_vec_size)]() mutable noexcept
    {
        vec_type          ret;
        const std::size_t n = gn();
        ret.reserve(n);
        std::ranges::generate_n(std::back_inserter(ret), n, halt_gen);
        return ret;
    };

    track.dep_check(
        std::tuple{NamedTest(tm.invariant(
                                 vec_gen,
                                 [&](vec_type&& vec) noexcept
                                 {
                                     const auto p =
                                         [m = bp_gen()](unsigned val) noexcept
                                     { return val < m; };
                                     bve::partition(vec.begin(), vec.end(), p);
                                     return std::pair{std::move(vec), p};
                                 },
                                 [](const auto& vec_p) noexcept
                                 {
                                     const auto& [vec, p] = vec_p;
                                     return std::ranges::is_partitioned(vec, p);
                                 }),
                             "partition"),
                   NamedTest(tm.invariant(
                                 vec_gen,
                                 [](vec_type&& vec) noexcept
                                 {
                                     bve::insertion_sort(vec.begin(),
                                                         vec.end());
                                     return vec;
                                 },
                                 std::ranges::is_sorted),
                             "insertion sort")},
        NamedTest(tm.invariant(
                      vec_gen,
                      [](vec_type&& vec) noexcept
                      {
                          bve::quicksort(vec.begin(), vec.end());
                          return vec;
                      },
                      std::ranges::is_sorted),
                  "quicksort"),
        NamedTest(tm.invariant(
                      vec_gen,
                      [](vec_type&& vec) noexcept
                      {
                          bve::introsort(vec.begin(), vec.end());
                          return vec;
                      },
                      std::ranges::is_sorted),
                  "introsort"));

    CountingSortTestSuite<bve::CountingSorter>{}(track, tm, rs);
    CountingSortTestSuite<simd::CountingSorter>{}(track, tm, rs);

    return track.ret_code();
}
