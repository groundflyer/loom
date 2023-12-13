// -*- C++ -*-
// math.hpp
//

/// @file
/// Mathematical functions.

#pragma once

#include "functional.hpp"

#include <static_math/cmath.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace lucid
{
template <std::floating_point T>
inline constexpr T Pi_ = T{3.141592653589793};

/// @brief Test whether the value is even.
template <std::integral T>
constexpr bool
is_even(const T val) noexcept
{
    return val % 2 == 0;
}

/// @brief Return result of @f$(-1)^{a}@f$.
template <std::integral T>
constexpr int
minus_one_pow(const T a)
{
    return is_even(a) ? 1 : -1;
}

/// @brief Compute number factorial.
template <std::integral T>
constexpr T
fac(const T val) noexcept
{
    T ret{1};

    if(val != 0)
        for(T i = 1; i <= val; ++i) ret *= i;

    return ret;
}

/// @brief Compute inversion number.
///
/// Complexity @f$O(N^{2})@f$.
/// https://en.wikipedia.org/wiki/Inversion_(discrete_mathematics)
template <typename Iterable>
constexpr unsigned
inversion_number(const Iterable& input) noexcept
{
    unsigned ret = 0;

    for(auto i = input.cbegin(); i != input.cend() - 1; ++i)
        for(auto j = std::next(i); j != input.cend(); ++j)
            if(*i > *j) ++ret;

    return ret;
}

/// @brief Test whether the input contains equal elements.
///
/// Complexity @f$O(N^{2})@f$.
template <typename Iterable>
constexpr bool
has_equal(const Iterable& input) noexcept
{
    for(auto i = input.cbegin(); i != input.cend() - 1; ++i)
        for(auto j = std::next(i); j != input.cend(); ++j)
            if(*i == *j) return true;
    return false;
}

/// @brief Compute Levi-Civita symbol (signature of permutation).
///
/// https://en.wikipedia.org/wiki/Levi-Civita_symbol
template <typename Iterable>
constexpr int
sgn(const Iterable& numbers) noexcept
{
    int ret{};

    if(!has_equal(numbers)) ret = minus_one_pow(inversion_number(numbers));

    return ret;
}

/// @brief Convert radians to degrees.
template <std::floating_point T>
constexpr T
degrees(const T _radians)
{
    return _radians * T{180} / Pi_<T>;
}

/// @brief Convert degrees to radians.
template <std::floating_point T>
constexpr T
radians(const T _degrees)
{
    return _degrees * Pi_<T> / T{180};
}

/// @brief Solve quadratic equation @f$ax^{2} + bx + c@f$ for non-complex cases.
/// @return pair of bool and the smallest of two positive roots.
template <std::floating_point T>
constexpr std::pair<bool, T>
quadratic(const T a, const T b, const T c) noexcept
{
    const T D      = b * b - T{4} * a * c;
    const T factor = T{0.5} * a;
    T       sqrtD{};
    if(std::is_constant_evaluated())
        sqrtD = smath::sqrt(D);
    else
        sqrtD = std::sqrt(D);
    const T  x1 = (-b + sqrtD) * factor;
    const T  x2 = (-b - sqrtD) * factor;
    const T& x =
        std::signbit(x1) ? x2 : (std::signbit(x2) ? x1 : std::min(x1, x2));
    return std::pair{!std::signbit(D * x), x};
}

/// @brief Resample canonical random point.
template <std::floating_point T>
constexpr T
resample(const T s) noexcept
{
    const T h{0.5};
    return T{2} * (s > h ? s - h : h - s);
}

template <std::totally_ordered T>
struct range
{
    T lower;
    T upper;

    constexpr range(const T _lower, const T _upper) :
        lower(_lower), upper(_upper)
    {
    }

    constexpr bool
    operator()(const T val) const noexcept
    {
        return val >= lower && val < upper;
    }
};

template <std::totally_ordered T>
struct inclusive_range
{
    T lower;
    T upper;

    constexpr inclusive_range(const T _lower, const T _upper) :
        lower(_lower), upper(_upper)
    {
    }

    constexpr bool
    operator()(const T val) const noexcept
    {
        return val >= lower && val <= upper;
    }
};

/// @brief Rise a number to a constant power.
///
/// @tparam exp power exponent.
template <unsigned exp, typename T>
constexpr T
static_pow(const T value) noexcept
{
    if(!exp) { return 1; }
    else
    {
        const T base = static_pow<exp / 2>(value);
        if constexpr(is_even(exp))
            return base * base;
        else
            return base * base * value;
    }
}

/// @brief Perform linear interpolation between two values.
/// @param bias interpolation bias.
template <std::floating_point T, std::floating_point Bias>
constexpr T
lerp(const T a, const T b, const Bias bias)
{
    return b * bias + a * (T{1} - bias);
}

/// @brief Compare floating-point values for equality using epsilon.
///
/// The implementation is taken from
/// https://en.cppreference.com/w/cpp/types/numeric_limits/epsilon
///
/// @param ulp spacing between adjascent numbers (see
/// https://en.wikipedia.org/wiki/Unit_in_the_last_place). @anchor ulp
template <std::floating_point T, std::integral ULP>
constexpr bool
almost_equal(const T a, const T b, const ULP ulp)
{
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    // unless the result is subnormal
    T amb{a - b};
    T apb{a + b};
    if(std::is_constant_evaluated())
    {
        amb = smath::abs(amb);
        apb = smath::abs(apb);
    }
    else
    {
        amb = std::abs(amb);
        apb = std::abs(apb);
    }
    return amb <= std::numeric_limits<T>::epsilon() * apb * ulp ||
           amb < std::numeric_limits<T>::min();
}
} // namespace lucid
