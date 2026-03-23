// -*- C++ -*-
/// @file string.hpp
/// @brief Utilities for working with strings.

#pragma once

#include "functional.hpp"
#include "math.hpp"
#include "tmp.hpp"

#include <static_math/cmath.h>

#include <fast_float/fast_float.h>

#include <cerrno>
#include <optional>
#include <string_view>
#include <type_traits>

namespace loom
{
/// @defgroup CTString Compile-time string
/// @brief Converting string literals into char_sequence at compile time.
/// @ingroup TMP
/// @{

/// @brief Wraps char[@p N].
template <std::size_t _N>
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

/// @brief Compile-time string.
template <char... Cs>
using char_sequence = std::integer_sequence<char, Cs...>;

namespace detail
{
template <StringLiteral s, typename Idxs>
struct sl2cs_impl;

template <StringLiteral s, std::size_t... Idxs>
struct sl2cs_impl<s, std::index_sequence<Idxs...>>
{
    using type = char_sequence<s[Idxs]...>;
};
} // namespace detail

/// @brief Constraint of char_sequence.
template <typename T>
concept CharSequence =
    intlist::List<T> && std::same_as<typename T::value_type, char>;

/// @brief Convert StringLiteral into char_sequence.
template <StringLiteral s>
using sl2cs =
    typename detail::sl2cs_impl<s, std::make_index_sequence<s.size()>>::type;

/// @brief Convert char_sequence @p S to std::string_view.
template <CharSequence S>
inline constexpr std::string_view cs2sv =
    std::string_view(intlist::array<S>, intlist::length<S>);
/// @}

struct ReadException
{
    std::errc        ec;
    std::string_view sv;
};

namespace fn
{
constexpr bool
is_alpha_upper(char c) noexcept
{
    return inclusive_range('A', 'Z')(c);
}
constexpr bool
is_alpha_lower(char c) noexcept
{
    return inclusive_range('a', 'z')(c);
}
constexpr bool
is_alpha(char c) noexcept
{
    return is_alpha_lower(c) || is_alpha_upper(c);
}
constexpr bool
is_digit(char c) noexcept
{
    return inclusive_range('0', '9')(c);
}
constexpr bool
is_alphadigit(char c) noexcept
{
    return is_alpha(c) || is_digit(c);
}

constexpr char
lower(char c) noexcept
{
    if(is_alpha_upper(c)) return c + 32;

    return c;
}

constexpr char
upper(char c) noexcept
{
    if(is_alpha_lower(c)) return c - 32;

    return c;
}

constexpr bool
is_abcdef_upper(char c) noexcept
{
    return inclusive_range('A', 'F')(c);
}

constexpr bool
is_abcdef_lower(char c) noexcept
{
    return inclusive_range('a', 'f')(c);
}

constexpr bool
is_abcdef(char c) noexcept
{
    return is_abcdef_upper(c) || is_abcdef_lower(c);
}

template <typename... Rest>
constexpr bool
any_string(std::string_view w, Rest... rest) noexcept
{
    return (... || (w == rest));
}

/// @brief Converts character into the corresponding integer number.
///
/// @param c character to convert. Valid set is [0-9,a-f,A-F] for base 16, [0-9]
/// for 10 and (0, 1) for 2.
/// @param the number base. Valid values are 16, 10, 2.
template <std::integral Int = int>
constexpr std::optional<Int>
c2i(char c, Int base = 10) noexcept
{
    if(base == 16)
    {
        if(is_abcdef_lower(c)) return static_cast<Int>(c - 87);
        if(is_abcdef_upper(c)) return static_cast<Int>(c - 55);
    }

    if(is_digit(c))
    {
        const Int num = static_cast<Int>(c - 48);
        if(num < base) return num;
    }

    return {};
}

template <typename T>
    requires(std::signed_integral<T> || std::floating_point<T>)
constexpr std::pair<T, std::string_view>
parse_sign(std::string_view s) noexcept
{
    T sign = static_cast<T>(1);

    if(s.starts_with('-') || s.starts_with('+'))
    {
        if(s.starts_with('-')) sign *= static_cast<T>(-1);
        s = s.substr(1);
    }

    return {sign, s};
}

template <std::integral Int>
constexpr std::optional<Int>
parse_num(std::string_view s, Int base) noexcept
{
    if(s.empty()) return {};

    Int ret = 0;
    Int exp = 0;
    for(auto it = s.crbegin(); it != s.crend(); ++it)
    {
        const auto o = c2i<Int>(*it, base);
        if(!o) return {};

        ret += o.value() * smath::pow(base, exp);
        ++exp;
    }

    return ret;
}

template <std::signed_integral Int>
constexpr std::optional<Int>
parse_signed(std::string_view s, Int base) noexcept
{
    auto [sign, rest] = parse_sign<Int>(s);

    if(rest.empty()) return {};

    std::optional<Int> num = parse_num<Int>(rest, base);
    if(num) return num.value() * sign;

    return {};
}

constexpr std::pair<unsigned, std::string_view>
detect_base(std::string_view s) noexcept
{
    if(any_string(s.substr(0, 2), "0x", "0X"))
        return {16, s.substr(2)};
    else if(any_string(s.substr(0, 2), "0b", "0B"))
        return {2, s.substr(2)};

    return {10, s};
}

/// @brief constexpr analogue of std::stoi
template <std::integral Int = int>
struct s2i_fn
{
    constexpr std::optional<Int>
    operator()(std::string_view s, unsigned base = 10) const noexcept
    {
        if constexpr(std::signed_integral<Int>)
            return parse_signed<Int>(s, base);
        else
            return parse_num<Int>(s, base);
    }

