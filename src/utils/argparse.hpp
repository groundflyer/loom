// -*- C++ -*-
/// @file argparse.hpp
/// @brief Parsing command line arguments.

#pragma once

#include <utils/debug.hpp>
#include <utils/functional.hpp>
#include <utils/string.hpp>
#include <utils/tmp.hpp>
#include <utils/tuple.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <functional>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <fmt/core.h>

#include <cstdio>

namespace lucid::argparse
{
/// @defgroup ArgParse ArgParse
/// @brief Zero-cost, Unix/GNU style command line arguments parser.
/// @{

/// @brief Special value. The exact meaning depends on the context.
///
/// In case of tokenization: no token.
///
/// In case of option number: indefenite number of options.
inline constexpr std::size_t npos = -1ul;

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
std::size_t
tokenize_impl(const char key) noexcept
{
    if(key == FirstOption::key) return Idx;

    if constexpr(sizeof...(RestOptions) > 0ul)
        return tokenize_impl<Idx + 1, RestOptions...>(key);
    else
        return npos;
}

template <std::size_t Idx, typename FirstOption, typename... RestOptions>
std::size_t
tokenize_impl(const std::string_view keyword) noexcept
{
    if(keyword == FirstOption::keyword) return Idx;

    if constexpr(sizeof...(RestOptions) > 0ul)
        return tokenize_impl<Idx + 1, RestOptions...>(keyword);
    else
        return npos;
}

template <typename CharSeq, StringLiteral s>
struct KeywordWrapper
{
    using type                             = CharSeq;
    static constexpr std::string_view view = s.view();
};

template <StringLiteral s, typename Idxs>
struct wrap_string_helper
{
    using type = KeywordWrapper<sl2cs<s>, s>;
};

template <typename Reader>
using result_type =
    std::decay_t<std::invoke_result_t<Reader, std::string_view>>;

template <typename S>
struct keyword_ok_impl;

template <char... cs>
struct keyword_ok_impl<char_sequence<cs...>>
{
    static constexpr bool value =
        (... && (is_alphadigit(cs) || cs == '-' || cs == '_'));
};
} // namespace detail

/// @brief Thrown when unknown key is encountered.
template <typename Key>
struct KeyException
{
    Key value;
};

/// @brief Thrown when reaching the end of the arguments list
/// while unset values remain.
struct FewArgumentsException
{
    std::string_view keyword;
};

/// @brief Thrown when parser encounters a value while expecting a key
/// or a keyword.
struct UnexpectedValue
{
    std::string_view value;
};

/// @brief Defines requirements for callables to convert
/// std::string_view to a value.
template <typename T>
concept Reader =
    std::is_invocable_v<T, std::string_view> &&
    !(std::is_void_v<std::invoke_result_t<T, std::string_view>> ||
      std::is_pointer_v<std::invoke_result_t<T, std::string_view>> ||
      std::is_reference_v<std::invoke_result_t<T, std::string_view>>);

/// @brief Reads a bool value.
///
/// "1", "yes", "YES", "true", "TRUE", "on", "ON" - considered to be true,
/// any other values to be false.
class FlagReader
{
    template <typename... Rest>
    static bool
    any(std::string_view w, Rest... rest) noexcept
    {
        return (... || (w == rest));
    }

  public:
    bool
    operator()(std::string_view word) const noexcept
    {
        return any(word, "1", "yes", "YES", "true", "TRUE", "on", "ON");
    }
};

/// @brief Default reader. Does nothing.
/// @return std::string_view.
struct DefaultReader
{
    constexpr std::string_view
    operator()(std::string_view v) const noexcept
    {
        return v;
    }
};

/// @brief Make a pair of StringLiteral and a char_sequence.
template <StringLiteral s>
using wrap_string =
    detail::wrap_string_helper<s, std::make_index_sequence<s.size()>>::type;

/// @brief Get the index of an Option with character key @p Key in the list of
/// @p Options
template <char Key, typename Options>
constexpr std::size_t key_index = detail::key_index<Key, Options>::value;

/// @brief Get the index of an Option with keyword @p Keyword in the list of @p
/// Options
template <typename Keyword, typename Options>
constexpr std::size_t keyword_index =
    detail::keyword_index<Keyword, Options>::value;

/// @brief Check if keyword @p s meets requirements.
template <StringLiteral s>
constexpr bool keyword_ok = detail::keyword_ok_impl<sl2cs<s>>::value;

// Empty class tag type to indicate that there is no keyword defined.
using missing_keyword = char_sequence<' '>;

// This character is used to indicate that character key is not defined.
constexpr inline char missing_keychar = ' ';

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
    static constexpr bool value = O::is_flag && has_keychar<O>::value;
};

template <typename O>
constexpr bool has_keychar_v = has_keychar<O>::value;

template <typename O>
constexpr bool has_keyword_v = has_keyword<O>::value;

template <typename O>
constexpr bool is_flag_v = is_flag<O>::value;

template <typename O>
constexpr bool is_option = !is_flag_v<O>;

class ArgsIterator : public std::random_access_iterator_tag
{
    int    m_argc = 0;
    char** p_argv = nullptr;
    int    m_pos  = 0;

  public:
    using difference_type = int;
    using value_type      = std::string_view;

    ArgsIterator(int argc, char* argv[], int pos = 0) noexcept :
        m_argc(argc), p_argv(argv), m_pos(pos)
    {
    }

    ArgsIterator() noexcept = default;

    bool
    operator==(std::default_sentinel_t) const noexcept
    {
        return m_pos == m_argc;
    }

    bool
    operator==(const ArgsIterator& rhs) const noexcept
    {
        return m_pos == rhs.m_pos;
    }

    std::strong_ordering
    operator<=>(const ArgsIterator& rhs) const noexcept
    {
        return m_pos <=> rhs.m_pos;
    }

