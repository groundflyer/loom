// -*- C++ -*-
/// @file string.hpp
/// @brief Utilities for working with strings.

#pragma once

#include "math.hpp"
#include "tmp.hpp"

#include <static_math/cmath.h>

#include <optional>
#include <string_view>
#include <type_traits>

namespace lucid
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

/// @brief Convert StringLiteral into char_sequence.
template <StringLiteral s>
using sl2cs =
    typename detail::sl2cs_impl<s, std::make_index_sequence<s.size()>>::type;

/// @brief Convert char_sequence @p S to std::string_view.
template <typename S>
inline constexpr std::string_view cs2sv =
    std::string_view(intlist::array<S>, intlist::length<S>);
/// @}

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

template <std::integral T, int Base>
constexpr std::optional<T>
c2i(char c) noexcept
{
    if constexpr(Base == 16)
    {
        if(is_abcdef_lower(c)) return static_cast<T>(c - 87);
        if(is_abcdef_upper(c)) return static_cast<T>(c - 55);
    }

    if(is_digit(c)) return static_cast<T>(c - 48);

    return {};
}

template <typename T>
consteval std::pair<T, std::string_view>
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

/// @brief constexpr analogue of std::stoi
template <std::integral T = int, T Base = 10>
consteval std::optional<T>
s2i(std::string_view word) noexcept
{
    auto [sign, s] = parse_sign<T>(word);

    if(s.empty()) return {};

    if constexpr(Base == 2)
        if(s.length() > 2 && (s.starts_with("0b") || s.starts_with("0B")))
            s = s.substr(2);

    if constexpr(Base == 16)
        if(s.length() > 2 && (s.starts_with("0x") || s.starts_with("0X")))
            s = s.substr(2);

    T ret = 0;
    T exp = 0;
    for(auto it = s.crbegin(); it != s.crend(); ++it)
    {
        const auto o = c2i<T, Base>(*it);
        if(!o) return {};

        ret += o.value() * smath::pow(Base, exp);
        ++exp;
    }

    return ret * sign;
}

/// @brief Inaccurate constexpr analogue of std::stof.
template <std::floating_point T = float>
consteval std::optional<T>
s2f(std::string_view word) noexcept
{
    auto [sign, s] = parse_sign<T>(word);

    if(s.empty()) return {};

    const std::size_t sep_pos = s.find('.');
    const std::size_t ep1     = s.find('e');
    const std::size_t ep2     = s.find('E');
    const std::size_t exp_pos = std::min(ep1, ep2);

    const bool has_sep = sep_pos != std::string_view::npos;
    const bool has_exp = exp_pos != std::string_view::npos;

    std::string_view int_part =
        has_sep ? s.substr(0, sep_pos) : (has_exp ? s.substr(0, exp_pos) : s);

    std::string_view frac_part =
        has_sep ? (has_exp ? s.substr(sep_pos + 1, exp_pos - sep_pos - 1) :
                             s.substr(sep_pos + 1)) :
                  "";

    std::string_view exp_part = has_exp ? s.substr(exp_pos + 1) : "";

    T ret{};

    if(!int_part.empty())
    {
        const auto oi = s2i<long>(int_part);
        if(!oi.has_value()) return {};

        ret = static_cast<T>(oi.value());
    }

    if(!frac_part.empty())
    {
        const auto oi = s2i<long>(frac_part);
        if(!oi.has_value()) return {};

        const auto frac =
            oi.value() * smath::pow(static_cast<T>(10),
                                    -static_cast<int>(frac_part.length()));

        ret += frac;
    }

    if(!exp_part.empty())
    {
        const auto oi = s2i<long>(exp_part);
        if(!oi.has_value()) return {};

        ret *= smath::pow(static_cast<T>(10), oi.value());
    }

    return ret * sign;
}
} // namespace lucid
