// -*- C++ -*-
// test_forbid.hpp
//

#pragma once

#include <vm/bve.hpp>
#include <vm/simd.hpp>

#include <common/test.hpp>
#include <common/tmp.hpp>

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

    static constexpr std::size_t num_blocks = _num_blocks;
    static constexpr index_t     halt_pos   = static_cast<index_t>(-1);
    static constexpr bool        is_straightline   = false;
};

template <unsigned _num_blocks, typename vars>
struct is_kernel<mock_kernel<_num_blocks, vars>> : std::true_type
{
};
} // namespace loom::vm