    value_type
    operator*() const noexcept
    {
        return {*(p_argv + m_pos)};
    }

    ArgsIterator&
    operator++() noexcept
    {
        ++m_pos;
        return *this;
    }

    ArgsIterator
    operator++(int) noexcept
    {
        ArgsIterator ret(*this);
        ++m_pos;
        return ret;
    }

    ArgsIterator&
    operator--() noexcept
    {
        --m_pos;
        return *this;
    }

    ArgsIterator
    operator--(int) noexcept
    {
        ArgsIterator ret(*this);
        --m_pos;
        return ret;
    }

    ArgsIterator
    operator+(difference_type rhs) const noexcept
    {
        return {m_argc, p_argv, m_pos + rhs};
    }

    difference_type
    operator+(const ArgsIterator& rhs) const noexcept
    {
        return m_pos + rhs.m_pos;
    }

    ArgsIterator
    operator-(difference_type rhs) const noexcept
    {
        return {m_argc, p_argv, m_pos - rhs};
    }

    difference_type
    operator-(const ArgsIterator& rhs) const noexcept
    {
        return m_pos - rhs.m_pos;
    }

    ArgsIterator&
    operator+=(difference_type rhs) noexcept
    {
        m_pos += rhs;
        return *this;
    }

    ArgsIterator&
    operator-=(difference_type rhs) noexcept
    {
        m_pos -= rhs;
        return *this;
    }

    value_type
    operator[](difference_type n) const noexcept
    {
        CHECK_INDEX(n, m_argc);
        return {*(p_argv + n)};
    }

    int
    count() const noexcept
    {
        return m_argc - m_pos;
    }

    friend ArgsIterator
    operator+(difference_type lhs, const ArgsIterator& rhs) noexcept
    {
        return {rhs.m_argc, rhs.p_argv, rhs.m_pos + lhs};
    }
};

/// @brief Wraps program arguments into a range view.
class ArgsRange : public std::ranges::view_interface<ArgsRange>
{
    int    m_argc = 0;
    char** p_argv = nullptr;

  public:
    ArgsRange() = default;

    ArgsRange(int argc, char* argv[]) noexcept : m_argc(argc), p_argv(argv) {}

    ArgsIterator
    begin() const noexcept
    {
        return {m_argc, p_argv};
    }

    ArgsIterator
    end() const noexcept
    {
        return {m_argc, p_argv, m_argc};
    }

    friend ArgsIterator
    begin(const ArgsRange& r) noexcept
    {
        return {r.m_argc, r.p_argv};
    }

    friend std::default_sentinel_t
    end(const ArgsRange&) noexcept
    {
        return std::default_sentinel;
    }

    friend int
    size(const ArgsRange& r) noexcept
    {
        return r.m_argc;
    }
};

/// @brief Holds pointer to words with values for an option.
template <Reader R, std::size_t nvals>
class Binding
{
    using value_type = std::array<detail::result_type<R>, nvals>;

    R            m_reader;
    value_type   m_default_value;
    bool         m_is_set = false;
    ArgsIterator m_iter;

  public:
    Binding(R reader, const value_type& value) noexcept :
        m_reader(reader), m_default_value(value)
    {
    }

    void
    set(ArgsIterator iter) noexcept
    {
        m_iter   = iter;
        m_is_set = true;
    }

    // sink for tokenize
    void
    set(std::string_view) noexcept
    {
    }

    value_type
    operator()() const
        noexcept(std::is_nothrow_invocable_r_v<value_type, R, std::string_view>)
    {
        if(m_is_set)
        {
            value_type ret;

            std::ranges::copy(std::views::counted(m_iter, nvals) |
                                  std::views::transform(m_reader),
                              ret.begin());
            return ret;
        }
        else
            return m_default_value;
    }
};

template <>
class Binding<FlagReader, 0>
{
    static constexpr FlagReader reader{};

    bool m_flag = false;

  public:
    Binding(FlagReader, bool default_value = false) : m_flag(default_value) {}

    void
    set(std::string_view word) noexcept
    {
        m_flag = word.empty() ? m_flag ^ true : reader(word);
    }

    // sink for tokenize
    void
    set(ArgsIterator) noexcept
    {
    }

    bool
    operator()() const noexcept
    {
        return m_flag;
    }
};

template <Reader R>
class Binding<R, 1>
{
    using value_type = detail::result_type<R>;

    R                m_reader;
    value_type       m_default_value;
    std::string_view m_word;
    bool             m_is_set = false;

  public:
    Binding(R reader, const value_type& value) noexcept :
        m_reader(reader), m_default_value(value)
    {
    }

    void
    set(std::string_view word) noexcept
    {
        m_word   = word;
        m_is_set = true;
    }

    void
    set(ArgsIterator iter) noexcept
    {
        m_word   = *iter;
        m_is_set = true;
    }

    value_type
    operator()() const
        noexcept(std::is_nothrow_invocable_r_v<value_type, R, std::string_view>)
    {
        return m_is_set ? m_reader(m_word) : m_default_value;
    }
};

template <Reader R>
class Binding<R, npos>
{
    R            m_reader;
    ArgsIterator m_first;
    ArgsIterator m_last;
    bool         m_first_is_set = false;

  public:
    explicit Binding(R reader) noexcept : m_reader(reader) {}

    void
    set(ArgsIterator iter) noexcept
    {
        if(m_first_is_set)
            m_last = iter;
        else
        {
            m_first        = iter;
            m_first_is_set = true;
        }
    }

    // sink for tokenize
    void
    set(std::string_view) noexcept
    {
    }

    auto
    operator()() const
        noexcept(std::is_nothrow_invocable_v<R, std::string_view>)
    {
        return std::ranges::subrange(m_first, m_last) |
               std::views::transform(m_reader);
    }
};

