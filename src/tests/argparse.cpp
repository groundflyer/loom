// -*- C++ -*-
/// @file argparse.cpp
/// @brief argparse module tests.

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
using namespace std::literals::string_view_literals;

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

    explicit ArgvMock(std::initializer_list<std::string> words) :
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
    argv() noexcept
    {
        return m_pointers.data();
    }

    int
    argc() noexcept
    {
        return m_pointers.size();
    }

    ArgsRange
    operator()()
    {
        return {argc(), argv()};
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
    bool few_args       = false;
    bool unexpected_val = false;
    bool keychar_ex     = false;
    bool keyword_ex     = false;

    void
    operator()(FewArgumentsException) noexcept
    {
        few_args = true;
    }

    void
    operator()(UnexpectedValue) noexcept
    {
        unexpected_val = true;
    }

    void
    operator()(KeyException<char>)
    {
        keychar_ex = true;
    }

    void
    operator()(KeyException<std::string_view>)
    {
        keyword_ex = true;
    }
};

template <char Key, typename Keyword, typename Reader, std::size_t n>
std::string
make_keyword(Option<Key, Keyword, Reader, n>) noexcept
{
    static_assert(!std::is_same_v<Keyword, missing_keyword>);
    return "--"s + std::string(cs2sv<Keyword>);
}

template <char Key, typename Keyword, typename Reader, std::size_t n>
std::string
make_keychar(Option<Key, Keyword, Reader, n>) noexcept
{
    static_assert(Key != missing_keychar);
    return "-"s + Key;
}

template <char Key,
          typename Keyword,
          typename Reader,
          std::size_t n,
          typename C>
