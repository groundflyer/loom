// -*- C++ -*-
// argparse.hpp
//

#pragma once

#include <utils/tmp.hpp>
#include <utils/tuple.hpp>

#include <array>
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/facade.hpp>

#include <fmt/printf.h>

#include <cstdio>

namespace lucid::argparse
{
template <char... Cs>
using char_sequence = std::integer_sequence<char, Cs...>;

template <typename Key>
struct KeyException
{
    Key value;
};

struct FewArgumentsException
{
    std::string_view keyword;
};

struct UnexpectedValue
{
    std::string_view value;
};

struct HelpRequest
{
};

constexpr bool
is_alpha_upper(char c) noexcept
{
    return c > 64 && c <= 90;
}
constexpr bool
is_alpha_lower(char c) noexcept
{
    return c > 96 && c <= 122;
}
constexpr bool
is_alpha(char c) noexcept
{
    return is_alpha_lower(c) || is_alpha_upper(c);
}
constexpr bool
is_digit(char c) noexcept
{
    return c > 47 && c <= 57;
}
constexpr bool
is_alphadigit(char c) noexcept
{
    return is_alpha(c) || is_digit(c);
}

namespace detail
{
template <char Key, typename Options>
struct key_index;

template <char Key, typename... Options>
struct key_index<Key, std::tuple<Options...>>
{
    static constexpr std::size_t value =
        intlist::index<Key, char_sequence<Options::key...>>;
};

template <typename Keyword, typename Options>
struct keyword_index;

template <typename Keyword, typename... Options>
struct keyword_index<Keyword, std::tuple<Options...>>
{
    static constexpr std::size_t value =
        typelist::index<Keyword,
                        type_sequence<typename Options::keyword_type...>>;
};

template <std::size_t Idx, typename FirstOption, typename... RestOptions>
constexpr std::size_t
tokenize_impl(const char key) noexcept
{
    if(key == FirstOption::key) return Idx;

    if constexpr(sizeof...(RestOptions) > 0ul)
        return tokenize_impl<Idx + 1, RestOptions...>(key);
    else
        return -1ul;
}

template <std::size_t Idx, typename FirstOption, typename... RestOptions>
constexpr std::size_t
tokenize_impl(const std::string_view keyword) noexcept
{
    if(keyword == FirstOption::keyword) return Idx;

    if constexpr(sizeof...(RestOptions) > 0ul)
        return tokenize_impl<Idx + 1, RestOptions...>(keyword);
    else
        return -1ul;
}

template <typename Converter, std::size_t... Idxs>
constexpr auto
argv2array(char*     argc[],
           Converter converter,
           std::index_sequence<Idxs...>) noexcept
{
    using value_type = std::array<
        std::decay_t<std::invoke_result_t<Converter, std::string_view>>,
        sizeof...(Idxs)>;
    return value_type{converter(argc[Idxs])...};
}

// we change state during parsing
template <typename Desc, typename Bindings>
struct State
{
    const Desc& desc;
    Bindings    bindings;
    // option token to store value in
    std::size_t token;
    // how many values remain to set
    std::size_t remain;

    constexpr State(const Desc& _desc,
                    std::size_t _token,
                    std::size_t _remain) noexcept :
        desc(_desc),
        bindings(make_bindings(_desc)), token(_token), remain(_remain)
    {
    }

    State()             = delete;
    State(const State&) = delete;
    State(State&&)      = delete;
    State&
    operator=(const State&) = delete;
};

// empty state if we have no positional arguments
template <>
struct State<std::tuple<>, void>
{
    template <typename... Args>
    constexpr State(Args&&...) noexcept
    {
    }
};

// Empty class tag type to indicate that there is no keyword defined.
struct missing_keyword
{
    using type                             = char_sequence<>;
    static constexpr std::string_view view = "";
};

// This character is used to indicate that character key is not defined.
constexpr inline char missing_keychar = ' ';

template <size_t _N>
struct StringLiteral
{
    // cut off null terminator
    static constexpr std::size_t N = _N - 1;

    constexpr StringLiteral(const char (&str)[_N])
    {
        for(std::size_t i = 0; i < N; ++i) value[i] = str[i];
    }

    char value[N]{};

    constexpr char
    operator[](const std::size_t i) const
    {
        return value[i];
    }

    constexpr std::size_t
    size() const
    {
        return N;
    }