template <Reader R, std::size_t nvals>
class ValuelessBinding
{
    R            m_reader;
    ArgsIterator m_iter;

  public:
    ValuelessBinding(R reader) noexcept : m_reader(reader) {}

    void
    set(ArgsIterator iter) noexcept
    {
        m_iter = iter;
    }

    // sink for tokenize
    void
    set(std::string_view) noexcept
    {
    }

    decltype(auto)
    operator()() const
        noexcept(std::is_nothrow_invocable_v<R, std::string_view>)
    {
        return std::views::counted(m_iter, nvals) |
               std::views::transform(m_reader);
    }
};

template <Reader R>
class ValuelessBinding<R, 1>
{
    using value_type = detail::result_type<R>;

    R                m_reader;
    std::string_view m_word;

  public:
    ValuelessBinding(R reader) noexcept : m_reader(reader) {}

    void
    set(std::string_view word) noexcept
    {
        m_word = word;
    }

    // sink for tokenize
    void
    set(ArgsIterator) noexcept
    {
    }

    value_type
    operator()() const
        noexcept(std::is_nothrow_invocable_r_v<value_type, R, std::string_view>)
    {
        return m_reader(m_word);
    }
};

/// @brief Describes an option with multiple fields.
template <char Key, typename Keyword, Reader R, std::size_t nvals>
struct Option
{
    using singular_value_type = detail::result_type<R>;
    using value_type =
        std::conditional_t<(nvals > 1),
                           std::array<singular_value_type, nvals>,
                           singular_value_type>;
    using var_type     = std::conditional_t<(nvals > 1),
                                        std::array<std::string_view, nvals>,
                                        std::string_view>;
    using keyword_type = Keyword;
    using binding_type = Binding<R, nvals>;

    static constexpr bool is_flag =
        std::is_same_v<bool, value_type> && (nvals == 0ul);
    static constexpr char             key      = Key;
    static constexpr std::size_t      num_vals = nvals;
    static constexpr std::string_view keyword  = cs2sv<Keyword>;

    R                reader;
    value_type       default_value;
    std::string_view doc;
    var_type         var;

    consteval Option(R                      _reader,
                     const value_type&      _default_value,
                     const std::string_view _doc,
                     const var_type&        _var) noexcept :
        reader(_reader),
        default_value(_default_value), doc(_doc), var(_var)
    {
    }

    binding_type
    binding() const noexcept
    {
        return {reader, default_value};
    }
};

template <char Key, typename Keyword, Reader R>
struct Option<Key, Keyword, R, npos>
{
    static constexpr std::size_t      num_vals = npos;
    static constexpr char             key      = Key;
    static constexpr bool             is_flag  = false;
    static constexpr std::string_view keyword  = cs2sv<Keyword>;

    using value_type   = detail::result_type<R>;
    using keyword_type = Keyword;
    using binding_type = Binding<R, npos>;

    R                reader;
    std::string_view doc;
    std::string_view var;

    consteval Option(R                      _reader,
                     const std::string_view _doc,
                     const std::string_view _var) noexcept :
        reader(_reader),
        doc(_doc), var(_var)
    {
    }

    binding_type
    binding() const noexcept
    {
        return binding_type(reader);
    }
};

/// @brief Describes a positional argument.
template <Reader R, std::size_t nvals>
struct Positional
{
    using var_type     = std::conditional_t<(nvals > 1 && nvals != npos),
                                        std::array<std::string_view, nvals>,
                                        std::string_view>;
    using binding_type = std::conditional_t<nvals == npos,
                                            Binding<R, nvals>,
                                            ValuelessBinding<R, nvals>>;
    static constexpr std::size_t num_vals = nvals;

    R                reader;
    std::string_view doc;
    var_type         var;

    consteval Positional(R                      _reader,
                         const std::string_view _doc,
                         const var_type&        _var) noexcept :
        reader(_reader),
        doc(_doc), var(_var)
    {
    }

    binding_type
    binding() const noexcept
    {
        return binding_type(reader);
    }
};

#define CHECK_KEYCHAR                               \
    static_assert(is_alphadigit(Key) && Key != 'h', \
                  "Only alphanumbers except 'h' (reserved) are allowed")
#define CHECK_KEYWORD                                                     \
    static_assert(s.size() > 1 && keyword_ok<s> && s.view() != "help",    \
                  "One-letter length or empty keywords are not allowed, " \
                  "('help' is reserved)")

/// @brief Create an option with key @p Key and keyword @p s.
/// @tparam Key ASCII character for Unix style argument setting.
/// @tparam s string for GNU style argument setting.
template <char Key, StringLiteral s, Reader R = DefaultReader>
consteval Option<Key, sl2cs<s>, R, 1ul>
option(const std::string_view        doc,
       const std::string_view        var,
       const detail::result_type<R>& def_val,
       R                             reader = DefaultReader{}) noexcept
{
    CHECK_KEYCHAR;
    CHECK_KEYWORD;
    return {reader, def_val, doc, var};
}

/// @brief Shortcut to create an option without keyword.
template <char Key, Reader R = DefaultReader>
consteval Option<Key, missing_keyword, R, 1ul>
option(const std::string_view        doc,
       const std::string_view        var,
       const detail::result_type<R>& def_val,
       R                             reader = DefaultReader{}) noexcept
{
    CHECK_KEYCHAR;
    return {reader, def_val, doc, var};
}

/// @brief Shorcut to create an option without character key.
template <StringLiteral s, Reader R = DefaultReader>
consteval Option<missing_keychar, sl2cs<s>, R, 1ul>
option(const std::string_view        doc,
       const std::string_view        var,
       const detail::result_type<R>& def_val,
       R                             reader = DefaultReader{}) noexcept
{
    CHECK_KEYWORD;
    return {reader, def_val, doc, var};
}

