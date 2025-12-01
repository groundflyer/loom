// -*- C++ -*-
// programs_generators.hpp
//

#pragma once

#include <utils/test.hpp>

#include <concepts>
#include <utility>

template <std::integral T, T min, T max>
struct GMIntRange
{
    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return rs.make_gen<T>(min, max);
    }
};

template <std::floating_point T>
struct GMCanon
{
    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return rs.make_gen<T>();
    }
};

template <std::floating_point T>
struct GMFloat
{
    static constexpr T l = 1e-6;
    static constexpr T h = 1e6;

    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return rs.make_gen<T>(l, h);
    }
};

template <std::integral T>
using GM_2_10 = GMIntRange<T, 2, 10>;

template <std::unsigned_integral T>
using GM_FAC = GMIntRange<T, 0, sizeof(T) == 8 ? 20 : 12>;

template <std::integral T>
using GM_Incr = GMIntRange<T,
                           std::numeric_limits<T>::lowest(),
                           std::numeric_limits<T>::max() - 1>;

template <std::unsigned_integral T>
using GM_0U = GMIntRange<T, 0, static_cast<T>(-1)>;

template <std::unsigned_integral T>
using GM_U3 = GMIntRange<T, 0, 3>;

template <std::unsigned_integral T>
using GM_FIB = GMIntRange<T, 0, 80>;

template <std::floating_point T>
inline constexpr GMCanon<T> gm_canon{};

template <std::floating_point T>
inline constexpr GMFloat<T> gm_float{};

template <std::integral T>
inline constexpr GM_2_10<T> gm_2_10{};

template <std::unsigned_integral T>
inline constexpr GM_FAC<T> gm_fac{};

template <std::unsigned_integral T>
inline constexpr GM_0U<T> gm_0u{};

template <std::unsigned_integral T>
inline constexpr GM_U3<T> gm_u3{};

template <std::floating_point T>
struct GM_J4
{
    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return [g_u3    = gm_u3<loom::uint_of_size_t<sizeof(T)>>(rs),
                g_canon = gm_canon<T>(rs)]() mutable noexcept
        { return std::pair{g_u3(), g_canon()}; };
    }
};

template <std::integral T>
struct GM_Bit_Bin
{
    using U = loom::uint_of_size_t<sizeof(T)>;

    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return [g_0u = gm_0u<U>(rs)]() mutable noexcept
        { return std::tuple{g_0u(), g_0u()}; };
    }
};

template <std::unsigned_integral T>
struct GM_Shift
{
    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return [g = gm_2_10<T>(rs)]() mutable noexcept
        { return std::tuple{g(), g()}; };
    }
};

template <std::floating_point T>
struct GM_F2
{
    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return [g_f = gm_float<T>(rs)]() mutable noexcept
        { return std::pair{g_f(), g_f()}; };
    }
};

template <std::floating_point T>
struct GM_F3
{
    auto
    operator()(loom::test::RandomState& rs) const noexcept
    {
        return [g_f = gm_float<T>(rs)]() mutable noexcept
        { return std::tuple{g_f(), g_f(), g_f()}; };
    }
};
