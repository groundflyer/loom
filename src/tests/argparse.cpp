// -*- C++ -*-
/// @file argparse.cpp
/// @brief argparse module tests.

#include <common/argparse.hpp>
#include <common/test.hpp>
#include <common/tuple.hpp>

#include <algorithm>
#include <array>
#include <bitset>
#include <initializer_list>
#include <ranges>
#include <string>
#include <vector>

#include <fmt/ranges.h>

using namespace loom;
using namespace loom::argparse;
using namespace loom::test;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

using strvec = std::vector<std::string>;

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
    strvec             m_words;
    std::vector<char*> m_pointers;

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

    explicit ArgvMock(strvec&& words) : m_words(std::move(words))
    {
        update_argv();
    }

    explicit ArgvMock(const strvec& words) : m_words(words) { update_argv(); }

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

    char**
    argv() const noexcept
    {
        return const_cast<char**>(m_pointers.data());
    }

    int
    argc() const noexcept
    {
        return m_pointers.size();
    }

    ArgsRange
    operator()()
    {
        return {argc(), argv()};
    }

    ArgsRange
    operator()() const
    {
        return {argc(), argv()};
    }
};

template <bool print = false>
struct ErrorCatcher
{
    bool few_args       = false;
    bool unexpected_val = false;
    bool keychar_ex     = false;
    bool keyword_ex     = false;
    bool help           = false;

    void
    operator()(FewArgumentsException e) noexcept
    {
        if constexpr(print)
            fmt::print("Few Arguments exception: {}\n", e.keyword);
        few_args = true;
    }

    void
    operator()(UnexpectedValue e) noexcept
    {
        if constexpr(print) fmt::print("Unexpected Value: {}\n", e.value);
        unexpected_val = true;
    }

    void
    operator()(KeyException<char> e) noexcept
    {
        if constexpr(print) fmt::print("Keychar exception: {}\n", e.value);
        keychar_ex = true;
    }

    void
    operator()(KeyException<std::string_view> e) noexcept
    {
        if constexpr(print) fmt::print("Keyword exception: {}\n", e.value);
        keyword_ex = true;
    }

    void
    operator()(HelpRequest) noexcept
    {
        if constexpr(print) fmt::print("Help requested\n");
        help = true;
    }
};

template <bool print = false>
class TupleCmp
{
    template <typename T1, typename T2, std::size_t... Is>
    constexpr static bool
    cmp_tuples_impl(const T1& t1,
                    const T2& t2,
                    std::index_sequence<Is...>) noexcept
    {
        return (... && (std::get<Is>(t1) == std::get<Is>(t2)));
    }

