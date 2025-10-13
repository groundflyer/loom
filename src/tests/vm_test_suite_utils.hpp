// -*- C++ -*-
// test_utils.hpp
//

#pragma once

#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <utils/test.hpp>
#include <utils/tmp.hpp>

namespace lucid::vm
{
template <typelist::List Types, typename Suite>
void
test_suite_types(Types               types,
                 Suite               suite,
                 test::Track&        track,
                 test::ParmTestMaker tm,
                 test::RandomState&  rs) noexcept
{
    typelist::for_each(types, suite, track, tm, rs);
}

template <typelist::List Types, typelist::List Suites>
void
test_suites_types(Types               types,
                  Suites              suites,
                  test::Track&        track,
                  test::ParmTestMaker tm,
                  test::RandomState&  rs) noexcept
{
    typelist::for_each(
        suites,
        []<typename Suite>(Types               types,
                           test::Track&        track,
                           test::ParmTestMaker tm,
                           test::RandomState&  rs) noexcept
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
struct stitch_name<simd::N_ORDER>
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
} // namespace lucid::vm