/// @brief Create a multi-field Option with constant size of fields.
/// @tparam Key ASCII character for Unix style argument setting.
/// @tparam s string for GNU style argument setting.
/// @tparam N the number of fields.
template <char Key, StringLiteral s, std::size_t N, Reader R = DefaultReader>
consteval Option<Key, sl2cs<s>, R, N>
option(const std::string_view                       doc,
       const std::array<std::string_view, N>&       var,
       const std::array<detail::result_type<R>, N>& def_val,
       R reader = DefaultReader{}) noexcept
{
    static_assert(N > 1ul, "Use option without N argument");
    static_assert(N != npos, "Use option_list");
    CHECK_KEYCHAR;
    CHECK_KEYWORD;
    return {reader, def_val, doc, var};
}

/// @brief Shortcut to create a multi-field, option without keyword.
template <char Key, std::size_t N, Reader R = DefaultReader>
consteval Option<Key, missing_keyword, R, N>
option(const std::string_view                       doc,
       const std::array<std::string_view, N>&       var,
       const std::array<detail::result_type<R>, N>& def_val,
       R reader = DefaultReader{}) noexcept
{
    static_assert(N > 1ul, "Use option without N argument");
    static_assert(N != npos, "Use option_list");
    CHECK_KEYCHAR;
    return {reader, def_val, doc, var};
}

/// @brief Shorcut to create a multi-field option without character key.
template <StringLiteral s, std::size_t N, Reader R = DefaultReader>
consteval Option<missing_keychar, sl2cs<s>, R, N>
option(const std::string_view                       doc,
       const std::array<std::string_view, N>&       var,
       const std::array<detail::result_type<R>, N>& def_val,
       R reader = DefaultReader{}) noexcept
{
    static_assert(N > 1ul, "Use option without N argument");
    static_assert(N != npos, "Use option_list");
    CHECK_KEYWORD;
    return {reader, def_val, doc, var};
}

/// @brief Create a list option, one that takes one-or-more values.
template <char Key, StringLiteral s, Reader R = DefaultReader>
consteval Option<Key, sl2cs<s>, R, npos>
option_list(const std::string_view doc,
            const std::string_view var,
            R                      reader = DefaultReader{}) noexcept
{
    CHECK_KEYCHAR;
    CHECK_KEYWORD;
    return {reader, doc, var};
}

/// @brief Shorcut to create a list option without keyword.
template <char Key, Reader R = DefaultReader>
consteval Option<Key, missing_keyword, R, npos>
option_list(const std::string_view doc,
            const std::string_view var,
            R                      reader = DefaultReader{}) noexcept
{
    CHECK_KEYCHAR;
    return {reader, doc, var};
}

/// @brief Shorcut to create a list option without character key.
template <StringLiteral s, Reader R = DefaultReader>
consteval Option<missing_keychar, sl2cs<s>, R, npos>
option_list(const std::string_view doc,
            const std::string_view var,
            R                      reader = DefaultReader{}) noexcept
{
    CHECK_KEYWORD;
    return {reader, doc, var};
}

/// @brief Create a flag option, i.e. one that holds a boolean value
/// thus not requiring an argument when being set.
template <char Key, StringLiteral s>
consteval Option<Key, sl2cs<s>, FlagReader, 0ul>
flag(const std::string_view doc, const bool def_val = false) noexcept
{
    CHECK_KEYCHAR;
    CHECK_KEYWORD;
    return {FlagReader{}, def_val, doc, ""};
}

/// @brief Shortcut to create a flag option without keyword.
template <char Key>
consteval Option<Key, missing_keyword, FlagReader, 0ul>
flag(const std::string_view doc, const bool def_val = false) noexcept
{
    CHECK_KEYCHAR;
    return {FlagReader{}, def_val, doc, ""};
}

/// @brief Shortcut to create a flag option without character key.
template <StringLiteral s>
consteval Option<missing_keychar, sl2cs<s>, FlagReader, 0ul>
flag(const std::string_view doc, const bool def_val = false) noexcept
{
    CHECK_KEYWORD;
    return {FlagReader{}, def_val, doc, ""};
}

#undef CHECK_KEYCHAR
#undef CHECK_KEYWORD

/// @brief Create a positional option.
template <Reader R = DefaultReader>
consteval Positional<R, 1ul>
positional(const std::string_view doc,
           const std::string_view var,
           R                      reader = DefaultReader{}) noexcept
{
    return {reader, doc, var};
}

/// @brief Create a multi-field positional option.
template <std::size_t nvals, Reader R = DefaultReader>
consteval Positional<R, nvals>
positional(const std::string_view                         doc,
           const typename Positional<R, nvals>::var_type& var,
           R reader = DefaultReader{}) noexcept
{
    return {reader, doc, var};
}

/// @brief Create a positional list option.
template <Reader R = DefaultReader>
consteval Positional<R, npos>
positional_list(const std::string_view doc,
                const std::string_view var,
                R                      reader = DefaultReader{}) noexcept
{
    return {reader, doc, var};
}

/// @brief Get a corresponding option index with a key @p key.
template <typename Key, typename... Options>
std::size_t
tokenize(const Key key, const std::tuple<Options...>) noexcept
{
    return detail::tokenize_impl<0ul, Options...>(key);
}

/// @brief Holds results of parsing.
template <typename OptBindings, typename PosBindings, typename Options>
class ParseResults
{
    OptBindings m_opt_bindings;
    PosBindings m_pos_bindings;

  public:
    ParseResults(const OptBindings& opt_bindings,
                 const PosBindings& pos_bindings,
                 Options) noexcept :
        m_opt_bindings(opt_bindings),
        m_pos_bindings(pos_bindings)
    {
    }

