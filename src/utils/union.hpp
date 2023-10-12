// -*- C++ -*-
/// @file union.hpp
/// @brief Generic union.

#pragma once

#include "debug.hpp"
#include "tmp.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <span>

namespace lucid
{
/// @brief Generic unsafe union.
template <typename... Ts>
    requires(std::conjunction_v<std::is_default_constructible<Ts>...>)
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

namespace detail
{
template <std::size_t n, typename List>
struct max_helper;

template <std::size_t n, typename... Ts>
struct max_helper<n, type_sequence<Ts...>>
{
    static constexpr std::size_t value = std::max({n, sizeof(Ts)...});
};
} // namespace detail

/// @brief Unsafe union of vectors.
///
/// UnionVector holds a contigous memory segment and provides a typed view to
/// it.
///
/// The number of elements of each type in type_sequence @p Types is always
/// equal and accesible via @p count().
///
/// @tparam Types type_sequence<Ts...>.
template <typename Types,
          typename Allocator    = std::pmr::polymorphic_allocator<std::byte>,
          std::size_t alignment = alignof(std::max_align_t)>
    requires(typelist::all<std::is_default_constructible, Types>)
class UnionVector
{
    std::size_t m_count{};
    Allocator   m_alloc = Allocator();
    std::byte*  p_data  = nullptr;

    template <typename T>
    static constexpr bool has = typelist::elem<T, Types>;

    static constexpr std::size_t m =
        detail::max_helper<alignment, Types>::value;

    _REINIT constexpr void
    reinit() noexcept
    {
        m_count = 0;
        p_data  = nullptr;
    }

    constexpr void
    allocate() noexcept
    {
        p_data = reinterpret_cast<std::byte*>(
            m_alloc.allocate_bytes(size(), alignment));
    }

    constexpr void
    deallocate() noexcept
    {
        if(p_data) m_alloc.deallocate_bytes(p_data, size(), alignment);
    }

  public:
    /// @brief Construct an empty UnionVector.
    constexpr UnionVector() noexcept = default;

    /// @brief Construct an empty UnionVector with allocator @p alloc.
    constexpr explicit UnionVector(const Allocator& alloc) noexcept :
        m_alloc(alloc)
    {
    }

    /// @brief Construct UnionVector with @p count of unitialized elements.
    constexpr UnionVector(std::size_t      count,
                          const Allocator& alloc = Allocator()) noexcept :
        m_count(count),
        m_alloc(alloc)
    {
        allocate();
    }

    /// @brief Construct UnionVector with @p count of elements initalized with
    /// @p value of type @p T.
    template <typename T>
    constexpr UnionVector(std::size_t      count,
                          const T&         value,
                          const Allocator& alloc = Allocator()) noexcept
        requires(has<T>)
        : m_count(count), m_alloc(alloc)
    {
        allocate();
        std::ranges::uninitialized_fill_n(reinterpret_cast<T*>(p_data),
                                          m_count,
                                          value);
    }

    constexpr UnionVector(const UnionVector& other) noexcept :
        m_count(other.m_count), m_alloc(other.m_alloc)
    {
        allocate();
        std::ranges::uninitialized_copy_n(other.p_data,
                                          size(),
                                          p_data,
                                          p_data + size());
    }

    constexpr UnionVector(UnionVector&& other) noexcept :
        m_count(other.m_count), m_alloc(other.m_alloc), p_data(other.p_data)
    {
        other.reinit();
    }

    constexpr UnionVector&
    operator=(const UnionVector& rhs) noexcept
    {
        deallocate();

        m_count = rhs.m_count;
        m_alloc = rhs.m_alloc;

        allocate();

        std::ranges::uninitialized_copy_n(rhs.p_data,
                                          size(),
                                          p_data,
                                          p_data + size());

        return *this;
    }

    constexpr UnionVector&
    operator=(UnionVector&& rhs) noexcept
    {
        deallocate();

        m_count = rhs.m_count;
        m_alloc = rhs.m_alloc;
        p_data  = rhs.p_data;

        rhs.reinit();

        return *this;
    }

    constexpr UnionVector&
    resize(std::size_t count) noexcept
    {
        if(count != m_count)
        {
            deallocate();
            m_count = count;
            allocate();
        }

        return *this;
    }

    constexpr std::size_t
    count() const noexcept
    {
        return count;
    }

    constexpr std::size_t
    size() const noexcept
    {
        return m_count * m;
    }

    template <typename T>
    constexpr std::span<T>
    view() const noexcept
        requires(has<T>)
    {
        return {reinterpret_cast<const T*>(p_data), m_count};
    }

    template <typename T>
    constexpr std::span<T>
    view() noexcept
        requires(has<T>)
    {
        return {reinterpret_cast<T*>(p_data), m_count};
    }

    constexpr std::span<std::byte>
    byteview() const noexcept
    {
        return {p_data, size()};
    }

    constexpr std::byte*
    data() const noexcept
    {
        return p_data;
    }

    virtual ~UnionVector() { deallocate(); }
};
} // namespace lucid