    COMPOSE_OP
};

/// @brief For strings that could have prefixes like "0x" or "0b".
template <std::integral Int = int>
struct s2i_base_prefix_fn
{
    constexpr std::optional<Int>
    operator()(std::string_view s) const noexcept
    {
        if(s.empty()) return {};

        if constexpr(std::signed_integral<Int>)
        {
            auto [sign, rest]            = parse_sign<Int>(s);
            const auto [base, rest2]     = detect_base(rest);
            const std::optional<Int> num = parse_num<Int>(rest2, base);
            if(num) return num.value() * sign;
        }
        else
        {
            const auto [base, rest] = detect_base(s);
            return parse_num<Int>(rest, base);
        }

        return {};
    }

    COMPOSE_OP
};

/// @brief Inaccurate constexpr analogue of std::stof.
template <std::floating_point T = float>
struct s2f_fn
{
    constexpr std::optional<T>
    operator()(std::string_view word) const noexcept
    {
        auto [sign, s] = parse_sign<T>(word);

        if(s.empty()) return {};

        const std::size_t sep_pos = s.find('.');
        const std::size_t ep1     = s.find('e');
        const std::size_t ep2     = s.find('E');
        const std::size_t exp_pos = std::min(ep1, ep2);

        const bool has_sep = sep_pos != std::string_view::npos;
        const bool has_exp = exp_pos != std::string_view::npos;

        std::string_view int_part = has_sep ?
                                        s.substr(0, sep_pos) :
                                        (has_exp ? s.substr(0, exp_pos) : s);

        std::string_view frac_part =
            has_sep ? (has_exp ? s.substr(sep_pos + 1, exp_pos - sep_pos - 1) :
                                 s.substr(sep_pos + 1)) :
                      "";

        std::string_view exp_part = has_exp ? s.substr(exp_pos + 1) : "";

        T ret{};

        if(!int_part.empty())
        {
            const auto oi = parse_num<long>(int_part, 10);
            if(!oi.has_value()) return {};

            ret = static_cast<T>(oi.value());
        }

        if(!frac_part.empty())
        {
            const auto oi = parse_num<long>(frac_part, 10);
            if(!oi.has_value()) return {};

            const auto frac =
                oi.value() * smath::pow(static_cast<T>(10),
                                        -static_cast<int>(frac_part.length()));

            ret += frac;
        }

        if(!exp_part.empty())
        {
            const auto oi = parse_signed<long>(exp_part, 10);
            if(!oi.has_value()) return {};

            ret *= smath::pow(static_cast<T>(10), oi.value());
        }

        return ret * sign;
    }

    COMPOSE_OP
};

template <typename T, bool throws>
struct read_number_fn;

template <std::integral T, bool throws>
struct read_number_fn<T, throws>
{
    constexpr std::conditional_t<throws, T, std::optional<T>>
    operator()(std::string_view s, int base = 10) const noexcept(!throws)
    {
        T    ret;
        auto pr =
            fast_float::from_chars(s.data(), s.data() + s.size(), ret, base);

        if(pr.ec == std::errc{})
            return ret;
        else if constexpr(throws)
            throw ReadException{pr.ec, s};
        else
            return std::nullopt;
    }
};

template <std::floating_point T, bool throws>
struct read_number_fn<T, throws>
{
    constexpr std::conditional_t<throws, T, std::optional<T>>
    operator()(std::string_view         s,
               fast_float::chars_format fmt =
                   fast_float::chars_format::general) const noexcept(!throws)
    {
        T    ret;
        auto pr =
            fast_float::from_chars(s.data(), s.data() + s.size(), ret, fmt);

        if(pr.ec == std::errc{})
            return ret;
        else if constexpr(throws)
            throw ReadException{pr.ec, s};
        else
            return std::nullopt;
    }
};
} // namespace fn

namespace detail
{
template <typename T>
struct select_parser;

template <std::floating_point T>
struct select_parser<T> : std::type_identity<fn::s2f_fn<T>>
{
};

template <std::integral T>
struct select_parser<T> : std::type_identity<fn::s2i_base_prefix_fn<T>>
{
};
} // namespace detail

MK_NIEBLOID(is_alpha_upper);
MK_NIEBLOID(is_alpha_lower);
MK_NIEBLOID(is_alpha);
MK_NIEBLOID(is_digit);
MK_NIEBLOID(is_alphadigit);
MK_NIEBLOID(upper);
MK_NIEBLOID(lower);
MK_NIEBLOID(any_string);

// std::from_chars is not constexpr until C++23

/// @brief Constexpr read integer value.
template <std::integral Int = int>
constexpr inline fn::s2i_fn<Int> s2i{};

/// @brief Constexpr read integer value detecting base from prefix.
template <std::integral Int = int>
constexpr inline fn::s2i_base_prefix_fn<Int> s2i_base_prefix{};

/// @brief Constexpr read floating point value.
template <std::floating_point T = float>
constexpr inline fn::s2f_fn<T> s2f{};

/// @brief Constexpr read integer or floating point number.
template <typename T>
constexpr inline typename detail::select_parser<T>::type s2n{};

/// @brief Read number.
/// @tparam throws if set, throws loom::ReadException.
/// @return T if @p throws, otherwise std::optional<T>.
template <typename T, bool throws = false>
constexpr inline fn::read_number_fn<T, throws> read_number{};
} // namespace loom