    constexpr std::string_view
    view() const
    {
        return std::string_view(value, N);
    }
};

template <typename CharSeq, StringLiteral s>
struct KeywordWrapper
{
    using type                             = CharSeq;
    static constexpr std::string_view view = s.view();
};

template <StringLiteral s, typename Idxs>
struct sl2cs_impl;

template <StringLiteral s, std::size_t... Idxs>
struct sl2cs_impl<s, std::index_sequence<Idxs...>>
{
    using type = char_sequence<s[Idxs]...>;
};

template <StringLiteral s, typename Idxs>
struct wrap_string_helper
{
    using type = KeywordWrapper<typename sl2cs_impl<s, Idxs>::type, s>;
};

template <typename Converter>
using result_type =
    std::decay_t<std::invoke_result_t<Converter, std::string_view>>;

template <typename S>
struct keyword_ok_impl;

template <char... cs>
struct keyword_ok_impl<char_sequence<cs...>>
{
    static constexpr bool value =
        (true && ... && (is_alphadigit(cs) || cs == '-' || cs == '_'));
};

// Lazy man's flag converter
// converts any word except no, off, false and 0 to true
struct FlagConverter
{
    constexpr bool
    operator()(std::string_view word) const noexcept
    {
        if(word == "no" || word == "off" || word == "false" || word == "0")
            return false;

        return true;
    }
};

template <typename O>
struct has_keychar
{
    static constexpr bool value = O::key != missing_keychar;
};

template <typename O>
struct has_keyword
{
    static constexpr bool value =
        !std::is_same_v<typename O::keyword_type, missing_keyword>;
};

template <typename O>
struct is_flag
{
    static constexpr bool value = O::is_flag;
};
} // namespace detail

template <detail::StringLiteral s>
using string_type =
    typename detail::sl2cs_impl<s, std::make_index_sequence<s.size()>>::type;

template <detail::StringLiteral s>
using wrap_string = typename detail::
    wrap_string_helper<s, std::make_index_sequence<s.size()>>::type;

template <char Key, typename Options>
constexpr std::size_t key_index = detail::key_index<Key, Options>::value;

template <typename Keyword, typename Options>
constexpr std::size_t keyword_index =
    detail::keyword_index<Keyword, Options>::value;

template <detail::StringLiteral s>
constexpr bool keyword_ok = detail::keyword_ok_impl<string_type<s>>::value;

template <typename O>
constexpr bool has_keychar = detail::has_keychar<O>::value;

template <typename O>
constexpr bool has_keyword = detail::has_keyword<O>::value;

template <typename O>
constexpr bool is_flag = detail::is_flag<O>::value;

template <typename O>
constexpr bool is_option = !is_flag<O>;

class ArgsRange : public ranges::view_facade<ArgsRange>
{
    friend ranges::range_access;

    int    idx   = 0;
    int    _argc = 0;
    char** _argv = nullptr;

    bool
    equal(ranges::default_sentinel_t) const noexcept
    {
        return idx == _argc;
    }

  public:
    ArgsRange() = default;

    ArgsRange(int argc, char* argv[]) noexcept : _argc(argc), _argv(argv) {}

    char**
    read() const noexcept
    {
        return _argv + idx;
    }

    void
    next() noexcept
    {
        ++idx;
    }
};

/// @brief Stores pointer to argv for later conversion
template <typename Converter, std::size_t nvals>
class Binding
{
    using value_type = std::array<detail::result_type<Converter>, nvals>;

    Converter   converter;
    value_type  default_value;
    char**      words  = nullptr;
    std::size_t idx    = 0ul;
    bool        is_set = false;

    constexpr void
    increment() noexcept
    {
        is_set |= idx == (nvals - 1);
        idx = (idx + 1ul) % nvals;
    }

  public:
    constexpr Binding(const Converter&  _converter,
                      const value_type& value) noexcept :
        converter(_converter),
        default_value(value)
    {
    }

    constexpr void
    set(std::string_view) noexcept
    {
        increment();
    }

    constexpr void
    set(char** data) noexcept
    {
        if(!words) words = data;
        increment();
    }

    constexpr value_type
    operator()() const
    {
        return is_set ? detail::argv2array(words,
                                           converter,
                                           std::make_index_sequence<nvals>{}) :
                        default_value;
    }
};

template <typename Converter>
class Binding<Converter, 1>
{
    using value_type = detail::result_type<Converter>;

    Converter        converter;
    value_type       default_value;
    std::string_view word;
    bool             is_set = false;

  public:
    constexpr Binding(const Converter&  _converter,
                      const value_type& value) noexcept :
        converter(_converter),
        default_value(value)
    {
    }

    constexpr void
    set(std::string_view new_word) noexcept
    {
        is_set = true;
        word   = new_word;
    }

    constexpr void
    set(char** new_word) noexcept
    {
        set(*new_word);
    }

    constexpr value_type
    operator()() const
    {
        if constexpr(std::is_same_v<value_type, bool>)
            // flip default bool value or convert word if it's available
            return word.empty() ? (default_value ^ is_set) : converter(word);
        else
            return is_set ? converter(word) : default_value;
    }
};

template <typename Converter>
class Binding<Converter, -1ul>
{
    class value_range : public ranges::view_facade<value_range>
    {
        friend ranges::range_access;

        Converter   converter;
        char**      data = nullptr;
        std::size_t size = 0ul;
        std::size_t idx  = 0ul;

        decltype(auto)
        read() const
        {
            return converter(data[idx]);
        }

        bool
        equal(ranges::default_sentinel_t) const noexcept
        {
            return idx == size;
        }

        void
        next() noexcept
        {
            ++idx;
        }

      public:
        value_range() = default;

        explicit value_range(const Converter& _converter,
                             char*            _data[],
                             std::size_t      _size) noexcept :
            converter(_converter),
            data(_data), size(_size)
        {
        }
    };

