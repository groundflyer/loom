// -*- C++ -*-
// sort.cpp
#include "vm_test_suite_utils.hpp"

#include <common/test.hpp>
#include <common/typeinfo.hpp>

#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <algorithm>
#include <vector>

using namespace loom;
using namespace vm;
using namespace test;

template <std::unsigned_integral T>
constexpr T halt = static_cast<T>(-1);

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

template <std::unsigned_integral Index, Index num_blocks>
auto
make_pc_gen(RandomState& rs, const double p = 0.9) noexcept
{
    auto pc_gen   = rs.make_gen<Index>(static_cast<Index>(0), num_blocks - 1);
    auto halt_gen = [pc_gen, gb = rs.make_gen<bool>(p)]() mutable noexcept
    { return gb() ? pc_gen() : halt<Index>; };
    return std::pair{pc_gen, halt_gen};
}

template <template <typename, typename> typename _Sorter, unsigned num_blocks>
struct CountingSortTestSuite
{
    template <typename T>
    bool
    operator()(Track& track, ParmTestMaker tm, RandomState& rs) noexcept
    {
        using val_var   = Variable<T, CS(v)>;
        using K         = mock_kernel<num_blocks, type_sequence<val_var>>;
        using reg_names = K::reg_names;
        using reg_types = K::reg_types;
        using pc        = K::pc;
        using index_t   = K::index_t;

        static constexpr std::hash<T> hash_v{};
        static constexpr auto         ti = TypeInfo<T>::chars;

        using memory = bve::Memory<reg_types,
                                   reg_names,
                                   pc,
                                   std::vector,
                                   CacheLineAlignedAllocator>;
        using sorter = _Sorter<K, memory>;

        auto [_, pcgen] = make_pc_gen<index_t, num_blocks>(rs);
        auto sgen =
            [ugen = rs.make_gen<unsigned>(100u, 10000u)]() mutable noexcept
        { return align_up(ugen(), 32u); };
        auto valgen =
            rs.make_gen<T>(static_cast<T>(-100000), static_cast<T>(100000));
        auto chash = [](const memory& m) noexcept
        {
            return ranges::accumulate(
                std::views::transform(
                    m,
                    [](auto r) noexcept
                    {
                        return hash_v(r.template get<val_var>()) +
                               static_cast<std::size_t>(r.pc());
                    }),
                0ul);
        };

        auto g = [&]() noexcept
        {
            const unsigned s = sgen();
            sorter         ret(s);
            for(unsigned i = 0; i < s; ++i)
            {
                ret.mem().pc()[i]                    = pcgen();
                ret.mem().template get<val_var>()[i] = valgen();
            }
            return ret;
        };
        auto t = [](sorter s) noexcept
        {
            s.sort();
            return s;
        };
        auto v = [chash](sorter tr, sorter gr) noexcept
        {
            return std::ranges::is_sorted(tr.mem().pc()) &&
                   chash(tr.mem()) == chash(gr.mem());
        };

        return track.check(
            NamedTest(tm.test(g, t, v),
                      fmt::format("{}: {}", sorter_name<sorter>::chars, ti)));
    }
};

int
main(int argc, char* argv[])
{
    fmt::print("Using x86-64 vector extensions: {}\n", simd::info::value);
    Track         track;
    RandomState   rs;
    ParmTestMaker tm{parse_numruns(argc, argv)};

    using vec_type = std::vector<unsigned>;

    auto [pc_gen, halt_gen] = make_pc_gen<unsigned, 20>(rs);
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
                                         [m = pc_gen()](unsigned val) noexcept
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

    test_suites_types(
        all_floats{},
        type_sequence<CountingSortTestSuite<bve::CountingSorter, 10>,
                      CountingSortTestSuite<simd::CountingSorter, 10>>{},
        track,
        tm,
        rs);

    return track.ret_code();
}
