// -*- C++ -*-
/// @file memory.hpp
/// @brief Memory utilities.

#pragma once

#include "math.hpp"

#include <memory>

namespace loom
{
#ifdef CACHE_LINE_SIZE
constexpr inline std::size_t cache_line_size = CACHE_LINE_SIZE;
#else
constexpr inline std::size_t cache_line_size = 64;
#endif

/// @brief Allocator capable to allocate aligned memory region.
template <typename T, std::size_t alignment>
class AlignedAllocator
{
    static constexpr std::size_t
    align_size(std::size_t n) noexcept
    {
        return align_up(n, alignment);
    }

  public:
    using value_type = T;
    using pointer    = T*;

    template <typename U>
    struct rebind
    {
        using other = AlignedAllocator<U, alignment>;
    };

    AlignedAllocator() noexcept = default;

    /// @brief Allocates unitialized storage aligned to @p alignment capable of
    /// holding at least @p n elements of type @p T.
    ///
    /// The size of the storage is also aligned to @p alignment.
    pointer
    allocate(std::size_t n) const
    {
        void* p = std::aligned_alloc(alignment, align_size(sizeof(T) * n));
        if(!p) throw std::bad_alloc();

        return static_cast<pointer>(p);
    }

    void
    deallocate(pointer p, std::size_t) const
    {
        std::free(p);
    }
};

/// @brief Allocates memory aligned to cache line size.
template <typename T>
using CacheLineAlignedAllocator = AlignedAllocator<T, cache_line_size>;

/// @brief The number of values of type @p T that can fit into cache line.
template <typename T>
    requires(sizeof(T) % alignof(T) == 0 && alignof(T) <= sizeof(T))
struct cache_line_capacity
{
    static constexpr std::size_t value = cache_line_size / sizeof(T);
};

/// @brief Helper template variable for cache_line_capacity.
template <typename T>
inline constexpr std::size_t cache_line_capacity_v =
    cache_line_capacity<T>::value;

/// @brief Declare a value protected from false sharing.
#define PADDED_VALUE(TYPE, NAME)                 \
    union alignas(cache_line_size)               \
    {                                            \
        TYPE         NAME{};                     \
        std::uint8_t __padding[cache_line_size]; \
    }
} // namespace loom