    template <char Key>
    decltype(auto)
    get_opt() const
        noexcept(std::is_nothrow_invocable_v<
                 std::tuple_element_t<key_index<Key, Options>, OptBindings>>)
    {
        return std::get<key_index<Key, Options>>(m_opt_bindings)();
    }

    template <StringLiteral s>
    decltype(auto)
    get_opt() const
        noexcept(std::is_nothrow_invocable_v<
                 std::tuple_element_t<keyword_index<sl2cs<s>, Options>,
                                      OptBindings>>)
    {
        return std::get<keyword_index<sl2cs<s>, Options>>(m_opt_bindings)();
    }

    template <std::size_t Idx>
    decltype(auto)
    get_pos() const noexcept(
        std::is_nothrow_invocable_v<std::tuple_element_t<Idx, PosBindings>>)
    {
        return std::get<Idx>(m_pos_bindings)();
    }
};

/// @brief Holds results of parsing when positionals are not defined.
template <typename... OptBindings, typename Options>
class ParseResults<std::tuple<OptBindings...>, std::tuple<>, Options>
{
    using OptBindings_t = std::tuple<OptBindings...>;

    OptBindings_t m_opt_bindings;

  public:
    ParseResults(const OptBindings_t& bindings, Options) noexcept :
        m_opt_bindings(bindings)
    {
    }

    template <char Key>
    decltype(auto)
    get_opt() const
        noexcept(std::is_nothrow_invocable_v<
                 std::tuple_element_t<key_index<Key, Options>, OptBindings_t>>)
    {
        return std::get<key_index<Key, Options>>(m_opt_bindings)();
    }

    template <StringLiteral s>
    decltype(auto)
    get_opt() const
        noexcept(std::is_nothrow_invocable_v<
                 std::tuple_element_t<keyword_index<sl2cs<s>, Options>,
                                      OptBindings_t>>)
    {
        return std::get<keyword_index<sl2cs<s>, Options>>(m_opt_bindings)();
    }
};

/// @brief Holds results of parsing when options are not defined.
template <typename... PosBindings>
class ParseResults<std::tuple<>, std::tuple<PosBindings...>, void>
{
    using PosBindings_t = std::tuple<PosBindings...>;

    PosBindings_t m_pos_bindings;

  public:
    ParseResults(const PosBindings_t& pos_bindings) noexcept :
        m_pos_bindings(pos_bindings)
    {
    }

    template <std::size_t Idx>
    decltype(auto)
    get_pos() const noexcept(
        std::is_nothrow_invocable_v<std::tuple_element_t<Idx, PosBindings_t>>)
    {
        return std::get<Idx>(m_pos_bindings)();
    }
};

template <typename OptBindings, typename PosBindings, typename Options>
ParseResults(const OptBindings&, const PosBindings&, const Options&)
    -> ParseResults<OptBindings, PosBindings, Options>;

template <typename OptBindings, typename Options>
ParseResults(const OptBindings&, const Options&)
    -> ParseResults<OptBindings, std::tuple<>, Options>;

template <typename PosBindings>
ParseResults(const PosBindings&)
    -> ParseResults<std::tuple<>, PosBindings, void>;

/// @brief Set @p token'th value.
template <typename... Bindings, typename Value>
void
bind_value(std::tuple<Bindings...>& bindings,
           const std::size_t        token,
           Value                    value) noexcept
{
    visit_at(
        token,
        [&](auto& binding) noexcept { binding.set(value); },
        bindings,
        sink);
}

// -------------------- Parsing --------------------

/// @brief Get number of expected values for a given token.
template <typename Options>
std::size_t
get_nvals(const std::size_t token, const Options& opts) noexcept
{
    return visit_at(
        token,
        []<typename Desc>(const Desc&) noexcept { return Desc::num_vals; },
        opts,
        []() noexcept -> std::size_t
        { return 0ul; }); // sink value doesn't matter as we don't use the
                          // function with invalid token
}

// we change state during parsing
template <typename Desc>
class State
{
    template <typename T>
    using f = T::binding_type;

    using Bindings =
        typelist::to<typelist::map<f, typelist::from<Desc>>, std::tuple>;

    std::bitset<std::tuple_size_v<Bindings>> set;

  public:
    const Desc& desc;
    Bindings    bindings;

    State(const Desc& _desc) noexcept :
        desc(_desc),
        bindings(std::apply([](const auto&... options) noexcept -> Bindings
                            { return {options.binding()...}; },
                            _desc))
    {
    }

    State()             = delete;
    State(const State&) = delete;
    State(State&&)      = delete;
    State&
    operator=(const State&) = delete;

    std::size_t
    last_token() const noexcept
    {
        const std::size_t token = set.count();
        return token >= std::tuple_size_v<Bindings> ? npos : token;
    }

    void
    set_token(const std::size_t token) noexcept
    {
        set.set(token);
    }
};

/// @brief Holds errors occured during parsing.
using ErrorState = std::optional<std::variant<FewArgumentsException,
                                              UnexpectedValue,
                                              KeyException<char>,
                                              KeyException<std::string_view>>>;

template <typename... Options>
std::size_t
tokenize_option(std::string_view              word,
                const std::tuple<Options...>& opts) noexcept
{
    if constexpr(sizeof...(Options) > 0)
    {
        if(word.size() == 2 && word.front() == '-')
            return tokenize(word[1], opts);

        if(word.size() >= 4 || word.substr(0, 2) == "--")
            return tokenize(word.substr(2), opts);
    }

    return npos;
}

template <typename Desc>
ArgsIterator
get_last(ArgsIterator iter, const Desc& desc) noexcept
{
    do ++iter;
    while(!(iter == std::default_sentinel || *iter == "--" ||
            tokenize_option(*iter, desc) != npos));
    return iter;
}

