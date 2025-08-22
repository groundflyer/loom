// -*- C++ -*-
// test_utils.hpp
//

#pragma once

#include <utils/test.hpp>
#include <utils/tmp.hpp>

namespace lucid
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
using fac_ints =
    type_sequence<std::int32_t, std::int64_t, std::uint32_t, std::uint64_t>;
using all_floats = type_sequence<float, double>;
} // namespace lucid
