// -*- C++ -*-
/// @file union.hpp
/// @brief Generic union.

#pragma once

#include <utils/tmp.hpp>

#include <algorithm>
#include <cstddef>

namespace lucid
{
/// @brief Generic unsafe union.
template <typename... Ts>
    requires(... && (std::is_trivially_default_constructible_v<Ts> &&
                     std::is_trivially_copy_assignable_v<Ts> &&
                     std::is_trivially_destructible_v<Ts>))
class Union
{
    alignas(Ts...) std::byte m_data[std::max({sizeof(Ts)...})];

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
    constexpr explicit operator T() const noexcept
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
