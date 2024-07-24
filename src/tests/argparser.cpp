// -*- C++ -*-
// argparser.cpp
#include <utils/argparse.hpp>
#include <utils/functional.hpp>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include <fmt/ranges.h>

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

constexpr std::tuple options{
    option<'a', "argonaft">(to_unsigned,
                            "long doc for foo lore ipsum hule bly",
                            "FOO",
                            100),
    flag<'f', "flag">("doc for flag bla bla bla foo bar"),
    flag<'F', "Flag">("doc for Flag2 bla bla bla foo bar", true),
    option<"two-val", 2ul>(
        lucid::identity,
        "Tu es comme le vent qui fait chanter les violons et emporte au loin "
        "le parfum des roses. Caramels, bonbons et chocolats Par moments, je "
        "ne te comprends pas. Merci, pas pour moi Mais tu peux bien les offrir "
        "à une autre qui aime le vent et le parfum des roses Moi, les mots "
        "tendres enrobés de douceur se posent sur ma bouche mais jamais sur "
        "mon cœur Une parole encore. Paroles, paroles, paroles Ecoute-moi. "
        "Paroles, paroles, paroles Je t'en prie. Paroles, paroles, paroles Je "
        "te jure. Parole, parole, parole, parole, parole encore des paroles "
        "que tu sèmes au vent Voilà mon destin te parler.... te parler comme "
        "la première fois. Encore des mots toujours des mots les mêmes mots",
        {"2val1", "2val2"},
        {"1111", "2222"}),
    option_list<'m', "multi-val">(lucid::identity,
                                  "doc for unlimited values",
                                  "VAL")};

constexpr std::tuple positionals{
    positional<2ul>(lucid::identity,
                    "doc for 1st positional",
                    {"pos11", "pos12"}),
    positional(lucid::identity, "doc for 2nd positional", "pos2")};

int
main(int argc, char* argv[])
{
    int       ret = 0;
    ArgsRange args(argc, argv);
    fmt::print("args: {}\n", args);
    const auto results = parse(options, positionals, args);
    fmt::print("argonaft/a = {}\n", results.get_opt<"argonaft">());
    fmt::print("f/flag = {}\n", results.get_opt<'f'>());
    fmt::print("F/Flag = {}\n", results.get_opt<"Flag">());
    fmt::print("twoval = {}\n", results.get_opt<"two-val">());
    fmt::print("multival = {}\n", results.get_opt<'m'>());
    const auto pos1 = results.get_pos<0>();
    fmt::print("pos1 = {}, {}\n", pos1[0], pos1[1]);
    fmt::print("pos2 = {}\n", results.get_pos<1>());
    // ret += no_error_check();
    // ret += error_check();
    return ret;
}
