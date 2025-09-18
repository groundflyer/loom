// -*- C++ -*-
// sort.cpp
#include <utils/test.hpp>

#include <vm/bve.hpp>

#include <algorithm>
#include <vector>

using namespace lucid;
using namespace test;

int
main(int argc, char* argv[])
{
    Track         track;
    RandomState   rs;
    ParmTestMaker tm{parse_numruns(argc, argv)};

    using vec_type = std::vector<unsigned>;

    constexpr unsigned    num_blocks   = 20;
    constexpr unsigned    halt         = -1;
    constexpr std::size_t min_vec_size = 100;
    constexpr std::size_t max_vec_size = 1000;

    auto bp_gen  = rs.make_gen<unsigned>(0u, num_blocks);
    auto val_gen = [&bp_gen, gb = rs.make_gen<bool>(0.9)]() mutable noexcept
    { return gb() ? bp_gen() : halt; };
    auto vec_gen = [&val_gen,
                    gn = rs.make_gen<unsigned>(min_vec_size,
                                               max_vec_size)]() mutable noexcept
    {
        vec_type          ret;
        const std::size_t n = gn();
        ret.reserve(n);
        std::ranges::generate_n(std::back_inserter(ret), n, val_gen);
        return ret;
    };

    const bool pts = track.check(
        NamedTest(tm.invariant(
                      vec_gen,
                      [&](vec_type&& vec) noexcept
                      {
                          const auto p = [m = bp_gen()](unsigned val) noexcept
                          { return val < m; };
                          bve::swap_only_partition(vec.begin(), vec.end(), p);
                          return std::pair{std::move(vec), p};
                      },
                      [](const auto& vec_p) noexcept
                      {
                          const auto& [vec, p] = vec_p;
                          return std::ranges::is_partitioned(vec, p);
                      }),
                  "swap only partition"));

    const bool iss = track.check(
        NamedTest(tm.invariant(
                      vec_gen,
                      [](vec_type&& vec) noexcept
                      {
                          bve::swap_only_insertion_sort(vec.begin(), vec.end());
                          return vec;
                      },
                      std::ranges::is_sorted),
                  "swap only insertion sort"));

    const auto make_heap_test =
        NamedTest(tm.invariant(
                      vec_gen,
                      [](vec_type&& vec) noexcept
                      {
                          bve::swap_only_make_heap(vec.begin(), vec.end());
                          return vec;
                      },
                      std::ranges::is_heap),
                  "swap only make heap");

    const auto heap_sort_test =
        NamedTest(tm.invariant(
                      vec_gen,
                      [](vec_type&& vec) noexcept
                      {
                          bve::swap_only_heap_sort(vec.begin(), vec.end());
                          return vec;
                      },
                      std::ranges::is_sorted),
                  "swap only heap sort");

    const bool hts = track.dep_check(make_heap_test, heap_sort_test);

    const auto introsort_test =
        NamedTest(tm.invariant(
                      vec_gen,
                      [](vec_type&& vec) noexcept
                      {
                          bve::swap_only_introsort(vec.begin(), vec.end());
                          return vec;
                      },
                      std::ranges::is_sorted),
                  "swap only instrosort");

    track.dep_check(
        std::tuple{NamedTest(constant(pts), "swap only partition"),
                   NamedTest(constant(iss), "swap only insertion sort"),
                   NamedTest(constant(hts), "swap only heap sort")},
        introsort_test);

    return track.ret_code();
}
