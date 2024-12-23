// -*- C++ -*-
// char.cpp
#include <utils/string.hpp>
#include <utils/test.hpp>

#include <limits>

using namespace lucid;

static_assert(std::is_same_v<char_sequence<'t', 'e', 's', 't'>, sl2cs<"test">>);

using namespace lucid;
using namespace lucid::test;

template <std::signed_integral Int>
std::string
gen_int_str(RandomState& rs) noexcept
{
    auto g = [c = rs.make_gen<bool>(0.5),
              i = rs.make_gen<Int>(
                  std::numeric_limits<Int>::lowest(),
                  std::numeric_limits<Int>::max())]() mutable noexcept
    { return i() * (c() ? static_cast<Int>(-1) : static_cast<Int>(1)); };
}

template <std::integral Int>
bool
test_integral(Track& track, const ParmTestMaker& tm, RandomState& rs) noexcept
{
}

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;

    auto choice = rs.make_gen<bool>(0.5);

    return 0;
}