    Converter   converter;
    char**      data = nullptr;
    std::size_t size = 0ul;

    constexpr void
    increment() noexcept
    {
        ++size;
    }

  public:
    constexpr explicit Binding(const Converter& _converter) noexcept :
        converter(_converter)
    {
    }

    constexpr void
    set(std::string_view) noexcept
    {
        increment();
    }

    constexpr void
    set(char** _data) noexcept
    {
        if(!data) data = _data;

        increment();
    }

    constexpr value_range
    operator()() const noexcept
    {
        return value_range(converter, data, size);
    }
};

template <typename Converter, std::size_t nvals>
class PositionalBinding
{
    Converter   converter;
    char**      words = nullptr;
    std::size_t idx   = 0ul;

  public:
    explicit constexpr PositionalBinding(const Converter& _converter) noexcept :
        converter(_converter)
    {
    }

    constexpr void
    set(std::string_view) noexcept
    {
        ++idx;
    }

    constexpr void
    set(char** data) noexcept
    {
        if(!words) words = data;
        ++idx;
    }

    constexpr auto
    operator()() const
    {
        return detail::argv2array(words,
                                  converter,
                                  std::make_index_sequence<nvals>{});
    }
};

template <typename Converter>
class PositionalBinding<Converter, 1>
{
    Converter        converter;
    std::string_view word;

  public:
    explicit constexpr PositionalBinding(const Converter& _converter) noexcept :
        converter(_converter){};

    constexpr void
    set(std::string_view new_word) noexcept
    {
        word = new_word;
    }

    constexpr void
    set(char** data) noexcept
    {
        set(*data);
    }

    constexpr auto
    operator()() const
    {
        return converter(word);
    }
};

template <char Key, typename Keyword, typename Converter, std::size_t nvals>
struct Option
{
    using singular_value_type = detail::result_type<Converter>;
    using value_type =
        std::conditional_t<(nvals > 1),
                           std::array<singular_value_type, nvals>,
                           singular_value_type>;
    using var_type     = std::conditional_t<(nvals > 1),
                                        std::array<std::string_view, nvals>,
                                        std::string_view>;
    using keyword_type = typename Keyword::type;

    static constexpr bool is_flag =
        std::is_same_v<bool, value_type> && (nvals == 1ul);
    static constexpr char             key      = Key;
    static constexpr std::size_t      num_vals = is_flag ? 0ul : nvals;
    static constexpr std::string_view keyword  = Keyword::view;

    Converter        converter;
    value_type       default_value;
    std::string_view doc;
    var_type         var;

    consteval Option(Converter              _converter,
                     const value_type&      _default_value,
                     const std::string_view _doc,
                     const var_type&        _var) noexcept :
        converter(_converter),
        default_value(_default_value), doc(_doc), var(_var)
    {
    }

    constexpr Binding<Converter, nvals>
    binding() const noexcept
    {
        return Binding<Converter, nvals>(converter, default_value);
    }
};

template <char Key, typename Keyword, typename Converter>
struct Option<Key, Keyword, Converter, -1ul>
{
    static constexpr std::size_t      num_vals = -1ul;
    static constexpr char             key      = Key;
    static constexpr bool             is_flag  = false;
    static constexpr std::string_view keyword  = Keyword::view;

    using value_type   = detail::result_type<Converter>;
    using keyword_type = typename Keyword::type;

    Converter        converter;
    std::string_view doc;
    std::string_view var;

    consteval Option(Converter              _converter,
                     const std::string_view _doc,
                     const std::string_view _var) noexcept :
        converter(_converter),
        doc(_doc), var(_var)
    {
    }

    constexpr Binding<Converter, -1ul>
    binding() const noexcept
    {
        return Binding<Converter, -1ul>(converter);
    }
};

template <typename Converter, std::size_t nvals>
struct Positional
{
    using var_type = std::conditional_t<(nvals > 1 && nvals != -1ul),
                                        std::array<std::string_view, nvals>,
                                        std::string_view>;
    static constexpr std::size_t num_vals = nvals;

    Converter        converter;
    std::string_view doc;
    var_type         var;

    consteval Positional(Converter&&            _converter,
                         const std::string_view _doc,
                         const var_type&        _var) noexcept :
        converter(_converter),
        doc(_doc), var(_var)
    {
    }

    constexpr auto
    binding() const noexcept
    {
        if constexpr(nvals == -1ul)
            return Binding<Converter, -1ul>(converter);
        else
            return PositionalBinding<Converter, nvals>(converter);
    }
};

template <detail::StringLiteral s>
struct Category
{
    static constexpr std::size_t num_vals = 0;
    static constexpr char        key      = detail::missing_keychar;
    static constexpr bool        is_flag  = false;

    using keyword_type = detail::missing_keyword;

