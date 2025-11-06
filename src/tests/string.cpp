// -*- C++ -*-
// char.cpp
#include <utils/string.hpp>
#include <utils/test.hpp>
#include <utils/typeinfo.hpp>

#include <fmt/format.h>

#include <iostream>
#include <limits>

using namespace loom;

#define FAIL_s2f(VAL) static_assert(!s2f<float>(VAL).has_value())
#define TEST_s2f(T, VAL) \
    static_assert(s2f<T>(#VAL).value() == static_cast<T>(VAL))

#define FAIL_read_number(VAL) static_assert(!s2f<float>(VAL).has_value())
#define TEST_read_number(T, VAL) \
    static_assert(s2f<T>(#VAL).value() == static_cast<T>(VAL))

TEST_s2f(float, 0);
TEST_s2f(float, 1);
TEST_s2f(float, 1234.567);
TEST_s2f(float, 1e2);
TEST_s2f(float, 0.5e2);
TEST_s2f(float, 0.5e-2);
TEST_s2f(float, 1e-8);
TEST_s2f(double, 0);
TEST_s2f(double, -1234.567);
TEST_s2f(double, -1e2);
TEST_s2f(double, -0.5e2);
TEST_s2f(double, -0.5e-2);
TEST_s2f(double, -2430.5e4);

FAIL_s2f("e334g");
FAIL_s2f("-");
FAIL_s2f("+");

TEST_read_number(float, 0);
TEST_read_number(float, 1);
TEST_read_number(float, 1234.567);
TEST_read_number(float, 1e2);
TEST_read_number(float, 0.5e2);
TEST_read_number(float, 0.5e-2);
TEST_read_number(float, 1e-8);
TEST_read_number(double, 0);
TEST_read_number(double, -1234.567);
TEST_read_number(double, -1e2);
TEST_read_number(double, -0.5e2);
TEST_read_number(double, -0.5e-2);
TEST_read_number(double, -2430.5e4);

FAIL_read_number("e334g");
FAIL_read_number("-");
FAIL_read_number("+");

static_assert(std::is_same_v<char_sequence<'t', 'e', 's', 't'>, sl2cs<"test">>);

using namespace loom;
using namespace loom::test;
using namespace fmt::literals;

enum BASE
{
    BIN = 0,
    DEC,
    HEX,
    OCT
};

template <std::integral Int>
std::string
format_int(Int num, BASE ts, unsigned width) noexcept
{
    // libfmt-9 doesn't support dynamic type specifier
    switch(ts)
    {
    case BIN: return fmt::format("{:0{}b}", num, width);
    case OCT: return fmt::format("{:0{}o}", num, width);
    case DEC: return fmt::format("{:0{}d}", num, width);
    case HEX: return fmt::format("{:0{}x}", num, width);
    }
}

template <std::integral Int>
std::string
format_int_prefix(Int num, BASE ts, unsigned width) noexcept
{
    // libfmt-9 doesn't support dynamic type specifier
    switch(ts)
    {
    case BIN: return fmt::format("{:#0{}b}", num, width);
    case OCT: return fmt::format("{:#0{}o}", num, width);
    case DEC: return fmt::format("{:#0{}d}", num, width);
    case HEX: return fmt::format("{:#0{}x}", num, width);
    }
}

unsigned
base_to_int(BASE base)
{
    switch(base)
    {
    case BIN: return 2;
    case DEC: return 10;
    case HEX: return 16;
    case OCT: return 8;
    }
};

template <std::integral Int>
bool
test_integral(Track& track, const ParmTestMaker& tm, RandomState& rs) noexcept
{
    auto gi = rs.make_gen<Int>(std::numeric_limits<Int>::lowest(),
                               std::numeric_limits<Int>::max());
    auto gb = [i = rs.make_gen<unsigned>(0, 3)]() mutable noexcept
    { return static_cast<BASE>(i()); };
    auto gb2 = [i = rs.make_gen<unsigned>(0, 2)]() mutable noexcept
    { return static_cast<BASE>(i()); }; // octal has no prefix, ignore it
    auto gw = rs.make_gen<unsigned>(0, 32);

    auto cmp = [](std::optional<Int> t, Int v) noexcept
    { return t && t.value() == v; };

    auto gh1 = [&]() noexcept
    {
        Int  num  = gi();
        BASE base = gb();
        return std::pair{
            std::pair{format_int(num, base, gw()), base_to_int(base)},
            num
        };
    };

    auto gh2 = [&]() noexcept
    {
        Int num = gi();
        return std::pair{format_int_prefix(num, gb2(), gw()), num};
    };

    auto test_s2i = NamedTest{tm.with_hint(gh1, s2i<Int>, cmp),
                              fmt::format("s2i {}", TypeInfo<Int>::chars)};
    auto test_s2i_bp =
        NamedTest{tm.with_hint(gh2, s2i_base_prefix<Int>, cmp),
                  fmt::format("s2i_base_prefix {}", TypeInfo<Int>::chars)};

    return track.dep_check(test_s2i, test_s2i_bp);
}

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns(argc, argv)};
    RandomState   rs;

    test_integral<int>(track, tm, rs);
    test_integral<long>(track, tm, rs);
    test_integral<unsigned>(track, tm, rs);
    test_integral<unsigned long>(track, tm, rs);
    test_integral<short>(track, tm, rs);
    test_integral<unsigned short>(track, tm, rs);

    return track.ret_code();
}