std::string
gen_key(const Option<Key, Keyword, Reader, n>& o, C& c) noexcept
{
    if constexpr(Key == missing_keychar)
        return make_keyword(o);
    else if constexpr(std::is_same_v<Keyword, missing_keyword>)
        return make_keychar(o);
    else
        return c() ? make_keychar(o) : make_keyword(o);
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

template <typename Option, typename W>
std::pair<ArgvMock, std::string>
gen_correct_kw(const Option& o, W& word_gen) noexcept
{
    const std::string correct = word_gen();
    const std::string test    = make_keyword(o) + "=" + correct;
    return {ArgvMock({test}), correct};
}

#define PARSE_INIT                     \
    State      opt_state{options};     \
    State      pos_state{positionals}; \
    ErrorState error;                  \
    auto       iter = args.begin()

template <typename... Options, typename... Positionals, typename Parser>
auto
test_parser_pr(const std::tuple<Options...>&     options,
               const std::tuple<Positionals...>& positionals,
               Parser                            p,
               ArgsRange                         args) noexcept
{
    PARSE_INIT;
    p(iter, opt_state, pos_state, error);
    if constexpr(sizeof...(Options) > 0 && sizeof...(Positionals) > 0)
        return ParseResults{opt_state.bindings, pos_state.bindings, options};
    else if constexpr(sizeof...(Options) > 0)
        return ParseResults{opt_state.bindings, options};
    else
        return ParseResults{pos_state.bindings};
};

template <typename Options, typename Positionals, typename Parser>
bool
test_parser_ignore(const Options&     options,
                   const Positionals& positionals,
                   Parser             p,
                   ArgsRange          args) noexcept
{
    PARSE_INIT;
    return p(iter, opt_state, pos_state, error);
};

template <typename Options, typename Positionals, typename Parser>
ErrorState
test_parser_error(const Options&     options,
                  const Positionals& positionals,
                  Parser             p,
                  ArgsRange          args) noexcept
{
    PARSE_INIT;
    p(iter, opt_state, pos_state, error);
    return error;
};

constexpr inline IntReader    int_reader{};
constexpr inline ErrorCatcher error_catcher{};

constexpr auto o1 =
    option<'o', "option">("long doc for option", "O", "default");
constexpr auto o1kc =
    option<'c'>("long doc for option with keychar only", "C", "default");
constexpr auto o1kw =
    option<"keyword">("long doc for option with keyword only", "K", "default");
constexpr auto of1  = flag<'f', "flag">("long doc for flag");
constexpr auto of2  = flag<'F', "Flag">("long doc for inverse Flag", true);
constexpr auto o2   = option<"two-val", 2>(
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
      "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
      "aliquip ex ea commodo consequat. Duis aute irure dolor in "
      "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
      "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
      "culpa qui officia deserunt mollit anim id est laborum.",
    {"2val1", "2val2"},
    {"1111", "2222"});
constexpr auto om =
    option_list<'m', "multi-val">("doc for unlimited values", "VAL");

constexpr std::tuple options{o1, o1kc, o1kw, of1, of2, o2, om};

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
    auto wgen = [&]()
    {
        std::string ret;
        unsigned    l = lgen();
        ret.reserve(l);
        std::generate_n(std::back_inserter(ret), l, azgen);
        return ret;
    };
    auto sgen = [&]()
    {
        std::vector<std::string> ret;
        unsigned                 n = lgen();
        ret.reserve(n);
        std::generate_n(std::back_inserter(ret), n, wgen);
        return ret;
    };

    // auto choice_gen    = rs.make_gen<bool>(0.5);

    auto o1_pr_gen  = [&]() { return gen_correct_kw(o1, wgen); };
    auto o1_pr_test = [&](ArgvMock mock) noexcept
    {
        return std::string(test_parser_pr(std::tuple{o1},
                                          std::tuple{},
                                          keyword_value_parser,
                                          mock())
                               .template get_opt<"option">());
    };

    auto o1_ignore_gen  = [&]() { return ArgvMock(sgen()); };
    auto o1_ignore_test = [&](ArgvMock mock)
    {
        return test_parser_ignore(std::tuple{o1},
                                  std::tuple{},
                                  keyword_value_parser,
                                  mock());
    };

    auto o1_wrong_keyword_gen = [&]()
    // there is negligible chance that wgen() produces "option"
    { return ArgvMock({"--"s + wgen() + '=' + wgen()}); };
    auto o1_wrong_keyword_test = [&](ArgvMock mock)
    {
        return test_parser_error(std::tuple{o1},
                                 std::tuple{},
                                 keyword_value_parser,
                                 mock());
    };
    auto check_keyword_ex = [&](ErrorState e)
    {
        ErrorCatcher ec;
        if(e.has_value())
        {
            std::visit(ec, e.value());
            return ec.keyword_ex;
        }
        else
            return false;
    };
    auto check_few_args_ex = [&](ErrorState e)
    {
        ErrorCatcher ec;
        if(e.has_value())
        {
            std::visit(ec, e.value());
            return ec.few_args;
        }
        else
            return false;
    };

    auto o2_kw_few_gen = [&]()
    { return ArgvMock({make_keyword(o2) + '=' + wgen()}); };
    auto o2_kw_few_test = [&](ArgvMock mock)
    {
        return test_parser_error(std::tuple{o2},
                                 std::tuple{},
                                 keyword_value_parser,
                                 mock());
    };

    /// @test @p --keyword=value parsing results test.
    /// Generate correct keyword and correct value.
    /// Check if binding holds the value.
    auto to1_pr = tm.test_with_hint(o1_pr_gen, o1_pr_test);

    /// @test @p --keyword=value ignore test.
    /// Generate random words that don't match @p --keyword=value pattern.
    /// Check if parser ignores them.
    auto to1_ignore =
        tm.invariant(o1_ignore_gen, o1_ignore_test, std::logical_not<>{});

    /// @test @p --keyword=value wrong key test.
    /// Generate @p --keyword=value string where @p keyword is invalid.
    /// Check if lucid::argparse::KeyException is stored in
    /// lucid::argparse::ErrorState
    auto to1_wrong_keyword = tm.invariant(o1_wrong_keyword_gen,
                                          o1_wrong_keyword_test,
                                          check_keyword_ex);

    /// @test @p --keyword=value few arguments exception test.
    /// Generate @p --keyword=value string for option that requires 2 operands.
    /// Check if lucid::argparse::FewArgumentsException is stored.
    auto to2_few_args_ex =
        tm.invariant(o2_kw_few_gen, o2_kw_few_test, check_few_args_ex);

    track.check(NamedTest(to1_pr, "Parsing result of --keyword=value parser"));
    track.check(
        NamedTest(to1_ignore, "Ignore random words by --keyword=value parser"));
    track.check(NamedTest(to1_wrong_keyword,
                          "Keyword exception in --keyword=value parser"));
    track.check(NamedTest(to2_few_args_ex,
                          "Few arguments exception in --keyword=value parser"));

    return track.ret_code();
}
