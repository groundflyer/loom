// -*- C++ -*-
/// @file argparse.cpp
/// @brief argparse module tests.

#include <utils/argparse.hpp>
#include <utils/test.hpp>
#include <utils/tuple.hpp>

#include <algorithm>
#include <array>
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

consteval auto
make_symbols() noexcept
{
    std::array<char, 26> az;
    for(int i = 0; i < 26; ++i) az[i] = static_cast<char>('a' + i);
    std::array<char, 26> AZ;
    for(int i = 0; i < 26; ++i) AZ[i] = static_cast<char>('A' + i);
    std::array<char, 10> nums;
    for(int i = 0; i < 10; ++i) nums[i] = static_cast<char>('0' + i);
    std::array allowed_symbols{'`', '~', '!', '@', '#', '$', '%', '^', '&',
                               '*', '(', ')', '_', '[', ']', '{', '}', ';',
                               ':', '<', '>', ',', '.', '?', '/', '|'};
    return array_cat(az, AZ, nums, allowed_symbols);
};

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
gen_vals(Option<Key, Keyword, Reader, n>, G& wgen, L& lgen) noexcept
{
    static_assert(!Option<Key, Keyword, Reader, n>::is_flag);
    if constexpr(n == 1)
        return wgen();
    else if constexpr(n == npos)
    {
        std::vector<std::string> ret;
        const unsigned           l = lgen();
        ret.reserve(l);
        std::generate_n(std::back_inserter(ret), l, wgen);
        return ret;
    }
    else
    {
        std::vector<std::string> ret;
        ret.reserve(n);
        std::generate_n(std::back_inserter(ret), n, wgen);
        return ret;
    }
}

template <typename Option, typename W, typename C, typename L>
auto
gen_key_vals(const Option& o, W& wgen, C& cgen, L& lgen)
{
    std::string key  = gen_key(o, cgen);
    auto        vals = gen_vals(o, wgen, lgen);
    if constexpr(std::is_same_v<decltype(vals), std::string>)
        return std::pair{ArgvMock({key, vals}), vals};
    else
    {
        std::vector<std::string> ws{std::move(key)};
        ws.reserve(1 + vals.size());
        for(auto& v: vals) ws.push_back(v);
        return std::pair{ArgvMock(std::move(ws)), std::move(vals)};
    }
};

template <typename Option, typename W>
std::pair<ArgvMock, std::string>
gen_keyword_value(const Option& o, W& wgen) noexcept
{
    const std::string correct = wgen();
    const std::string kw      = make_keyword(o);
    return {ArgvMock({kw + '=' + correct}), correct};
}

