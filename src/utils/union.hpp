// -*- C++ -*-
/// @file union.hpp
/// @brief Generic union.

#pragma once

#include "debug.hpp"
#include "tmp.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
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

template <typename... Ts>
    requires(std::conjunction_v<std::is_default_constructible<Ts>...> &&
             typelist::of_same_size<type_sequence<Ts...>>)
class ProxyUnion
{
    using types = type_sequence<Ts...>;

    static constexpr std::size_t data_size = typelist::max_sizeof<types>;
    std::byte*                   p_data;

    template <typename T>
    static constexpr bool has = typelist::elem<T, types>;

  public:
    ProxyUnion() = delete;
    constexpr ProxyUnion(const ProxyUnion& other) noexcept :
        p_data(other.p_data)
    {
    }
    constexpr ProxyUnion(ProxyUnion&& other) noexcept : p_data(other.p_data) {}
    constexpr ProxyUnion(std::byte* data) noexcept : p_data(data) {}

    constexpr ProxyUnion&
    operator=(const ProxyUnion& rhs) noexcept
    {
        for(std::size_t i = 0; i < data_size; ++i) p_data[i] = rhs.p_data[i];
        return *this;
    }

    template <typename T>
    constexpr ProxyUnion&
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
        return *reinterpret_cast<T*>(p_data);
    }

    template <typename T>
    constexpr const T&
    ref() const noexcept
    {
        return *reinterpret_cast<const T*>(p_data);
    }

    friend void
    swap(ProxyUnion a, ProxyUnion b) noexcept
    {
        for(std::size_t i = 0; i < data_size; ++i)
            std::swap(a.p_data[i], b.p_data[i]);
    }
};

namespace detail
{
template <typename I, typename Types>
struct useless_dep
{
    using type = typelist::to<Types, ProxyUnion>;
};

template <typename I, typename Types>
using useless_t = useless_dep<I, Types>::type;
} // namespace detail

/// @brief Unsafe union of vectors.
///
/// UnionVector holds a contigous memory segment and provides a typed view to
/// it.
///
/// Only one type can be accessed at a time as
/// the segment isn't aligned to common size.
///
/// When the integrated @p Types have the same size the operator[] is available.
///
/// The number of elements of each type in type_sequence @p Types is always
/// equal and accesible via @p count().
///
/// @tparam Types type_sequence<Ts...>.
template <typename Types, typename Allocator = std::allocator<std::byte>>
    requires(typelist::all<std::is_default_constructible, Types>)
class UnionVector
{
    template <typename T>
    static constexpr bool has = typelist::elem<T, Types>;

    static constexpr std::size_t m = typelist::max_sizeof<Types>;

    std::size_t m_count{};
    Allocator   m_alloc = Allocator();
    std::byte*  p_data  = nullptr;

    _REINIT constexpr void
    reinit() noexcept
    {
        m_count = 0;
        p_data  = nullptr;
    }

    constexpr void
    allocate() noexcept
    {
        p_data = reinterpret_cast<std::byte*>(m_alloc.allocate(size_bytes()));
    }

    constexpr void
    deallocate() noexcept
    {
        if(p_data) m_alloc.deallocate(p_data, size_bytes());
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

    // operator[] depends on useless_t so it won't be instantiated if not asked
    template <std::integral I>
    constexpr const detail::useless_t<I, Types>
    operator[](const I i) const noexcept
    {
        CHECK_INDEX(i, size());
        return {p_data + (i * m)};
    }

    template <std::integral I>
    constexpr detail::useless_t<I, Types>
    operator[](const I i) noexcept
    {
        CHECK_INDEX(i, size());
        return {p_data + (i * m)};
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
    size() const noexcept
    {
        return m_count;
    }

    constexpr std::size_t
    size_bytes() const noexcept
    {
        return m_count * m;
    }

    template <typename T>
    constexpr std::span<const T>
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

    constexpr std::span<const std::byte>
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

namespace std
{
template <typename... Ts>
void
swap(lucid::ProxyUnion<Ts...> a, lucid::ProxyUnion<Ts...> b) noexcept
{
    return swap(a, b);
}
} // namespace std
