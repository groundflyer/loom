// -*- C++ -*-
// argparser.cpp
#include <utils/argparse.hpp>
#include <utils/functional.hpp>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

using namespace std;
using namespace lucid;
using namespace lucid::argparse;

struct ArgvMock
{
    std::vector<std::string> words;
    std::vector<char*>       pointers;

    void
    update_argv()
    {
        pointers.clear();
        std::generate_n(std::back_inserter(pointers),
                        words.size(),
                        [iter = words.cbegin()]() mutable
                        { return const_cast<char*>((iter++)->data()); });
    }

    explicit ArgvMock(std::initializer_list<std::string_view> _words) :
        words(_words.begin(), _words.end())
    {
        update_argv();
    }

    explicit ArgvMock(const std::vector<std::string>& _words) : words(_words)
    {
        update_argv();
    }

    char**
    argv()
    {
        return pointers.data();
    }

    int
    argc()
    {
        return pointers.size();
    }
};

struct to_unsigned_f
{
    unsigned
    operator()(const std::string_view word) const noexcept
    {
        return std::atoi(word.data());
    }
};

constexpr to_unsigned_f to_unsigned{};

constexpr tuple options{
    option<'a', "argonaft">(to_unsigned,
                            100,
                            "long doc for foo lore ipsum hule bly",
                            "FOO"),
    flag<'f', "flag">(false, "doc for flag bla bla bla foo bar"),
    option<'d', "two-val", 2ul>(
        lucid::identity,
        {"1111", "2222"},
        "doc for two-val here we have an array fo string views of size two",
        {"2val1", "2val2"}),
    option_list<'m', "multi-val">(lucid::identity,
                                  "doc for unlimited values",
                                  "VAL")};

constexpr tuple positionals{
    positional<2ul>(lucid::identity,
                    "doc for 1st positional",
                    {"pos11", "pos12"}),
    positional(lucid::identity, "doc for 2nd positional", "pos2")};

/// @test Passing correct arguments to argument parser.
bool
no_error_check() noexcept
{
    ArgvMock  mock({"argtest",
                   "-a",
                   "foo value",
                   "-d",
                   "d1",
                   "d2",
                   "-m",
                   "mval1",
                   "mval2",
                   "mval3",
                   "-f",
                   "1posval1",
                   "1posval2",
                   "2posval"});
    ArgsRange args(mock.argc(), mock.argv());
    bool      ret = false;
    parse(options, positionals, args, [&ret](const auto&) { ret = true; });
    return ret;
}

/// @test Passing wrong arguments (no required positionals) to argument parser.
bool
error_check() noexcept
{
    ArgvMock mock({"argtest"});
    ArgsRange args(mock.argc(), mock.argv());
    bool ret = true;
    parse(options, positionals, args, [&ret](const auto&){ ret = false; });
    return ret;
}

int
main(int argc, char* argv[])
{
    int        ret = 0;
    ArgsRange  args(argc, argv);
    const auto results =
        parse(options, positionals, args, StandardErrorHandler{});
    fmt::print("a = {}\n", results.get_opt<'a'>());
    fmt::print("argonaft = {}\n", results.get_opt<"argonaft">());
    fmt::print("f = {}\n", results.get_opt<'f'>());
    fmt::print("flag = {}\n", results.get_opt<"flag">());
    const std::array<std::string_view, 2> pos1 = results.get_pos<0>();
    fmt::print("pos1 = {}, {}\n", pos1[0], pos1[1]);
    fmt::print("pos2 = {}\n", results.get_pos<1>());
    // ret += no_error_check();
    // ret += error_check();
    // show_help("lucid", options, positionals);
    return ret;
}