    static constexpr std::string_view value = s.view();
};

template <detail::StringLiteral s>
consteval Category<s>
category() noexcept
{
    return {};
}

namespace detail
{
template <typename T>
struct is_category
{
    static constexpr bool value = false;
};

template <std::size_t N, StringLiteral<N> s>
struct is_category<Category<s>>
{
    static constexpr bool value = true;
};
} // namespace detail

template <typename T>
constexpr bool is_category = detail::is_category<T>::value;

#define CHECK_KEY                                   \
    static_assert(is_alphadigit(Key) && Key != 'h', \
                  "Only alphanumbers except 'h' (reserved) are allowed")
#define CHECK_KEYWORD                                                     \
    static_assert(s.size() > 1 && keyword_ok<s> && s.view() != "help",    \
                  "One-letter length or empty keywords are not allowed, " \
                  "('help' is reserved)")

template <char Key, detail::StringLiteral s, typename Converter>
consteval Option<Key, wrap_string<s>, Converter, 1ul>
option(Converter                             converter,
       const detail::result_type<Converter>& def_val,
       const std::string_view                doc,
       const std::string_view                var) noexcept
{
    CHECK_KEY;
    CHECK_KEYWORD;
    return {converter, def_val, doc, var};
}

template <char Key, typename Converter>
consteval Option<Key, detail::missing_keyword, Converter, 1ul>
option(Converter                             converter,
       const detail::result_type<Converter>& def_val,
       const std::string_view                doc,
       const std::string_view                var) noexcept
{
    CHECK_KEY;
    return {converter, def_val, doc, var};
}

template <detail::StringLiteral s, typename Converter>
consteval Option<detail::missing_keychar, wrap_string<s>, Converter, 1ul>
option(Converter                             converter,
       const detail::result_type<Converter>& def_val,
       const std::string_view                doc,
       const std::string_view                var) noexcept
{
    CHECK_KEYWORD;
    return {converter, def_val, doc, var};
}

template <char Key, detail::StringLiteral s, std::size_t N, typename Converter>
consteval Option<Key, wrap_string<s>, Converter, N>
option(Converter                                            converter,
       const std::array<detail::result_type<Converter>, N>& def_val,
       const std::string_view                               doc,
       const std::array<std::string_view, N>&               var) noexcept
{
    static_assert(N > 1ul && N != -1ul, "Use option without N argument");
    CHECK_KEY;
    CHECK_KEYWORD;
    return {converter, def_val, doc, var};
}

template <char Key, std::size_t N, typename Converter>
consteval Option<Key, detail::missing_keyword, Converter, N>
option(Converter                                            converter,
       const std::array<detail::result_type<Converter>, N>& def_val,
       const std::string_view                               doc,
       const std::array<std::string_view, N>&               var) noexcept
{
    static_assert(N > 1ul && N != -1ul, "Use option without N argument");
    CHECK_KEY;
    return {converter, def_val, doc, var};
}

template <detail::StringLiteral s, std::size_t N, typename Converter>
consteval Option<detail::missing_keychar, wrap_string<s>, Converter, N>
option(Converter                                            converter,
       const std::array<detail::result_type<Converter>, N>& def_val,
       const std::string_view                               doc,
       const std::array<std::string_view, N>&               var) noexcept
{
    static_assert(N > 1ul && N != -1ul, "Use option without N argument");
    CHECK_KEYWORD;
    return {converter, def_val, doc, var};
}

template <char                  Key,
          detail::StringLiteral s,
          typename Converter,
          typename... Args>
consteval Option<Key, wrap_string<s>, Converter, -1ul>
option_list(Converter              converter,
            const std::string_view doc,
            const std::string_view var) noexcept
{
    CHECK_KEY;
    CHECK_KEYWORD;
    return {converter, doc, var};
}

template <char Key, typename Converter, typename... Args>
consteval Option<Key, detail::missing_keyword, Converter, -1ul>
option_list(Converter              converter,
            const std::string_view doc,
            const std::string_view var) noexcept
{
    CHECK_KEY;
    return {converter, doc, var};
}

template <detail::StringLiteral s, typename Converter, typename... Args>
consteval Option<detail::missing_keychar, wrap_string<s>, Converter, -1ul>
option_list(Converter              converter,
            const std::string_view doc,
            const std::string_view var) noexcept
{
    CHECK_KEYWORD;
    return {converter, doc, var};
}

template <char Key, detail::StringLiteral s>
consteval Option<Key, wrap_string<s>, detail::FlagConverter, 1ul>
flag(const bool def_val, const std::string_view doc) noexcept
{
    CHECK_KEY;
    CHECK_KEYWORD;
    return {detail::FlagConverter{}, def_val, doc, ""};
}

template <char Key>
consteval Option<Key, detail::missing_keyword, detail::FlagConverter, 1ul>
flag(const bool def_val, const std::string_view doc) noexcept
{
    CHECK_KEY;
    return {detail::FlagConverter{}, def_val, doc, ""};
}

template <detail::StringLiteral s>
consteval Option<detail::missing_keychar,
                 wrap_string<s>,
                 detail::FlagConverter,
                 1ul>
flag(const bool def_val, const std::string_view doc) noexcept
{
    CHECK_KEYWORD;
    return {detail::FlagConverter{}, def_val, doc, ""};
}

#undef CHECK_KEY
#undef CHECK_KEYWORD

template <typename Converter>
consteval Positional<Converter, 1ul>
positional(Converter&&            converter,
           const std::string_view doc,
           const std::string_view var) noexcept
{
    return {converter, doc, var};
}

template <std::size_t nvals, typename Converter>
consteval Positional<Converter, nvals>
positional(Converter&&                                            converter,
           const std::string_view                                 doc,
           const typename Positional<Converter, nvals>::var_type& var) noexcept
{
    return {converter, doc, var};
}

template <typename Converter>
consteval Positional<Converter, -1ul>
positional_list(Converter&&            converter,
                const std::string_view doc,
                const std::string_view var) noexcept
{
    return {converter, doc, var};
}

template <typename Key, typename... Options>
constexpr std::size_t
tokenize(const Key key, const std::tuple<Options...>) noexcept
{
    return detail::tokenize_impl<0ul, Options...>(key);
}

template <typename Bindings, typename Options>
struct ParseResults;

template <typename OptBindings, typename PosBindings, typename Options>
struct ParseResults<std::pair<OptBindings, PosBindings>, Options>
{
    OptBindings opt_bindings;
    PosBindings pos_bindings;