template <typename Opt, typename Options>
ArgsIterator
list_bind(State<Opt>&    state,
          const Options& opts_desc,
          std::size_t    token,
          ArgsIterator   iter) noexcept
{
    ArgsIterator last = get_last(iter, opts_desc);
    bind_value(state.bindings, token, iter);
    bind_value(state.bindings, token, last);
    return last;
}

/// @brief Matches "--keyword=value".
struct KeywordValueParser
{
    template <typename Opt, typename Pos>
    bool
    operator()(ArgsIterator& iter,
               State<Opt>&   opt_state,
               State<Pos>& /* pos_state */,
               ErrorState& error) const noexcept
    {
        const std::string_view word = *iter;
        if(word.size() < 4) return false;
        if(word.substr(0, 2) != "--") return false;
        const std::size_t eq_pos = word.find('=');
        if(eq_pos == std::string_view::npos) return false;

        const std::string_view sub = word.substr(eq_pos);
        const std::size_t      token =
            tokenize(word.substr(2, eq_pos - 2), opt_state.desc);

        if(token == npos)
        {
            error = KeyException<std::string_view>{sub};
            return true;
        }

        const std::size_t nvals = get_nvals(token, opt_state.desc);
        if(nvals > 1)
        {
            error = FewArgumentsException{sub};
            return true;
        }

        bind_value(opt_state.bindings, token, word.substr(eq_pos + 1));
        ++iter;
        return true;
    }
};

template <typename Opt, typename Desc>
ArgsIterator
consume_values(ArgsIterator iter,
               State<Opt>&  state,
               const Desc&  desc,
               ErrorState&  error,
               std::size_t  token,
               std::size_t  nvals) noexcept
{
    const std::string_view word = *iter;

    auto fail = [&]() noexcept
    {
        error = FewArgumentsException{word};
        return iter;
    };

    ++iter;
    switch(nvals)
    {
    // flag
    case 0:
    {
        bind_value(state.bindings, token, "");
        return iter;
    }
    // value
    case 1:
    {
        if(iter.count() < 1) return fail();

        bind_value(state.bindings, token, *iter++);
        return iter;
    }
    // arbitrary number of values
    case npos:
    {
        if(iter.count() < 1) return fail();

        return list_bind(state, desc, token, iter);
    }
    // constant number of arguments
    default:
    {
        if(iter.count() < static_cast<int>(nvals)) return fail();

        bind_value(state.bindings, token, iter);
        return iter + nvals;
    }
    }
}

/// @brief Matches "-k [value...] or --keyword [value...]".
struct KeyParser
{
    template <typename Opt, typename Pos>
    bool
    operator()(ArgsIterator& iter,
               State<Opt>&   opt_state,
               State<Pos>& /* pos_state */,
               ErrorState& error) const noexcept
    {
        const std::string_view word  = *iter;
        const std::size_t      token = tokenize_option(word, opt_state.desc);

        // doesn't match any keys
        if(token == npos) return false;

        const std::size_t nvals = get_nvals(token, opt_state.desc);

        iter = consume_values(iter,
                              opt_state,
                              opt_state.desc,
                              error,
                              token,
                              nvals);
        return true;
    }
};

/// @brief Matches "-abcd [values...]"
struct KeyCharSeqParser
{
    template <typename Opt, typename Pos>
    bool
    operator()(ArgsIterator& iter,
               State<Opt>&   opt_state,
               State<Pos>& /* pos_state */,
               ErrorState& error) const noexcept
    {
        const std::string_view word = *iter;
        if(word.size() < 3 || word.front() != '-' || word.substr(0, 2) == "--")
            return false;

        for(std::size_t i = 1; i < word.size(); ++i)
        {
            const std::size_t token = tokenize(word[i], opt_state.desc);

            if(token == npos)
            {
                error = KeyException<char>{word[i]};
                return true;
            }

            const std::size_t nvals = get_nvals(token, opt_state.desc);
            if(nvals == 0) // flag
                bind_value(opt_state.bindings, token, "");

            if(nvals == 1)
            {
                if(word.size() - i > 1) // not at the end
                {
                    bind_value(opt_state.bindings, token, word.substr(i + 1));
                    ++iter;
                }
                else
                    iter = consume_values(iter,
                                          opt_state,
                                          opt_state.desc,
                                          error,
                                          token,
                                          nvals);

                return true;
            }

            if(nvals > 1)
            {
                if(word.size() - i > 1)
                    error = FewArgumentsException{word};
                else
                    iter = consume_values(iter,
                                          opt_state,
                                          opt_state.desc,
                                          error,
                                          token,
                                          nvals);

                return true;
            }
        }

        ++iter;
        return true;
    }
};

/// @brief Matches any remained arguments.
struct PositionalsParser
{
    template <typename Opt, typename Pos>
    bool
    operator()(ArgsIterator& iter,
               State<Opt>&   opt_state,
               State<Pos>&   pos_state,
               ErrorState&   error) const noexcept
    {
        const std::size_t token = pos_state.last_token();

        auto fail = [&]() noexcept
        {
            error = FewArgumentsException{};
            return false;
        };

        // no unset positionals left
        if(token == npos)
        {
            error = UnexpectedValue{*iter};
            return false;
        }

        const std::size_t nval = get_nvals(token, pos_state.desc);

        // arbitrary number of arguments
        switch(nval)
        {
        case npos:
        {
            if(iter.count() < 1) return fail();

            iter = list_bind(pos_state, opt_state.desc, token, iter);

            pos_state.set_token(token);
            return true;
        }
        case 1:
        {
            if(iter.count() < 1) return fail();
            bind_value(pos_state.bindings, token, *iter++);
            pos_state.set_token(token);
            return true;
        }
        default:
            // constant number of arguments
            if(iter.count() < static_cast<int>(nval)) return fail();

            bind_value(pos_state.bindings, token, iter);
            iter += nval;
            pos_state.set_token(token);
            return true;
        }
    }
};

// -------------------- Printing help --------------------