template <typename Option, typename C>
std::pair<ArgvMock, bool>
gen_keyword_value_bool(const Option& o, C& choice) noexcept
{
    const bool        correct = choice();
    const std::string test =
        make_keyword(o) + "=" + (correct ? "true"s : "false"s);
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
constexpr inline KeyParser          key_parser{};
constexpr inline KeyCharSeqParser   keycharseq_parser{};

int main(/*int argc, char* argv[]*/)
{
    Track         track;
    ParmTestMaker tm{10000};
    RandomState   rs;

    constexpr auto symbols = make_symbols();
    auto           lgen    = rs.make_gen<unsigned>(1, 100);
    auto           symbolgen =
        [&symbols,
         d = rs.make_gen<std::size_t>(0, symbols.size() - 1)]() mutable
    { return symbols[d()]; };
    auto cgen = rs.make_gen<bool>(0.5);
    auto wgen = [&]()
    {
        std::string ret;
        unsigned    l = lgen();
        // randomly add '-' in front
        bool add_dash  = l > 2 && cgen();
        bool add_2dash = add_dash && cgen();
        ret.reserve(l + add_dash + add_2dash);
        if(add_dash) ret.push_back('-');
        if(add_2dash) ret.push_back('-');
        std::generate_n(std::back_inserter(ret), l, symbolgen);
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

#define MAKE_ERROR_CHECK(name)            \
    auto check_##name = [&](ErrorState e) \
    {                                     \
        ErrorCatcher ec;                  \
        if(e.has_value())                 \
        {                                 \
            std::visit(ec, e.value());    \
            return ec.name;               \
        }                                 \
        else                              \
            return false;                 \
    }

    MAKE_ERROR_CHECK(keyword_ex);
    MAKE_ERROR_CHECK(few_args);

    auto o1_kw_pr_gen = [&]() { return gen_keyword_value(o1, wgen); };
    auto o1_pr_test   = [&](ArgvMock& mock, auto p)
    {
        return std::string(
            test_parser_pr(std::tuple{o1}, std::tuple{}, p, mock())
                .template get_opt<"option">());
    };
    auto o1_kw_pr_test = [&](ArgvMock&& mock)
    { return o1_pr_test(mock, keyword_value_parser); };
    auto o1_k_pr_gen  = [&]() { return gen_key_vals(o1, wgen, cgen, lgen); };
    auto o1_k_pr_test = [&](ArgvMock&& mock)
    { return o1_pr_test(mock, key_parser); };
    auto o1_kcs_pr_gen = [&]()
    {
        std::string val = wgen();
        return std::pair{ArgvMock({make_keychar(o1) + val}), val};
    };
    auto o1_kcs_pr_test = [&](ArgvMock&& mock)
    { return o1_pr_test(mock, keycharseq_parser); };

    auto ignore_gen     = [&]() { return ArgvMock(sgen()); };
    auto o1_ignore_test = [&](ArgvMock&& mock)
    {
        return test_parser_ignore(std::tuple{o1},
                                  std::tuple{},
                                  keyword_value_parser,
                                  mock());
    };

    auto o1_wrong_keyword_gen = [&]()
    // there is negligible chance that wgen() produces "option"
    { return ArgvMock({"--"s + wgen() + '=' + wgen()}); };
    auto o1_wrong_keyword_test = [&](ArgvMock&& mock)
    {
        return test_parser_error(std::tuple{o1},
                                 std::tuple{},
                                 keyword_value_parser,
                                 mock());
    };

    auto o2_kw_few_gen = [&]()
    { return ArgvMock({make_keyword(o2) + '=' + wgen()}); };
    auto o2_kw_few_test = [&](ArgvMock&& mock)
    {
        return test_parser_error(std::tuple{o2},
                                 std::tuple{},
                                 keyword_value_parser,
                                 mock());
    };
    auto o2_k_few_gen = [&]() { return ArgvMock({gen_key(o2, cgen), wgen()}); };
    auto o2_k_few_test = [&](ArgvMock&& mock) {
        return test_parser_error(std::tuple{o2},
                                 std::tuple{},
                                 key_parser,
                                 mock());
    };

    auto of1_kw_pr_gen  = [&]() { return gen_keyword_value_bool(of1, cgen); };
    auto of1_kw_pr_test = [&](ArgvMock&& mock) noexcept
    {
        return test_parser_pr(std::tuple{of1},
                              std::tuple{},
                              keyword_value_parser,
                              mock())
            .template get_opt<"flag">();
    };
    auto of1_k_pr_gen  = [&]() { return ArgvMock({gen_key(of1, cgen)}); };
    auto of1_k_pr_test = [&](ArgvMock&& mock) noexcept
    {
        return test_parser_pr(std::tuple{of1}, std::tuple{}, key_parser, mock())
            .template get_opt<"flag">();
    };
    auto of2_k_pr_gen  = [&]() { return ArgvMock({gen_key(of2, cgen)}); };
    auto of2_k_pr_test = [&](ArgvMock&& mock) noexcept
    {
        return test_parser_pr(std::tuple{of2}, std::tuple{}, key_parser, mock())
            .template get_opt<'F'>();
    };

    auto o2_k_pr_gen  = [&]() { return gen_key_vals(o2, wgen, cgen, lgen); };
    auto o2_k_pr_test = [&](ArgvMock&& mock)
    {
        auto rng =
            test_parser_pr(std::tuple{o2}, std::tuple{}, key_parser, mock())
                .template get_opt<"two-val">();
        return std::vector<std::string>(std::ranges::begin(rng),
                                        std::ranges::end(rng));
    };

    auto om_k_pr_gen  = [&]() { return gen_key_vals(om, wgen, cgen, lgen); };
    auto om_k_pr_test = [&](ArgvMock&& mock)
    {
        auto rng =
            test_parser_pr(std::tuple{om}, std::tuple{}, key_parser, mock())
                .template get_opt<'m'>();
        return std::vector<std::string>(std::ranges::begin(rng),
                                        std::ranges::end(rng));
    };
    auto k_ignore_test = [&](ArgvMock&& mock)
    {
        return test_parser_ignore(std::tuple{o1, o1kc, o1kw, o2, of1, of2, om},
                                  std::tuple{},
                                  key_parser,
                                  mock());
    };

    auto of_kcs_pr_gen = [&]()
    {
        std::string r;
        unsigned    n1 = lgen();
        unsigned    n2 = lgen();
        r.reserve(n1 + n2 + 1);
        for(unsigned i = 0; i < n1; ++i) r.push_back('f');
        for(unsigned i = 0; i < n2; ++i) r.push_back('F');
        std::shuffle(r.begin(), r.end(), rs.g);
        r.insert(0, 1, '-');
        return std::pair{
            ArgvMock({std::move(r)}
            ),
            std::pair{n1 & 1, !(n2 & 1)}
        };
    };
    auto of_kcs_pr_test = [&](ArgvMock&& mock)
    {
        auto pr = test_parser_pr(std::tuple{of1, of2},
                                 std::tuple{},
                                 keycharseq_parser,
                                 mock());
        bool f1 = pr.get_opt<'f'>();
        bool f2 = pr.get_opt<'F'>();
        return std::pair{f1, f2};
    };
    auto cmp_pairs = [](auto p1, auto p2) noexcept
    { return p1.first == p2.first && p1.second == p2.second; };

    /// @test @p --keyword=value parsing results test.
    /// Generate correct keyword and correct value.
    /// Check if binding holds the value.
    auto to1_kw_pr = tm.test_with_hint(o1_kw_pr_gen, o1_kw_pr_test);

    /// @test <tt>-k val</tt> or <tt>\--keyword val</tt> parsing results test.
    /// Generate correct key and value.
    /// Check if binding holds the value.
    auto to1_k_pr = tm.test_with_hint(o1_k_pr_gen, o1_k_pr_test);

    /// @test @p --keyword=value ignore test.
    /// Generate random words that don't match @p --keyword=value pattern.
    /// Check if parser ignores them.
    auto to1_ignore =
        tm.invariant(ignore_gen, o1_ignore_test, std::logical_not<>{});

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
    auto to2_kw_few_args_ex =
        tm.invariant(o2_kw_few_gen, o2_kw_few_test, check_few_args);

    /// @test <tt>--two-val val1 val2</tt> parsing result test.
    auto to2_k_pr = tm.test_with_hint(o2_k_pr_gen, o2_k_pr_test);

    /// @test Parse results in @p --flag=bool.
    auto tof1_kw_pr = tm.test_with_hint(of1_kw_pr_gen, of1_kw_pr_test);

    /// @test Parse results in @p --flag or @p -f.
    // GCC fails this test with third parameter = std::identity
    // that stupid labmda is the workaround
    auto tof1_k_pr =
        tm.invariant(of1_k_pr_gen, of1_k_pr_test, [](bool r) { return r; });

    /// @test Parse results in @p --flag or @p -f with true default.
    auto tof2_k_pr =
        tm.invariant(of2_k_pr_gen, of2_k_pr_test, std::logical_not<>{});

    /// @test Parse results in <tt>--multi-val vals...</tt>
    auto tom_k_pr = tm.test_with_hint(om_k_pr_gen, om_k_pr_test);

    /// @test <tt>--two-val value</tt> few arguments exception test.
    auto to2_k_few_args_ex =
        tm.invariant(o2_k_few_gen, o2_k_few_test, check_few_args);

    /// @test KeyParser ignore test.
    auto tk_ignore =
        tm.invariant(ignore_gen, k_ignore_test, std::logical_not<>{});

    /// @test KeyCharSeqParser parse result.
    auto to1_kcs_pr = tm.test_with_hint(o1_kcs_pr_gen, o1_kcs_pr_test);

    /// @test KeyCharSeqParser flags.
    auto tf_kcs_pr = [&]()
    {
        ArgvMock mock({"-fF"s});
        auto     pr = test_parser_pr(std::tuple{of1, of2},
                                 std::tuple{},
                                 keycharseq_parser,
                                 mock());
        bool     f1 = pr.get_opt<'f'>();
        bool     f2 = pr.get_opt<'F'>();
        return f1 && !f2;
    };

    /// @test KeyCharSeqParser multiple flags flips.
    auto tf_kcs_pr_m =
        tm.test_with_hint(of_kcs_pr_gen, of_kcs_pr_test, cmp_pairs);

    show_help("help show demo", options, positionals);

    track.check(NamedTest(to1_kw_pr, "KeywordValueParser parsing results"));
    track.check(
        NamedTest(to1_ignore, "KeywordValueParser ignore non-matching"));
    track.check(
        NamedTest(to1_wrong_keyword, "KeywordValueParser keyword exception"));
    track.check(NamedTest(to2_kw_few_args_ex,
                          "KeywordValueParser few arguments exception"));
    track.check(NamedTest(tof1_kw_pr,
                          "KeywordValueParser flag converter parsing results"));
    track.check(NamedTest(to1_k_pr, "KeyParser single value parsing results"));
    track.check(NamedTest(to2_k_pr, "KeyParser two values parsing results"));
    track.check(NamedTest(tof1_k_pr, "KeyParser flag parsing results"));
    track.check(NamedTest(tof2_k_pr, "KeyParser reverse flag parsing results"));
    track.check(NamedTest(tom_k_pr, "KeyParser multi values parsing results"));
    track.check(
        NamedTest(to2_k_few_args_ex, "KeyParser few arguments exception"));
    track.check(NamedTest(tk_ignore, "KeyParser ignore non-matching"));
    track.check(
        NamedTest(to1_kcs_pr, "KeyCharSeqParser single value parsing results"));
    track.check(NamedTest(tf_kcs_pr, "KeyCharSeqParser two flags"));
    track.check(
        NamedTest(tf_kcs_pr_m, "KeyCharSeqParser two flags multiple flips"));

    return track.ret_code();
}