    constexpr ParseResults(const std::pair<OptBindings, PosBindings>& _bindings,
                           const Options&) noexcept :
        opt_bindings(_bindings.first),
        pos_bindings(_bindings.second)
    {
    }

    template <char Key>
    constexpr decltype(auto)
    get_opt() const
    {
        return std::get<key_index<Key, Options>>(opt_bindings)();
    }

    template <detail::StringLiteral s>
    constexpr decltype(auto)
    get_opt() const
    {
        return std::get<keyword_index<string_type<s>, Options>>(opt_bindings)();
    }

    template <std::size_t Idx>
    constexpr decltype(auto)
    get_pos() const
    {
        return std::get<Idx>(pos_bindings)();
    }
};

template <typename... OptBindings, typename Options>
struct ParseResults<std::tuple<OptBindings...>, Options>
{
    std::tuple<OptBindings...> opt_bindings;

    constexpr ParseResults(const std::tuple<OptBindings...>& _bindings,
                           const Options&) noexcept :
        opt_bindings(_bindings)
    {
    }

    template <char Key>
    constexpr decltype(auto)
    get_opt() const noexcept
    {
        return std::get<key_index<Key, Options>>(opt_bindings)();
    }

    template <detail::StringLiteral s>
    constexpr decltype(auto)
    get_opt() const noexcept
    {
        return std::get<keyword_index<string_type<s>, Options>>(opt_bindings)();
    }
};

template <typename OptBindings, typename PosBindings, typename Options>
ParseResults(const std::pair<OptBindings, PosBindings>&, const Options&)
    -> ParseResults<std::pair<OptBindings, PosBindings>, Options>;

template <typename... OptBindings, typename Options>
ParseResults(const std::tuple<OptBindings...>&, const Options&)
    -> ParseResults<std::tuple<OptBindings...>, Options>;

template <typename... Bindings, typename Word>
constexpr auto
set_binding(std::tuple<Bindings...>& bindings,
            const std::size_t        token,
            Word                     word) noexcept
{
    return visit(
        token,
        [&](auto& binding) noexcept { binding.set(word); },
        bindings,
        []() noexcept {});
}

template <typename KeyType>
struct Key
{
    KeyType value;
};

struct KeywordValue
{
    std::string_view keyword;
    std::string_view value;
};

struct Value
{
    char** data;

    std::string_view
    string() const noexcept
    {
        return *data;
    }
};

struct KeyCharSeq
{
    std::string_view seq;
};

using KeyChar = Key<char>;
using Keyword = Key<std::string_view>;
using ParsedWord =
    std::variant<KeyChar, KeyCharSeq, Keyword, KeywordValue, Value>;

ParsedWord
arg_case(char** data) noexcept
{
    std::string_view word{*data};
    std::size_t      pos = 0ul;

    while(word[pos] == '-' && pos <= 2ul) ++pos;

    ParsedWord ret = Value{data};

    switch(pos)
    {
        // -
    case 1ul:
    {
        if(word.size() > 2ul)
            // -abcd
            ret = KeyCharSeq{word.substr(pos)};
        else
            // -a
            ret = KeyChar{word[pos]};
    }
    break;
        // --
    case 2ul:
    {
        const std::size_t eq_pos = word.find('=');
        if(eq_pos == std::string_view::npos)
            // --keyword
            ret = Keyword{word.substr(pos)};
        else
            // --keyword=value
            ret = KeywordValue{word.substr(pos, eq_pos - 2ul),
                               word.substr(eq_pos + 1ul)};
    }
    break;
    }

    return ret;
}

template <typename... Options>
constexpr auto
make_bindings(const std::tuple<Options...>& options) noexcept
{
    return std::apply([](const Options&... options) constexpr noexcept
                      { return std::tuple{options.binding()...}; },
                      options);
}

template <typename OptDesc, typename PosDesc>
class Visitor
{
    static constexpr std::size_t num_pos = std::tuple_size_v<PosDesc>;
    static constexpr bool        has_pos = num_pos > 0ul;
    using OptBindings =
        std::decay_t<decltype(make_bindings(std::declval<OptDesc>()))>;
    using PosBindings = std::conditional_t<
        has_pos,
        std::decay_t<decltype(make_bindings(std::declval<PosDesc>()))>,
        void>;

