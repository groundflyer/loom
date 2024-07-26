// -*- C++ -*-
// argparser.cpp
#include <utils/argparse.hpp>
#include <utils/test.hpp>

#include <algorithm>
#include <initializer_list>
#include <ranges>
#include <string>
#include <vector>

#include <fmt/ranges.h>

using namespace lucid;
using namespace lucid::argparse;
using namespace lucid::test;
using namespace std::literals::string_literals;

class ArgvMock
{
    std::vector<std::string> m_words;
    std::vector<char*>       m_pointers;

    void
    update_argv()
    {
        m_pointers.reserve(m_words.size());
        std::transform(m_words.begin(),
                       m_words.end(),
                       std::back_inserter(m_pointers),
                       [](std::string& w) { return w.data(); });
    }

  public:
    explicit ArgvMock(std::initializer_list<std::string_view> words) :
        m_words(words.begin(), words.end())
    {
        update_argv();
    }

    explicit ArgvMock(std::vector<std::string>&& words) :
        m_words(std::move(words))
    {
        update_argv();
    }

    explicit ArgvMock(std::string_view words)
    {
        auto view = std::views::split(words, ' ');
        std::transform(view.begin(),
                       view.end(),
                       std::back_inserter(m_words),
                       [](auto rng)
                       { return std::string(rng.begin(), rng.end()); });
        m_words.shrink_to_fit();
        update_argv();
    }

    char**
    argv()
    {
        return m_pointers.data();
    }

    int
    argc()
    {
        return m_pointers.size();
    }
};

struct IntReader
{
    int
    operator()(const std::string_view word) const noexcept
    {
        return std::atoi(word.data());
    }
};

struct ErrorCatcher
{
    bool got = false;

    template <typename T>
    void
    operator()(T) noexcept
    {
        got = true;
    }
};

template <char Key,
          typename Keyword,
          typename Reader,
          std::size_t n,
          typename C>
std::string
gen_key(Option<Key, Keyword, Reader, n>, C& c) noexcept
{
    if constexpr(Key == missing_keychar)
        return "--"s + cs2sv<Keyword>;
    else if constexpr(std::is_same_v<Keyword, missing_keyword>)
        return "-"s + Key;
    else
        return c() ? "--"s + cs2sv<Keyword> : "-"s + Key;
}

template <char Key,
          typename Keyword,
          typename Reader,
          std::size_t n,
          typename G,
          typename L>
auto
gen_vals(Option<Key, Keyword, Reader, n>, G& word_gen, L& lgen) noexcept
{
    static_assert(!Option<Key, Keyword, Reader, n>::is_flag);
    if constexpr(n == 1)
        return word_gen();
    else if constexpr(n == npos)
    {
        std::vector<std::string> ret;
        const unsigned           l = lgen();
        ret.reserve(l);
        std::generate_n(std::back_inserter(ret), l, word_gen);
        return ret;
    }
    else
    {
        std::vector<std::string> ret;
        ret.reserve(n);
        std::generate_n(std::back_inserter(ret), n, word_gen);
        return ret;
    }
}

constexpr inline IntReader    int_reader{};
constexpr inline ErrorCatcher error_catcher{};

constexpr auto o_num = option<'o', "option">("long doc for option", "O", "100");
constexpr auto o_flag1 = flag<'f', "flag">("long doc for flag");
constexpr auto o_flag2 = flag<'F', "Flag">("long doc for inverse Flag", true);
constexpr auto o_two   = option<"two-val", 2>(
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
      "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
      "aliquip ex ea commodo consequat. Duis aute irure dolor in "
      "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
      "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
      "culpa qui officia deserunt mollit anim id est laborum.",
    {"2val1", "2val2"},
    {"1111", "2222"});
constexpr auto o_multi =
    option_list<'m', "multi-val">("doc for unlimited values", "VAL");

constexpr std::tuple options{o_num, o_flag1, o_flag2, o_two, o_multi};

constexpr std::tuple positionals{
    positional<2ul>("doc for 1st positional", {"pos11", "pos12"}),
    positional("doc for 2nd positional", "pos2")};

constexpr inline KeywordValueParser keyword_value_parser{};

int main(/*int argc, char* argv[]*/)
{
    Track         track;
    ParmTestMaker tm{1000};
    RandomState   rs;

    auto lgen  = rs.make_gen<unsigned>(1, 100);
    auto azgen = [d = rs.make_gen<int>(static_cast<int>('a'),
                                       static_cast<int>('z'))]() mutable
    { return static_cast<char>(d()); };
    auto word_gen = [&]()
    {
        std::string ret;
        unsigned    l = lgen();
        ret.reserve(l);
        std::generate_n(std::back_inserter(ret), l, azgen);
        return ret;
    };
    auto choice_gen = rs.make_gen<bool>(0.5);

    int       ret = 0;
    ArgvMock  mock({"name", "-h"});
    ArgsRange args(mock.argc(), mock.argv());
    fmt::print("args: {}\n", args);
    return ret;
}