  public:
    template <typename T1, typename T2>
    bool
    operator()(const T1& t1, const T2& t2) const noexcept
        requires(std::tuple_size_v<T1> == std::tuple_size_v<T2>)
    {
        bool ret =
            cmp_tuples_impl(t1,
                            t2,
                            std::make_index_sequence<std::tuple_size_v<T1>>{});
        if constexpr(print)
            if(!ret)
            {
                auto printer = [](const auto&... elems)
                { (..., fmt::print("{}\n", elems)); };
                fmt::print("TEST ELEMS\n");
                std::apply(printer, t1);
                fmt::print("VALID ELEMS\n");
                std::apply(printer, t2);
            }
        return ret;
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
        strvec         ret;
        const unsigned l = lgen();
        ret.reserve(l);
        std::generate_n(std::back_inserter(ret), l, wgen);
        return ret;
    }
    else
    {
        strvec ret;
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
        strvec ws{std::move(key)};
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

template <typename... Options, typename... Positionals, typename Matcher>
auto
test_parser_pr(const std::tuple<Options...>&     options,
               const std::tuple<Positionals...>& positionals,
               Matcher                           p,
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

template <typename Options, typename Positionals, typename Matcher>
bool
test_parser_ret(const Options&     options,
                const Positionals& positionals,
                Matcher            p,
                ArgsRange          args) noexcept
{
    PARSE_INIT;
    return p(iter, opt_state, pos_state, error);
};

template <typename Options, typename Positionals, typename Matcher>
ErrorState
test_parser_error(const Options&     options,
                  const Positionals& positionals,
                  Matcher            p,
                  ArgsRange          args) noexcept
{
    PARSE_INIT;
    p(iter, opt_state, pos_state, error);
    return error;
};

template <typename Options, typename Positionals, typename Matcher>
bool
test_parser_iter(const Options&     options,
                 const Positionals& positionals,
                 Matcher            p,
                 ArgsRange          args) noexcept
{
    PARSE_INIT;
    p(iter, opt_state, pos_state, error);
    return iter == std::ranges::end(args);
};

constexpr auto o1 =
    option<'o', "option">("long doc for option", "O", "default");
constexpr auto o1kc =
    option<'c'>("long doc for option with keychar only", "C", "default");
constexpr auto o1kw =
    option<"keyword">("long doc for option with keyword only", "K", "default");
constexpr auto of1 = flag<'f', "flag">("long doc for flag");
constexpr auto of2 = flag<'F', "Flag">("long doc for inverse Flag", true);
constexpr auto o2  = option<"two-val", 2>(
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

constexpr auto pos1 = positional("doc for 1st positional", "pos1");
constexpr auto pos2 =
    positional<2ul>("doc for 2nd positional", {"pos21", "pos22"});
constexpr auto posl = positional_list("doc for positional list", "poss");

constexpr std::tuple options{o1, o1kc, o1kw, of1, of2, o2, om};
constexpr std::tuple positionals{pos1, pos2, posl};

constexpr KeywordValueMatcher keyword_value_parser{};
constexpr KeyMatcher          key_parser{};
constexpr KeyCharSeqMatcher   keycharseq_parser{};
constexpr PositionalsMatcher  pos_parser{};

constexpr TupleCmp tuple_cmp{};

template <typename Option, typename Tuple>
decltype(auto)
get_corr_opt(Option, Tuple&& t) noexcept
{
    return (std::get<typelist::index<
                std::remove_cvref_t<Option>,
                typelist::from<std::remove_cvref_t<decltype(options)>>>>(
        std::forward<Tuple>(t)));
}

auto
default_vals() noexcept
{
    return std::tuple{
        "default"s,
        "default"s,
        "default"s,
        false,
        true,
        strvec{"1111"s, "2222"s},
        strvec{}
    };
}

#define PUSH_KEY_VAL(OPT)               \
    args.push_back(gen_key(OPT, cgen)); \
    args.push_back(val)

template <typename C, typename W, typename L>
auto
gen_full(C& cgen, W& wgen, L& lgen, RandomState& rs)
{
    auto                                    opt_vals = default_vals();
    std::tuple<std::string, strvec, strvec> pos_vals;

    constexpr std::size_t num_opts = std::tuple_size_v<decltype(opt_vals)>;

    std::bitset<num_opts> opt_set;
    std::bitset<3>        pos_set;

    auto opt_choice = rs.make_gen<unsigned>(0, num_opts - 1);
    auto threeway   = rs.make_gen<unsigned>(0, 2);
    auto con_gen    = rs.make_gen<bool>(0.8);

    strvec args{"program name"};

    bool last_multival = false;

    // flag generator
    auto fgen = [&](const auto& o)
    {
        bool& val = get_corr_opt(o, opt_vals);
        // --flag=true or --flag
        if(cgen())
        {
            val = cgen();
            args.push_back(make_keyword(o) + '=' + (val ? "true"s : "false"s));
        }
        else
        {
            args.push_back(gen_key(o, cgen));
            val = o.default_value ^ true;
        }
    };

    while(!pos_set.all() || (con_gen() && !opt_set.all()))
    {
        // set positional
        if(!pos_set.all() && cgen())
        {
            if(last_multival) args.push_back("--"s);

            switch(pos_set.count())
            {
            case 0: // pos1
                std::get<0>(pos_vals) = wgen();
                args.push_back(std::get<0>(pos_vals));
                pos_set.set(0);
                break;
            case 1: // pos2
                std::get<1>(pos_vals).push_back(wgen());
                std::get<1>(pos_vals).push_back(wgen());
                args.push_back(std::get<1>(pos_vals)[0]);
                args.push_back(std::get<1>(pos_vals)[1]);
                pos_set.set(1);
                break;
            case 2: // posl
                unsigned l = lgen();
                std::get<2>(pos_vals).reserve(l);
                for(unsigned i = 0; i < l; ++i)
                {
                    std::string val = wgen();
                    args.push_back(val);
                    std::get<2>(pos_vals).push_back(std::move(val));
                }
                pos_set.set(2);
                break;
            }

            last_multival = false;
        }
        else if(!opt_set.all())
        {
            unsigned choice = opt_choice();
            // we don't wanna use same option twice
            while(opt_set.test(choice)) choice = opt_choice();

            // FIXME: can be extended with a couple options:
            // combination of flags: -fF
            // combination of flags with single value at the end: -fFcvalue
            // combination of flags with multi value at the end: -fFm foo bar...
            switch(choice)
            {
            case 0: // o1
            {
                std::string& val = get_corr_opt(o1, opt_vals);
                val              = wgen();

                // --option=value or -o value or -ovalue
                switch(threeway())
                {
                case 0: args.push_back(make_keyword(o1) + '=' + val); break;
                case 1: PUSH_KEY_VAL(o1); break;
                case 2: args.push_back(make_keychar(o1) + val); break;
                }

                opt_set.set(0);
                last_multival = false;
                break;
            }
            case 1: // o1kc
            {
                std::string& val = get_corr_opt(o1kc, opt_vals);
                val              = wgen();

                // -cvalue or -c value
                if(cgen())
                    args.push_back(make_keychar(o1kc) + val);
                else { PUSH_KEY_VAL(o1kc); }

                opt_set.set(1);
                last_multival = false;
                break;
            }
            case 2: // o1kw
            {
                std::string& val = get_corr_opt(o1kw, opt_vals);
                val              = wgen();

                if(cgen())
                    args.push_back(make_keyword(o1kw) + '=' + val);
                else { PUSH_KEY_VAL(o1kw); }
                opt_set.set(2);
                last_multival = false;
                break;
            }
            case 3: // -f
                fgen(of1);
                opt_set.set(3);
                last_multival = false;
                break;
            case 4: // -F
                fgen(of2);
                opt_set.set(4);
                last_multival = false;
                break;
            case 5: // o2
            {
                strvec& val = get_corr_opt(o2, opt_vals);
                val         = {wgen(), wgen()};
                args.push_back(gen_key(o2, cgen));
                args.push_back(val[0]);
                args.push_back(val[1]);
                opt_set.set(5);
                last_multival = false;
                break;
            }
            case 6: // om
            {
                strvec& vals = get_corr_opt(om, opt_vals);
                vals.clear();
                unsigned l = lgen();
                vals.reserve(l);
                args.push_back(gen_key(om, cgen));
                for(unsigned i = 0; i < l; ++i)
                {
                    std::string val = wgen();
                    args.push_back(val);
                    vals.push_back(std::move(val));
                }
                opt_set.set(6);
                last_multival = true;
                break;
            }
            }
        }
    }

    return std::pair{ArgvMock(std::move(args)),
                     std::tuple_cat(std::move(opt_vals), std::move(pos_vals))};
}

bool
parsing_condition(bool r, ArgsIterator iter, ErrorState error) noexcept
{
    return r && iter != std::default_sentinel && !error.has_value();
}

template <typename Pos>
auto
parse_positionals_pr(const Pos& positionals, ArgsRange args)
{
    std::tuple<> options;
    PARSE_INIT;
    bool r = true;
    while(parsing_condition(r, iter, error))
        r = pos_parser(iter, opt_state, pos_state, error);

    return ParseResults(pos_state.bindings);
}

template <typename Pos>
ErrorState
parse_positionals_er(const Pos& positionals, ArgsRange args)
{
    std::tuple<> options;
    PARSE_INIT;
    bool r = true;
    while(parsing_condition(r, iter, error))
        r = pos_parser(iter, opt_state, pos_state, error);

    return error;
}

template <typename Pos>
bool
parse_positionals_iter(const Pos& positionals, ArgsRange args)
{
    std::tuple<> options;
    PARSE_INIT;
    bool r = true;
    while(parsing_condition(r, iter, error))
        r = pos_parser(iter, opt_state, pos_state, error);

    return iter == std::ranges::end(args);
}

// there is a mess down here, but who cares: it's not an API, just tests
// FIXME: refactor this
int main(/*int argc, char* argv[]*/)
{
    Track         track;
    ParmTestMaker tm{10000};
    RandomState   rs;

    constexpr auto symbols = make_symbols();
    auto           lgen    = rs.make_gen<unsigned>(2, 50);
    auto           symbolgen =
        [&symbols,
         d = rs.make_gen<std::size_t>(0, symbols.size() - 1)]() mutable
    { return symbols[d()]; };
    auto cgen = rs.make_gen<bool>(0.5);
    auto wgen = [&](bool dash = true)
    {
        std::string ret;
        unsigned    l = lgen();
        // randomly add '-' in front
        bool add_dash  = dash && l > 2 && cgen();
        bool add_2dash = add_dash && cgen();
        ret.reserve(l + add_dash + add_2dash);
        if(add_dash) ret.push_back('-');
        if(add_2dash) ret.push_back('-');
        std::generate_n(std::back_inserter(ret), l, symbolgen);
        return ret;
    };
    auto sgen = [&](bool dash = true)
    {
        strvec   ret;
        unsigned n = lgen();
        ret.reserve(n);
        std::generate_n(std::back_inserter(ret),
                        n,
                        [&]() { return wgen(dash); });
        return ret;
    };

    auto wgen_no_dash       = [&]() { return wgen(false); };
    auto sgen_no_dash       = [&]() { return sgen(false); };
    auto ignore_gen         = [&]() { return ArgvMock(sgen()); };
    auto ignore_gen_no_dash = [&]() { return ArgvMock(sgen_no_dash()); };

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
    MAKE_ERROR_CHECK(keychar_ex);
    MAKE_ERROR_CHECK(few_args);
    MAKE_ERROR_CHECK(unexpected_val);

    auto o1_kw_pr_gen   = [&]() { return gen_keyword_value(o1, wgen); };
    auto o1_kw_halt_gen = [&]() { return gen_keyword_value(o1, wgen).first; };
    auto o1_pr_test     = [&](const ArgvMock& mock, auto p)
    {
        return std::string(
            test_parser_pr(std::tuple{o1}, std::tuple{}, p, mock())
                .template get_opt<"option">());
    };
    auto o1_kw_pr_test = [&](const ArgvMock& mock)
    { return o1_pr_test(mock, keyword_value_parser); };
    auto o1_kw_iter_test = [&](const ArgvMock& mock)
    {
        return test_parser_iter(std::tuple{o1},
                                std::tuple{},
                                keyword_value_parser,
                                mock());
    };
    auto o1_k_pr_gen  = [&]() { return gen_key_vals(o1, wgen, cgen, lgen); };
    auto o1_k_pr_test = [&](const ArgvMock& mock)
    { return o1_pr_test(mock, key_parser); };
    auto o1_k_halt_gen = [&]()
    { return gen_key_vals(o1, wgen, cgen, lgen).first; };
    auto o1_k_halt_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{o1},
                               std::tuple{},
                               key_parser,
                               mock());
    };
    auto o1_k_iter_test = [&](const ArgvMock& mock)
    {
        return test_parser_iter(std::tuple{o1},
                                std::tuple{},
                                key_parser,
                                mock());
    };
    auto o1_kcs_pr_gen = [&]()
    {
        std::string val = wgen();
        return std::pair{ArgvMock({make_keychar(o1) + val}), val};
    };
    auto o1_kcs_pr_test = [&](const ArgvMock& mock)
    { return o1_pr_test(mock, keycharseq_parser); };
    auto o1_kw_ignore_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{o1},
                               std::tuple{},
                               keyword_value_parser,
                               mock());
    };
    auto o1_kw_halt_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{o1},
                               std::tuple{},
                               keyword_value_parser,
                               mock());
    };

    auto o1_wrong_keyword_gen = [&]()
    // there is negligible chance that wgen() produces "option"
    { return ArgvMock({"--"s + wgen() + '=' + wgen()}); };
    auto o1_wrong_keyword_test = [&](const ArgvMock& mock)
    {
        return test_parser_error(std::tuple{o1},
                                 std::tuple{},
                                 keyword_value_parser,
                                 mock());
    };

    auto o2_kw_few_gen = [&]()
    { return ArgvMock({make_keyword(o2) + '=' + wgen()}); };
    auto o2_kw_few_test = [&](const ArgvMock& mock)
    {
        return test_parser_error(std::tuple{o2},
                                 std::tuple{},
                                 keyword_value_parser,
                                 mock());
    };
    auto o2_k_few_gen = [&]() { return ArgvMock({gen_key(o2, cgen), wgen()}); };
    auto o2_k_few_test = [&](const ArgvMock& mock)
    {
        return test_parser_error(std::tuple{o2},
                                 std::tuple{},
                                 key_parser,
                                 mock());
    };

    auto of1_kw_pr_gen  = [&]() { return gen_keyword_value_bool(of1, cgen); };
    auto of1_kw_pr_test = [&](const ArgvMock& mock) noexcept
    {
        return test_parser_pr(std::tuple{of1},
                              std::tuple{},
                              keyword_value_parser,
                              mock())
            .template get_opt<"flag">();
    };
    auto of1_k_pr_gen  = [&]() { return ArgvMock({gen_key(of1, cgen)}); };
    auto of1_k_pr_test = [&](const ArgvMock& mock) noexcept
    {
        return test_parser_pr(std::tuple{of1}, std::tuple{}, key_parser, mock())
            .template get_opt<"flag">();
    };
    auto of2_k_pr_gen  = [&]() { return ArgvMock({gen_key(of2, cgen)}); };
    auto of2_k_pr_test = [&](const ArgvMock& mock) noexcept
    {
        return test_parser_pr(std::tuple{of2}, std::tuple{}, key_parser, mock())
            .template get_opt<'F'>();
    };

    auto o2_k_pr_gen  = [&]() { return gen_key_vals(o2, wgen, cgen, lgen); };
    auto o2_k_pr_test = [&](const ArgvMock& mock)
    {
        auto rng =
            test_parser_pr(std::tuple{o2}, std::tuple{}, key_parser, mock())
                .template get_opt<"two-val">();
        return strvec(std::ranges::begin(rng), std::ranges::end(rng));
    };
    auto o2_k_halt_gen  = [&]() { return o2_k_pr_gen().first; };
    auto o2_k_halt_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{o2},
                               std::tuple{},
                               key_parser,
                               mock());
    };
    auto o2_k_iter_test = [&](const ArgvMock& mock)
    {
        return test_parser_iter(std::tuple{o2},
                                std::tuple{},
                                key_parser,
                                mock());
    };

    auto om_k_pr_gen = [&]()
    { return gen_key_vals(om, wgen_no_dash, cgen, lgen); };
    auto om_k_pr_test = [&](const ArgvMock& mock)
    {
        auto rng =
            test_parser_pr(std::tuple{om}, std::tuple{}, key_parser, mock())
                .template get_opt<'m'>();
        return strvec(std::ranges::begin(rng), std::ranges::end(rng));
    };
    auto om_k_halt_gen  = [&]() { return om_k_pr_gen().first; };
    auto om_k_halt_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{om},
                               std::tuple{},
                               key_parser,
                               mock());
    };
    auto om_k_iter_test = [&](const ArgvMock& mock)
    {
        auto er =
            test_parser_error(std::tuple{om}, std::tuple{}, key_parser, mock());
        ErrorCatcher<true> ec;
        if(er.has_value()) std::visit(ec, er.value());
        return test_parser_iter(std::tuple{om},
                                std::tuple{},
                                key_parser,
                                mock());
    };
    auto k_ignore_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{o1, o1kc, o1kw, o2, of1, of2, om},
                               std::tuple{},
                               key_parser,
                               mock());
    };

    auto kcs_multi_flags = [&](unsigned n1, unsigned n2)
    {
        std::string r;
        r.reserve(n1 + n2 + 1);
        for(unsigned i = 0; i < n1; ++i) r.push_back('f');
        for(unsigned i = 0; i < n2; ++i) r.push_back('F');
        std::shuffle(r.begin(), r.end(), rs.g);
        r.insert(0, 1, '-');
        return r;
    };
    auto of_kcs_pr_gen = [&]()
    {
        unsigned n1 = lgen();
        unsigned n2 = lgen();
        return std::pair{
            ArgvMock({kcs_multi_flags(n1,   n2)}
            ),
            std::pair{              true, false}
        };
    };
    auto of_kcs_pr_test = [&](const ArgvMock& mock)
    {
        auto pr = test_parser_pr(std::tuple{of1, of2},
                                 std::tuple{},
                                 keycharseq_parser,
                                 mock());
        bool f1 = pr.get_opt<'f'>();
        bool f2 = pr.get_opt<'F'>();
        return std::pair{f1, f2};
    };
    auto of_kcs_halt_gen  = [&]() { return of_kcs_pr_gen().first; };
    auto of_kcs_halt_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{of1, of2},
                               std::tuple{},
                               keycharseq_parser,
                               mock());
    };
    auto of_kcs_iter_test = [&](const ArgvMock& mock)
    {
        return test_parser_iter(std::tuple{of1, of2},
                                std::tuple{},
                                keycharseq_parser,
                                mock());
    };
    auto cmp_pairs = [](auto p1, auto p2) noexcept
    { return p1.first == p2.first && p1.second == p2.second; };

    auto o1_last_kcs_pr_gen = [&]()
    {
        std::string v = wgen();
        return std::pair{ArgvMock({kcs_multi_flags(lgen(), lgen()) + 'o', v}),
                         v};
    };
    auto o1_last_kcs_pr_test = [&](const ArgvMock& mock)
    {
        auto pr = test_parser_pr(std::tuple{o1, of1, of2},
                                 std::tuple{},
                                 keycharseq_parser,
                                 mock());
        return std::string{pr.get_opt<'o'>()};
    };

    auto kcs_keychar_ex_gen = [&]()
    {
        std::string r = (cgen() ? "-f" : "-F") + wgen(false);
        return ArgvMock({r});
    };
    auto kcs_keychar_ex_test = [&](const ArgvMock& mock)
    {
        return test_parser_error(std::tuple{of1, of2},
                                 std::tuple{},
                                 keycharseq_parser,
                                 mock());
    };

    auto kcs_ignore_test = [&](const ArgvMock& mock)
    {
        return test_parser_ret(std::tuple{o1, o1kc, o1kw, o2, of1, of2, om},
                               std::tuple{},
                               keycharseq_parser,
                               mock());
    };

    auto pp_pr_gen = [&]()
    {
        auto s = sgen();
        while(s.size() < 4) s = sgen();
        return std::pair{
            ArgvMock(s),
            std::tuple{s.front(),
                       strvec(s.begin() + 1, s.begin() + 3),
                       strvec(s.begin() + 3, s.end())}
        };
    };
    auto pp_pr_test = [&](const ArgvMock& mock)
    {
        auto pr    = parse_positionals_pr(positionals, mock());
        auto pos2r = pr.get_pos<1>();
        auto poslr = pr.get_pos<2>();
        return std::tuple{std::string{pr.get_pos<0>()},
                          strvec(pos2r.begin(), pos2r.end()),
                          strvec(poslr.begin(), poslr.end())};
    };
    auto pp_iter_gen  = [&]() { return pp_pr_gen().first; };
    auto pp_iter_test = [&](const ArgvMock& mock)
    { return parse_positionals_iter(positionals, mock()); };
    auto pp_un_gen = [&]()
    {
        auto s = sgen();
        while(s.size() < 5) s = sgen();
        return ArgvMock(std::move(s));
    };
    auto pp_un_test = [&](const ArgvMock& mock)
    { return parse_positionals_er(std::tuple{pos1, pos2}, mock()); };
    auto pp_few_gen  = [&]() { return ArgvMock({wgen()}); };
    auto pp_few_test = [&](const ArgvMock& mock)
    { return parse_positionals_er(std::tuple{pos2}, mock()); };

    auto full_few_test = [&](const ArgvMock& mock)
    {
        ErrorCatcher ec;
        parse(options, positionals, mock(), ec);
        return ec.few_args;
    };

    auto full_pr_gen = [&]() { return gen_full(cgen, wgen_no_dash, lgen, rs); };
    auto full_pr_test = [&](const ArgvMock& mock)
    {
        ErrorCatcher ec;
        auto         pr     = parse(options, positionals, mock(), ec);
        auto         o1_r   = pr.get_opt<'o'>();
        auto         o1kc_r = pr.get_opt<'c'>();
        auto         o1kw_r = pr.get_opt<"keyword">();
        auto         of1_r  = pr.get_opt<'f'>();
        auto         of2_r  = pr.get_opt<'F'>();
        auto         o2_r   = pr.get_opt<"two-val">();
        auto         om_r   = pr.get_opt<'m'>();
        auto         p1_r   = pr.get_pos<0>();
        auto         p2_r   = pr.get_pos<1>();
        auto         pl_r   = pr.get_pos<2>();
        return std::tuple{
            std::string(o1_r),
            std::string(o1kc_r),
            std::string(o1kw_r),
            of1_r,
            of2_r,
            strvec{std::string(o2_r[0]), std::string(o2_r[1])},
            strvec(std::ranges::begin(om_r), std::ranges::end(om_r)),
            std::string(p1_r),
            strvec(std::ranges::begin(p2_r), std::ranges::end(p2_r)),
            strvec(std::ranges::begin(pl_r), std::ranges::end(pl_r))
        };
    };

    auto full_er_gen  = [&]() { return full_pr_gen().first; };
    auto full_er_test = [&](const ArgvMock& mock)
    {
        bool err = false;
        parse(options, positionals, mock(), [&err](auto) { err = true; });
        return err;
    };

    // GCC fails with std::identity
    // that stupid labmda is the workaround
    auto copy_id = [](auto v) noexcept { return v; };

    /// @test @p --keyword=value parsing results test.
    /// Generate correct keyword and correct value.
    /// Check if binding holds the value.
    auto to1_kw_pr = tm.with_hint(o1_kw_pr_gen, o1_kw_pr_test);

    /// @test @p --keyword=value ignore test.
    /// Generate random words that don't match @p --keyword=value pattern.
    /// Check if parser ignores them.
    auto to1_kw_ignore =
        tm.invariant(ignore_gen, o1_kw_ignore_test, std::logical_not<>{});

    /// @test @p --keyword=value halt test.
    auto to1_kw_halt = tm.invariant(o1_kw_halt_gen, o1_kw_halt_test, copy_id);

    /// @test @p --keyword=value iterator test.
    auto to1_kw_iter = tm.invariant(o1_kw_halt_gen, o1_kw_iter_test, copy_id);

    /// @test @p --keyword=value wrong key test.
    /// Generate @p --keyword=value string where @p keyword is invalid.
    /// Check if loom::argparse::KeyException is stored in
    /// loom::argparse::ErrorState
    auto to1_kw_wrong_keyword = tm.invariant(o1_wrong_keyword_gen,
                                             o1_wrong_keyword_test,
                                             check_keyword_ex);

    /// @test @p --keyword=value few arguments exception test.
    /// Generate @p --keyword=value string for option that requires 2 operands.
    /// Check if loom::argparse::FewArgumentsException is stored.
    auto to2_kw_few_args_ex =
        tm.invariant(o2_kw_few_gen, o2_kw_few_test, check_few_args);

    /// @test Parse results in @p --flag=bool.
    auto tof1_kw_pr = tm.with_hint(of1_kw_pr_gen, of1_kw_pr_test);

    /// @test <tt>-k val</tt> or <tt>\--keyword val</tt> parsing results test.
    /// Generate correct key and value.
    /// Check if binding holds the value.
    auto to1_k_pr = tm.with_hint(o1_k_pr_gen, o1_k_pr_test);

    /// @test <tt>-k val</tt> or <tt>\--keyword val</tt> halt test.
    auto to1_k_halt = tm.invariant(o1_k_halt_gen, o1_k_halt_test, copy_id);

    /// @test <tt>-k val</tt> or <tt>\--keyword val</tt> iterator test.
    auto to1_k_iter = tm.invariant(o1_k_halt_gen, o1_k_iter_test, copy_id);

    /// @test <tt>--two-val val1 val2</tt> halt test.
    auto to2_k_halt = tm.invariant(o2_k_halt_gen, o2_k_halt_test, copy_id);

    /// @test <tt>--two-val val1 val2</tt> iterator test.
    auto to2_k_iter = tm.invariant(o2_k_halt_gen, o2_k_iter_test, copy_id);

    /// @test <tt>--two-val val1 val2</tt> parsing result test.
    auto to2_k_pr = tm.with_hint(o2_k_pr_gen, o2_k_pr_test);

    /// @test Parse results in @p --flag or @p -f.
    auto tof1_k_pr = tm.invariant(of1_k_pr_gen, of1_k_pr_test, copy_id);

    /// @test Parse results in @p --flag or @p -f with true default.
    auto tof2_k_pr =
        tm.invariant(of2_k_pr_gen, of2_k_pr_test, std::logical_not<>{});

    /// @test Parse results in <tt>--multi-val vals...</tt>
    auto tom_k_pr = tm.with_hint(om_k_pr_gen, om_k_pr_test);

    /// @test <tt>--multi-val vals...</tt> halt test.
    auto tom_k_halt = tm.invariant(om_k_halt_gen, om_k_halt_test, copy_id);

    /// @test <tt>--multi-val vals...</tt> iter test.
    auto tom_k_iter = tm.invariant(om_k_halt_gen, om_k_iter_test, copy_id);

    /// @test <tt>--two-val value</tt> few arguments exception test.
    auto to2_k_few_args_ex =
        tm.invariant(o2_k_few_gen, o2_k_few_test, check_few_args);

    /// @test KeyMatcher ignore test.
    auto tk_ignore =
        tm.invariant(ignore_gen_no_dash, k_ignore_test, std::logical_not<>{});

    /// @test KeyCharSeqMatcher parse result.
    auto to1_kcs_pr = tm.with_hint(o1_kcs_pr_gen, o1_kcs_pr_test);

    /// @test KeyCharSeqMatcher flags.
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

    /// @test KeyCharSeqMatcher multiple flags flips.
    auto tf_kcs_pr_m = tm.with_hint(of_kcs_pr_gen, of_kcs_pr_test, cmp_pairs);

    /// @test KeyCharSeqMatcher multiple flags and last option.
    /// Generate key char sequence with last key char and an operand.
    auto to1_kcs_last_pr =
        tm.with_hint(o1_last_kcs_pr_gen, o1_last_kcs_pr_test);

    /// @test KeyCharSeqMatcher keychar exception.
    auto t_kcs_keychar_ex =
        tm.invariant(kcs_keychar_ex_gen, kcs_keychar_ex_test, check_keychar_ex);

    /// @test KeyCharSeqMatcher ignore test.
    auto t_kcs_ignore =
        tm.invariant(ignore_gen_no_dash, kcs_ignore_test, std::logical_not<>{});

    /// @test KeyCharSeqMatcher halt test.
    auto tf_kcs_halt = tm.invariant(of_kcs_halt_gen, of_kcs_halt_test, copy_id);

    /// @test KeyCharSeqMatcher iterator test.
    auto tf_kcs_iter = tm.invariant(of_kcs_halt_gen, of_kcs_iter_test, copy_id);

    /// @test Positionals parser results.
    auto t_pp_pr = tm.with_hint(pp_pr_gen, pp_pr_test, ::tuple_cmp);

    /// @test Positionals parser iterator test.
    auto t_pp_iter = tm.invariant(pp_iter_gen, pp_iter_test, copy_id);

    /// @test Positionals parser unexpected value exception.
    auto t_pp_un = tm.invariant(pp_un_gen, pp_un_test, check_unexpected_val);

    /// @test Positionals parser few arguments exception.
    auto t_pp_few = tm.invariant(pp_few_gen, pp_few_test, check_few_args);

    /// @test loom::argparse::parse for few arguments exception.
    auto t_full_few = tm.invariant(pp_few_gen, full_few_test, copy_id);

    /// @test loom::argparse::parse parsing results.
    auto t_full_pr = tm.with_hint(full_pr_gen, full_pr_test, TupleCmp<true>{});

    /// @test Check that loom::argparse::parse doesn't generate errors with
    /// valid data.
    auto t_full_er =
        tm.invariant(full_er_gen, full_er_test, std::logical_not<>{});

    show_help("help show demo", options, positionals);

    track.check(NamedTest(to1_kw_pr, "KeywordValueMatcher parsing results"));
    track.check(
        NamedTest(to1_kw_ignore, "KeywordValueMatcher ignore non-matching"));
    track.check(NamedTest(to1_kw_halt, "KeywordValueMatcher halt"));
    track.check(NamedTest(to1_kw_iter, "KeywordValueMatcher iterator"));
    track.check(NamedTest(to1_kw_wrong_keyword,
                          "KeywordValueMatcher keyword exception"));
    track.check(NamedTest(to2_kw_few_args_ex,
                          "KeywordValueMatcher few arguments exception"));
    track.check(
        NamedTest(tof1_kw_pr,
                  "KeywordValueMatcher flag converter parsing results"));
    track.check(NamedTest(to1_k_pr, "KeyMatcher single value parsing results"));
    track.check(NamedTest(to1_k_halt, "KeyMatcher single value halt"));
    track.check(NamedTest(to1_k_iter, "KeyMatcher single value iterator"));
    track.check(NamedTest(to2_k_halt, "KeyMatcher two values halt"));
    track.check(NamedTest(to2_k_iter, "KeyMatcher two values iterator"));
    track.check(NamedTest(to2_k_pr, "KeyMatcher two values parsing results"));
    track.check(NamedTest(tof1_k_pr, "KeyMatcher flag parsing results"));
    track.check(
        NamedTest(tof2_k_pr, "KeyMatcher reverse flag parsing results"));
    track.check(NamedTest(tom_k_halt, "KeyMatcher multi values halt"));
    track.check(NamedTest(tom_k_iter, "KeyMatcher multi values iterator"));
    track.check(NamedTest(tom_k_pr, "KeyMatcher multi values parsing results"));
    track.check(
        NamedTest(to2_k_few_args_ex, "KeyMatcher few arguments exception"));
    track.check(NamedTest(tk_ignore, "KeyMatcher ignore non-matching"));
    track.check(NamedTest(tf_kcs_halt, "KeyCharSeqMatcher halt"));
    track.check(NamedTest(tf_kcs_iter, "KeyCharSeqMatcher iterator"));
    track.check(NamedTest(to1_kcs_pr,
                          "KeyCharSeqMatcher single value parsing results"));
    track.check(NamedTest(tf_kcs_pr, "KeyCharSeqMatcher two flags"));
    track.check(
        NamedTest(tf_kcs_pr_m, "KeyCharSeqMatcher two flags multiple flips"));
    track.check(NamedTest(
        to1_kcs_last_pr,
        "KeyCharSeqMatcher multiple flags and last option parsing results"));
    track.check(
        NamedTest(t_kcs_keychar_ex, "KeyCharSeqMatcher keychar exception"));
    track.check(NamedTest(t_kcs_ignore, "KeyCharSeqMatcher ignore"));
    track.check(NamedTest(t_pp_iter, "PositionalsMatcher iterator"));
    track.check(NamedTest(t_pp_pr, "PositionalsMatcher parsing results"));
    track.check(
        NamedTest(t_pp_un, "PositionalsMatcher unexpected value exception"));
    track.check(
        NamedTest(t_pp_few, "PositionalsMatcher few arguments exception"));
    track.check(NamedTest(t_full_pr, "Full parse parsing results"));
    track.check(NamedTest(t_full_few, "Full parse few arguments exception"));
    track.check(NamedTest(t_full_er, "Full parse no errors with valid input"));

    return track.ret_code();
}