/// @brief Attempt to put a newline if the desired line width reached.
/// @return current column.
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

/// @brief Prints @p var in angle brackets.
/// @return current column.
template <typename O>
std::size_t
print_var(const O&,
          const std::string_view var,
          std::size_t            column,
          std::FILE*             file) noexcept
{
    fmt::print(file, " <{}", var);
    column += 2 + var.size();

    if constexpr(O::num_vals == npos)
    {
        // is parameter list
        std::fputs("...", file);
        column += 3;
    }

    std::fputc('>', file);
    ++column;
    return column;
}

/// @brief Print one or more variables in angle brackets.
/// @return current column.
template <bool t, typename O, typename LB>
std::size_t
print_vars(const O&    option,
           LB&&        line_breaker,
           std::size_t column,
           std::FILE*  file) noexcept
{
    if constexpr(O::num_vals == npos || O::num_vals == 1)
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

/// @brief Prints formatted help message to @p file.
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

    const auto try_newline1 = [margin1, file](std::size_t column) noexcept
    { return try_newline(line_width, margin1, column, file); };

    // print flags key characters first, e.g. [-fLaGs]
    using flag_opts = typelist::filter<is_flag, type_sequence<Options...>>;
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
        [&]<typename O>(const O& option) noexcept
        {
            if constexpr(is_option<O>)
            {
                if constexpr(has_keychar_v<O>)
                {
                    fmt::print(file, " [-{}", O::key);
                    column += 4;
                }
                else if constexpr(has_keyword_v<O>)
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
        [&](const auto& p) noexcept
        { column = print_vars<true>(p, try_newline1, column, file); },
        positionals);

    std::fputs("\n\nOptions:\n", file);

    constexpr std::size_t short_margin = 2;
    const auto            shift = []<typename O>(const O& option) noexcept
    {
        std::size_t ret = 0;

        if constexpr(is_option<O> || is_flag_v<O>)
        {
            ret += 2; // '-k'
            if constexpr(has_keyword_v<O>)
                ret += 4 + O::keyword.size(); // ', --keyword'

            if constexpr(O::num_vals == npos)
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
        fold_tuple([](const auto& o1, const auto& o2) noexcept
                   { return std::max(o1, o2); },
                   map_tuple(shift, options)) +
        short_margin + 1;

    const auto doc_align = [&]() noexcept
    {
        while(column++ < doc_margin - 1) std::fputc(' ', file);
    };

    const auto doc_print = [&](const std::string_view doc) noexcept
    {
        doc_align();

        std::ranges::for_each(
            doc | std::views::split(' ') |
                std::views::transform(
                    [](auto&& rng) {
                        return std::string_view(&*rng.begin(),
                                                std::ranges::distance(rng));
                    }) |
                std::views::filter([](std::string_view w) noexcept
                                   { return !w.empty(); }),
            [&column, doc_align, file](std::string_view w) noexcept
            {
                if(column + w.size() <= line_width)
                {
                    fmt::print(file, " {}", w);
                    column += 1 + w.size();
                }
                else
                {
                    std::fputc('\n', file);
                    column = 0;
                    doc_align();
                    fmt::print(file, " {}", w);
                    column += 1 + w.size();
                }
            });

        std::fputc('\n', file);
        column = 0;
    };

    const auto opt_align = [&column, file]() noexcept
    {
        for(std::size_t i = 0; i < short_margin; ++i) std::fputc(' ', file);
        column += short_margin;
    };

    column = 0;
    lucid::for_each(
        [&]<typename O>(const O& option) noexcept
        {
            opt_align();

            if constexpr(has_keychar_v<O> && !has_keyword_v<O>)
                fmt::print(file, "-{}", O::key);
            else if constexpr(!has_keychar_v<O> && has_keyword_v<O>)
                fmt::print(file, "    --{}", O::keyword);
            else if constexpr(has_keychar_v<O> && has_keyword_v<O>)
                fmt::print(file, "-{}, --{}", O::key, O::keyword);

            if constexpr(O::num_vals == npos)
                fmt::print(file, " <{}...>", option.var);
            else if constexpr(O::num_vals > 1)
            {
                for(std::size_t i = 0; i < O::num_vals; ++i)
                    fmt::print(file, " <{}>", option.var[i]);
            }
            else if constexpr(O::num_vals == 1)
            {
                if constexpr(has_keyword_v<O>)
                    fmt::print(file, "=<{}>", option.var);
                else
                    fmt::print(file, " <{}>", option.var);
            }

            column += shift(option);
            doc_print(option.doc);
        },
        options);

    if constexpr(sizeof...(Positionals) > 0)
    {
        std::fputs("\nRequired arguments:\n", file);

        lucid::for_each(
            [&](const auto& o) noexcept
            {
                opt_align();
                column = print_vars<false>(
                    o,
                    []() {},
                    column,
                    file);
                doc_print(o.doc);
            },
            positionals);
    }
}

/// @brief Beautify program name from argv[0].
std::string_view
make_program_name(std::string_view argv0) noexcept
{
    const std::size_t slash_pos{argv0.rfind('/')};
    return slash_pos == std::string_view::npos ? argv0 :
                                                 argv0.substr(slash_pos + 1);
}

/// @brief Prints parse errors to stderr and terminates program with exit
/// code 1.
class StandardErrorHandler
{
    static constexpr std::string_view error_head = "Argument Parsing Error";

  public:
    void
    operator()(const FewArgumentsException& ex) const noexcept
    {
        if(ex.keyword.empty())
            fmt::print(stderr, "{}: too few required arguments\n", error_head);
        else
            fmt::print(stderr,
                       "{}: too few arguments for parameter \"{}\"\n",
                       error_head,
                       ex.keyword);
        exit(1);
    }

    template <typename Key>
    void
    operator()(const KeyException<Key>& ex) const noexcept
    {
        fmt::print(stderr, "{}: unknown key \"{}\"\n", error_head, ex.value);
        exit(1);
    }

    void
    operator()(const UnexpectedValue& ex) const noexcept
    {
        fmt::print(stderr,
                   "{}: unexpected value \"{}\"\n",
                   error_head,
                   ex.value);
        exit(1);
    }
};

template <typename Options, typename Positionals>
void
check_help(const Options&     options,
           const Positionals& positionals,
           std::string_view   word,
           std::string_view   argv0) noexcept
{
    if(word == "-h" || word == "--help")
    {
        show_help(make_program_name(argv0), options, positionals);
        exit(0);
    }
}

bool
parsing_condition(bool r, ArgsIterator iter, const ErrorState& error) noexcept
{
    return r && iter != std::default_sentinel && !error.has_value();
}

template <typename Options, typename Positionals>
class MainParser
{
    using Parsers = type_sequence<KeywordValueParser,
                                  KeyCharSeqParser,
                                  KeyParser,
                                  PositionalsParser>;

    static constexpr Parsers           ps{};
    static constexpr PositionalsParser pp{};

  public:
    template <typename ErrorHandler>
    auto
    operator()(const Options&     options,
               const Positionals& positionals,
               ArgsRange          args,
               ErrorHandler&&     error_handler) const noexcept
    {
        State      opt_state{options};
        State      pos_state{positionals};
        ErrorState error;
        bool       r        = true;
        bool       pos_only = false;
        auto       iter     = std::next(std::ranges::begin(args));
        while(parsing_condition(r, iter, error))
        {
            const std::string_view word = *iter;

            if(word == "--")
            {
                pos_only = true;
                ++iter;
                continue;
            }

            check_help(options, positionals, word, *std::ranges::begin(args));

            if(pos_only)
                r = pp(iter, opt_state, pos_state, error);
            else
                r = typelist::first_of(ps, iter, opt_state, pos_state, error);
        }

#if 0
        if(!error.has_value() &&
           pos_state.last_token() != std::tuple_size_v<Positionals>)
            error = FewArgumentsException{};
#endif

        if(error.has_value())
            std::visit(std::forward<ErrorHandler>(error_handler),
                       error.value());

        return ParseResults(opt_state.bindings, pos_state.bindings, options);
    }
};

template <typename... Options>
class MainParser<std::tuple<Options...>, std::tuple<>>
{
    using Parsers =
        type_sequence<KeywordValueParser, KeyCharSeqParser, KeyParser>;

    static constexpr Parsers ps{};

  public:
    template <typename ErrorHandler>
    auto
    operator()(const std::tuple<Options...>& options,
               std::tuple<>                  positionals,
               ArgsRange                     args,
               ErrorHandler&&                error_handler) const noexcept
    {
        State      opt_state{options};
        State      pos_state{positionals};
        ErrorState error;
        bool       r    = true;
        auto       iter = std::next(std::ranges::begin(args));
        while(parsing_condition(r, iter, error))
        {
            const std::string_view word = *iter;

            check_help(options, positionals, word, *std::ranges::begin(args));
            r = typelist::first_of(ps, iter, opt_state, pos_state, error);
        }

        if(error.has_value())
            std::visit(std::forward<ErrorHandler>(error_handler),
                       error.value());

        return ParseResults(opt_state.bindings, options);
    }
};

template <typename... Positionals>
class MainParser<std::tuple<>, std::tuple<Positionals...>>
{
    static constexpr PositionalsParser pp{};

  public:
    template <typename ErrorHandler>
    auto
    operator()(std::tuple<>                      options,
               const std::tuple<Positionals...>& positionals,
               ArgsRange                         args,
               ErrorHandler&&                    error_handler) const noexcept
    {
        State      opt_state{options};
        State      pos_state{positionals};
        ErrorState error;
        bool       r    = true;
        auto       iter = std::next(std::ranges::begin(args));
        while(parsing_condition(r, iter, error))
        {
            const std::string_view word = *iter;

            check_help(options, positionals, word, *std::ranges::begin(args));

            r = pp(iter, opt_state, pos_state, error);
        }

        if(!error.has_value() &&
           pos_state.last_token() != sizeof...(Positionals))
            error = FewArgumentsException{};

        if(error.has_value())
            std::visit(std::forward<ErrorHandler>(error_handler),
                       error.value());

        return ParseResults(pos_state.bindings);
    }
};

/// @brief Parse command line arguments.
template <typename... Options,
          typename... Positionals,
          typename ErrorHandler = StandardErrorHandler>
auto
parse(const std::tuple<Options...>&     options,
      const std::tuple<Positionals...>& positionals,
      ArgsRange                         args,
      ErrorHandler&&                    error_handler = ErrorHandler{}) noexcept
{
    using keychars =
        intlist::purge<missing_keychar, char_sequence<Options::key...>>;
    using keywords =
        typelist::purge<missing_keyword,
                        type_sequence<typename Options::keyword_type...>>;
    static_assert(!intlist::has_duplicates<keychars>, "Keys must be unique");
    static_assert(!typelist::has_duplicates<keywords>, "Keys must be unique");

    using parser =
        MainParser<std::tuple<Options...>, std::tuple<Positionals...>>;

    constexpr parser p{};
    return p(options,
             positionals,
             args,
             std::forward<ErrorHandler>(error_handler));
}

/// @brief Parse command line arguments when no positional arguments required.
template <typename... Options, typename ErrorHandler = StandardErrorHandler>
auto
parse(const std::tuple<Options...>& options,
      ArgsRange                     args,
      ErrorHandler&& error_handler = StandardErrorHandler{}) noexcept
{
    return parse(options, std::tuple<>{}, args, std::move(error_handler));
}
/// @}
} // namespace lucid::argparse
