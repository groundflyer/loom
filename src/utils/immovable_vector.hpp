// -*- C++ -*-
/// @file immovable_vector.hpp
/// @brief vector for non copy/move-constructible objects.

#pragma once

#include "common.hpp"
#include "debug.hpp"

#include <iterator>
#include <memory>

namespace loom
{
/// @brief Dynamic size container for non copy/move-constructible objects.
///
/// ImmovableVector is capable of holding objects that aren't
/// default-constructible, copy-constructible, move-constructible,
/// copy-assignable, move-assignable. As a result it is not resizeable, copyable
/// or moveable itself.
///
/// Containing objects are constructed and initialized in-place only once, in
/// the constructor.
template <typename T, typename Allocator = std::allocator<T>>
class ImmovableVector
{
    using alloc_traits = std::allocator_traits<Allocator>;

    Allocator   m_alloc;
    std::size_t m_size;
    T*          p_data;

  public:
    using value_type             = T;
    using allocator_type         = Allocator;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = T&;
    using const_reference        = const T&;
    using pointer                = alloc_traits::pointer;
    using const_pointer          = alloc_traits::const_pointer;
    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    FORBID_ALL(ImmovableVector);

    /// @brief Constructs ImmovableVector.
    /// @param alloc allocator to use.
    /// @param size the number of elements to hold.
    /// @param args arguments passed to the constructors of the elements.
    template <typename... Args>
    ImmovableVector(const Allocator& alloc, size_type size, Args&&... args) :
        m_alloc(alloc), m_size(size)
    {
        p_data = alloc_traits::allocate(m_alloc, m_size);

        for(size_type i = 0; i < m_size; ++i)
            std::construct_at(p_data + i, std::forward<Args>(args)...);
    }

    /// @brief Constructs ImmovableVector.
    ///
    /// Allocator is default-constructed.
    /// @param size the number of elements to hold.
    /// @param args arguments passed to the constructors of the elements.
    template <typename... Args>
    ImmovableVector(size_type size, Args&&... args) :
        ImmovableVector(Allocator{}, size, std::forward<Args>(args)...)
    {
    }

    ~ImmovableVector()
    {
        for(size_type i = 0; i < m_size; ++i)
            alloc_traits::destroy(m_alloc, p_data + i);

        alloc_traits::deallocate(m_alloc, p_data, m_size);
    }

    size_type
    size() const noexcept
    {
        return m_size;
    }

    reference
    operator[](const size_type i) noexcept
    {
        CHECK_INDEX(i, m_size);
        return p_data[i];
    }

    const_reference
    operator[](const size_type i) const noexcept
    {
        CHECK_INDEX(i, m_size);
        return p_data[i];
    }

    const_iterator
    begin() const noexcept
    {
        return p_data;
    }

    const_iterator
    end() const noexcept
    {
        return p_data + m_size;
    }

    iterator
    begin() noexcept
    {
        return p_data;
    }

    iterator
    end() noexcept
    {
        return p_data + m_size;
    }

    const_iterator
    cbegin() const noexcept
    {
        return p_data;
    }

    const_iterator
    cend() const noexcept
    {
        return p_data + m_size;
    }

    reverse_iterator
    rbegin() noexcept
    {
        return {end()};
    }

    reverse_iterator
    rend() noexcept
    {
        return {end()};
    }

    const_reverse_iterator
    rbegin() const noexcept
    {
        return {end()};
    }

    const_reverse_iterator
    rend() const noexcept
    {
        return {end()};
    }

    const_reverse_iterator
    crbegin() const noexcept
    {
        return {end()};
    }

    const_reverse_iterator
    crend() const noexcept
    {
        return {end()};
    }

    bool
    empty() const noexcept
    {
        return m_size == 0;
    }

    pointer
    data() noexcept
    {
        return p_data;
    }

    const_pointer
    data() const noexcept
    {
        return p_data;
    }

    friend iterator
    begin(ImmovableVector& v) noexcept
    {
        return v.begin();
    }

    friend iterator
    end(ImmovableVector& v) noexcept
    {
        return v.end();
    }

    friend const_iterator
    begin(const ImmovableVector& v) noexcept
    {
        return v.begin();
    }

    friend const_iterator
    end(const ImmovableVector& v) noexcept
    {
        return v.end();
    }

    friend const_iterator
    cbegin(const ImmovableVector& v) noexcept
    {
        return v.begin();
    }

    friend const_iterator
    cend(const ImmovableVector& v) noexcept
    {
        return v.end();
    }

    friend reverse_iterator
    rbegin(ImmovableVector& v) noexcept
    {
        return v.rbegin();
    }

    friend reverse_iterator
    rend(ImmovableVector& v) noexcept
    {
        return v.rend();
    }

    friend const_reverse_iterator
    rbegin(const ImmovableVector& v) noexcept
    {
        return v.rbegin();
    }

    friend const_reverse_iterator
    rend(const ImmovableVector& v) noexcept
    {
        return v.rend();
    }

    friend const_reverse_iterator
    crbegin(const ImmovableVector& v) noexcept
    {
        return v.crbegin();
    }

    friend const_reverse_iterator
    crend(const ImmovableVector& v) noexcept
    {
        return v.crend();
    }

    friend size_type
    size(const ImmovableVector& v) noexcept
    {
        return v.size();
    }

    friend pointer
    data(ImmovableVector& v) noexcept
    {
        return v.data();
    }

    friend const_pointer
    data(const ImmovableVector& v) noexcept
    {
        return v.data();
    }
};
} // namespace loom