    detail::State<OptDesc, OptBindings> opt_state;
    detail::State<PosDesc, PosBindings> pos_state;
    bool                                is_pos = has_pos;

    template <typename... Ts>
    static constexpr std::size_t
    get_nvals(const std::size_t _token, const std::tuple<Ts...>& desc)
    {
        if constexpr(sizeof...(Ts) > 0)
            return visit(
                _token,
                []<typename Desc>(const Desc&) constexpr noexcept
                { return Desc::num_vals; },
                desc,
                []() constexpr noexcept -> std::size_t { return 0ul; });
        else
            return 0ul;
    }

    template <typename Desc>
    std::string_view
    get_keyword(std::size_t _token, const Desc& opts_desc) const noexcept
    {
        return visit(
            _token,
            []<typename Opt>(const Opt&) constexpr noexcept
            { return Opt::keyword; },
            opts_desc,
            []() constexpr noexcept -> std::string_view { return ""; });
    }

    bool
    pos_not_set() const noexcept
    {
        if constexpr(has_pos)
            return pos_state.token < num_pos;
        else
            return false;
    }

    void
    reset_opt() noexcept
    {
        opt_state.token  = -1ul;
        opt_state.remain = 0ul;
        is_pos           = pos_not_set();
    }

    template <typename Key>
    constexpr std::pair<std::size_t, std::size_t>
    key_info(const Key key)
    {
        const std::size_t new_token = tokenize(key, opt_state.desc);
        const std::size_t new_nvals = get_nvals(new_token, opt_state.desc);
        return {new_token, new_nvals};
    }

    constexpr void
    reclaim_opt_later(const std::size_t new_token,
                      const std::size_t new_nvals) noexcept
    {
        opt_state.token  = new_token;
        opt_state.remain = new_nvals;
        is_pos           = false;
    }

    constexpr void
    set_flag(const std::size_t new_token) noexcept
    {
        set_binding(opt_state.bindings, new_token, "");
        reset_opt();
    }

  public:
    constexpr Visitor(const OptDesc& opt_desc, const PosDesc& pos_desc) :
        opt_state(opt_desc, -1ul, 0ul),
        pos_state(pos_desc, 0ul, get_nvals(0ul, pos_desc))
    {
    }

    Visitor()               = delete;
    Visitor(const Visitor&) = delete;
    Visitor(Visitor&&)      = delete;
    Visitor&
    operator=(const Visitor&) = delete;

    std::string_view
    check_opt() const noexcept
    {
        if(opt_state.token != -1ul && opt_state.remain != -1ul &&
           opt_state.remain > 0ul)
            return get_keyword(opt_state.token, opt_state.desc);

        return "";
    }

    bool
    check_pos() const noexcept
    {
        return pos_not_set();
    }

    constexpr auto
    get_bindings() const
    {
        if constexpr(has_pos)
            return std::pair{opt_state.bindings, pos_state.bindings};
        else
            return opt_state.bindings;
    }

    // just "-k"
    template <typename T>
    constexpr void
    operator()(const Key<T>& key)
    {
        check_opt();

        const auto [new_token, new_nvals] = key_info(key.value);

        // is flag
        if(new_nvals == 0ul)

            set_flag(new_token);

        else

            reclaim_opt_later(new_token, new_nvals);
    }

    // --keyword=value
    constexpr void
    operator()(const KeywordValue& keyval)
    {
        check_opt();

        const auto [new_token, new_nvals] = key_info(keyval.keyword);

        if(new_nvals > 1ul) throw FewArgumentsException{keyval.keyword};

        set_binding(opt_state.bindings, new_token, keyval.value);
        reset_opt();
    }

    // just "value"
    constexpr void
    operator()(const Value& value)
    {
        auto state_update = [&](auto&       state,
                                const auto& token_check,
                                const auto& reset) constexpr
        {
            if(token_check(state.token)) throw UnexpectedValue{value.string()};

            // we send char** data here because we need
            // to have access to neigbour args
            // in multi arg options
            set_binding(state.bindings, state.token, value.data);

            state.remain -= (state.remain != -1ul);

            if(state.remain == 0ul) reset(*this);
        };

        if(is_pos)
        {
            if constexpr(has_pos)
                state_update(
                    pos_state,
                    [&](std::size_t token) constexpr noexcept
                    { return token >= num_pos; },
                    [&](Visitor&) constexpr noexcept
                    {
                        ++pos_state.token;
                        pos_state.remain =
                            get_nvals(pos_state.token, pos_state.desc);
                        is_pos = pos_not_set();
                    });
        }
        else
            state_update(
                opt_state,
                [&](std::size_t token) constexpr noexcept
                { return token == -1ul; },
                std::mem_fn(&Visitor::reset_opt));
    }

