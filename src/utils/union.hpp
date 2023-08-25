// -*- C++ -*-
/// @file union.hpp
/// @brief Generic union.

#pragma once

#include <utils/tmp.hpp>

namespace lucid
{
/// @brief Generic unsafe union.
template <typename... Ts>
    requires(... && (std::is_trivially_default_constructible_v<Ts> &&
                     std::is_trivially_copy_assignable_v<Ts> &&
                     std::is_trivially_destructible_v<Ts>))
class Union
{
    using Data = std::aligned_union_t<0, Ts...>;

    Data m_data;

    template <typename T>
    static constexpr bool has = typelist::elem<T, type_sequence<Ts...>>;

  public:
    template <typename T>
    constexpr Union&
    operator=(const T& rhs) noexcept
        requires(has<T>)
    {
        ref<T>() = rhs;
        return *this;
    }

    template <typename T>
    constexpr explicit operator T() noexcept
        requires(has<T>)
    {
        return ref<T>();
    }

    template <typename T>
    constexpr T&
    ref() noexcept
    {
        return reinterpret_cast<T&>(m_data);
    }

    template <typename T>
    constexpr const T&
    ref() const noexcept
    {
        return reinterpret_cast<const T&>(m_data);
    }
};
} // namespace lucid