    // -abcd where "bcd" could be keys or a value
    constexpr void
    operator()(const KeyCharSeq& value)
    {
        check_opt();

        const std::string_view seq = value.seq;
        for(std::size_t i = 0ul; i < seq.size(); ++i)
        {
            const char si                     = seq[i];
            const auto [new_token, new_nvals] = key_info(si);

            if(new_token == -1ul) throw KeyException<char>{si};

            if(new_nvals == 1)
            {
                if(i == seq.size() - 1)
                {
                    // if we're at the end of the word
                    // store token to reclaim the value later
                    reclaim_opt_later(new_token, new_nvals);
                }
                else
                {
                    // otherwise we bind option's value to the tail of the
                    // string
                    set_binding(opt_state.bindings,
                                new_token,
                                seq.substr(i + 1ul));
                    break;
                }
            }
            else if(new_nvals > 1ul)
            {
                throw FewArgumentsException{
                    get_keyword(new_token, opt_state.desc)};
            }
            else
            {
                // that's just a flag
                set_flag(new_token);
            }
        }
    }
};

std::size_t
try_newline(const std::size_t line_width,
            const std::size_t margin,
            std::size_t       column,
            std::FILE*        file) noexcept
{
    if(column >= line_width)
    {
        std::fputc('\n', file);
        for(std::size_t i = 0; i < margin; ++i) std::fputc(' ', file);
        column = margin;
    }
    return column;
}

template <typename O>
std::size_t
print_var(const O&,
          const std::string_view var,
          std::size_t            column,
          std::FILE*             file) noexcept
{
    fmt::print(file, " <{}", var);
    column += 2 + var.size();

    if constexpr(O::num_vals == -1ul)
    {
        // is parameter list
        std::fputs("...", file);
        column += 3;
    }

    std::fputc('>', file);
    ++column;
    return column;
}

template <bool t, typename O, typename LB>
std::size_t
print_vars(const O&    option,
           LB&&        line_breaker,
           std::size_t column,
           std::FILE*  file) noexcept
{
    if constexpr(O::num_vals == -1ul || O::num_vals == 1)
    { // option.var is just a string_view
        column = print_var(option, option.var, column, file);
        if constexpr(t) column = line_breaker(column);
    }
    else
        // option.var is std::array<num_vals, std::string_view>
        for(std::size_t i = 0; i < O::num_vals; ++i)
        {
            column = print_var(option, option.var[i], column, file);
            if constexpr(t) column = line_breaker(column);
        }
    return column;
}

template <typename... Options, typename... Positionals>
void
show_help(std::string_view                  program_name,
          const std::tuple<Options...>&     options,
          const std::tuple<Positionals...>& positionals,
          std::FILE*                        file = stdout) noexcept
{
    constexpr std::size_t line_width = 80;
    fmt::print(file, "Usage: {}", program_name);

    const std::size_t margin1 = 7 + program_name.size();
    std::size_t       column  = margin1;

    const auto try_newline1 =
        [margin1, file](std::size_t column) constexpr noexcept
    { return try_newline(line_width, margin1, column, file); };

    // print flags key characters first, e.g. [-fLaGs]
    using flag_opts =
        typelist::filter<detail::is_flag, type_sequence<Options...>>;
    if constexpr(!typelist::empty<flag_opts>)
    {
        std::fputs(" [-", file);
        typelist::for_each(
            [file, &column]<typename O>() noexcept
            {
                std::fputc(O::key, file);
                ++column;
            },
            flag_opts{});
        std::fputc(']', file);
        column += 4;
        column = try_newline1(column);
    }

    // print options short descriptions, e.g. [-o var] or [--option var]
    lucid::for_each(
        [&]<typename O>(const O& option) constexpr noexcept
        {
            if constexpr(is_option<O>)
            {
                if constexpr(has_keychar<O>)
                {
                    fmt::print(file, " [-{}", O::key);
                    column += 4;
                }
                else if constexpr(has_keyword<O>)
                {
                    fmt::print(file, " [--{}", O::keyword);
                    column += 4 + O::keyword.size();
                }
                column = print_vars<false>(option, try_newline1, column, file);
                std::fputc(']', file);
                ++column;
                column = try_newline1(column);
            }
        },
        options);

    // print positionals short description
    lucid::for_each(
        [&](const auto& p) constexpr noexcept
        { column = print_vars<true>(p, try_newline1, column, file); },
        positionals);

    std::fputs("\n\nOptions:\n", file);

    constexpr std::size_t short_margin = 2;
    const auto shift = []<typename O>(const O& option) constexpr noexcept
    {
        std::size_t ret = 0;

        if constexpr(is_option<O> || is_flag<O>)
        {
            if constexpr(has_keychar<O>) ret += 2; // '-k'
            if constexpr(has_keyword<O>)
                ret += 2 + O::keyword.size(); // '--keyword'
            if constexpr(has_keychar<O> && has_keyword<O>) ret += 2; // ', '

            if constexpr(O::num_vals == -1ul)
                ret += 6 + option.var.size(); // ' <var...>'
            else if constexpr(O::num_vals > 1)
            {
                for(std::size_t i = 0; i < O::num_vals; ++i)
                    ret += 3 + option.var[i].size(); // ' <var1> <var2>'
            }
            else if constexpr(O::num_vals == 1)
                ret += 3 + option.var.size(); // '=<var>'
        }

        return ret;
    };
    const std::size_t doc_margin =
        fold_tuple([](const auto& o1, const auto& o2) constexpr noexcept
                   { return std::max(o1, o2); },
                   map_tuple(shift, options)) +
        short_margin + 1;

    column = 0;
    lucid::for_each(
        [file, doc_margin, &column, shift]<typename O>(const O& option) noexcept
        {
            if constexpr(is_category<O>) fmt::print(file, "{}:\n", O::value);

            if constexpr(is_option<O> || is_flag<O>)
            {
                for(std::size_t i = 0; i < short_margin; ++i)
                    std::fputc(' ', file);

                if constexpr(has_keychar<O> && !has_keyword<O>)
                    fmt::print(file, "-{}", O::key);
                else if constexpr(!has_keychar<O> && has_keyword<O>)
                    fmt::print(file, "--{}", O::keyword);
                else if constexpr(has_keychar<O> && has_keyword<O>)
                    fmt::print(file, "-{}, --{}", O::key, O::keyword);

                if constexpr(O::num_vals == -1ul)
                    fmt::print(file, " <{}...>", option.var);
                else if constexpr(O::num_vals > 1)
                {
                    for(std::size_t i = 0; i < O::num_vals; ++i)
                        fmt::print(file, " <{}>", option.var[i]);
                }
                else if constexpr(O::num_vals == 1)
                    fmt::print(file, "=<{}>", option.var);

                column += short_margin + shift(option);
                while(column++ < doc_margin) std::fputc(' ', file);

                fmt::print(file, "{}\n", option.doc);
                column = 0;
            }
        },
        options);
}

std::string_view
make_program_name(std::string_view argv0) noexcept
{
    const std::size_t slash_pos{argv0.rfind('/')};
    return slash_pos == std::string_view::npos ? argv0 :
                                                 argv0.substr(slash_pos + 1);
}

template <typename... Options, typename... Positionals, typename ErrorHandler>
constexpr auto
parse(const std::tuple<Options...>&     options,
      const std::tuple<Positionals...>& positionals,
      ArgsRange                         args,
      ErrorHandler&&                    error_handler) noexcept
{
    using keychars =
        intlist::del<detail::missing_keychar, char_sequence<Options::key...>>;
    using keywords =
        typelist::del<typename detail::missing_keyword::type,
                      type_sequence<typename Options::keyword_type...>>;
    static_assert(!intlist::has_duplicates<keychars>, "Keys must be unique");
    static_assert(!typelist::has_duplicates<keywords>, "Keys must be unique");

    const std::string_view argv0 = {*args.read()};
    args.next();

    Visitor visitor{options, positionals};

    auto update = [&visitor, only_values = false](char** data) mutable
    {
        std::string_view word{*data};

        // very dirty way to stop iteration
        if(word == "-h" || word == "--help") throw HelpRequest{};

        if(word == "--")
            only_values = true;
        else
            std::visit(visitor, only_values ? Value{data} : arg_case(data));
    };

    try
    {
        ranges::for_each(args, update);
    }
    catch(const HelpRequest&)
    {
        show_help(make_program_name(argv0), options, positionals);
        exit(0);
    }
    catch(const FewArgumentsException& ex)
    {
        error_handler(ex);
    }
    catch(const UnexpectedValue& ex)
    {
        error_handler(ex);
    }
    catch(const KeyException<char>& ex)
    {
        error_handler(ex);
    }
    catch(const KeyException<std::string_view>& ex)
    {
        error_handler(ex);
    }

    std::string_view incomplete_option = visitor.check_opt();
    if(!incomplete_option.empty()) error_handler(incomplete_option);

    if(visitor.check_pos()) error_handler(true);

    return ParseResults(visitor.get_bindings(), options);
}

template <typename... Options, typename ErrorHandler>
auto
parse(const std::tuple<Options...>& options,
      ArgsRange                     args,
      ErrorHandler&&                error_handler) noexcept
{
    return parse(options, std::tuple<>{}, args, std::move(error_handler));
}

class StandardErrorHandler
{
    static constexpr std::string_view error_head = "Argument Parsing Error";

  public:
    void
    operator()(const FewArgumentsException& ex) const noexcept
    {
        fmt::print(stderr,
                   "{}: too few arguments for parameter \"{}\"\n",
                   error_head,
                   ex.keyword);
    }

    template <typename Key>
    void
    operator()(const KeyException<Key>& ex) const noexcept
    {
        fmt::print(stderr, "{}: unknown key \"{}\"\n", error_head, ex.value);
    }

    void
    operator()(const UnexpectedValue& ex) const noexcept
    {
        fmt::print(stderr,
                   "{}: unexpected value \"{}\"\n",
                   error_head,
                   ex.value);
    }

    void
    operator()(const std::string_view problem_key) const noexcept
    {
        fmt::print(stderr,
                   "{}: too few arguments for parameter \"{}\"\n",
                   error_head,
                   problem_key);
    }

    void
    operator()(bool) const noexcept
    {
        fmt::print(stderr, "{}: too few positional arguments\n", error_head);
    }
};
} // namespace lucid::argparse
