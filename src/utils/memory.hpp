// -*- C++ -*-
/// @file memory.hpp
/// @brief Memory utilities.

#pragma once

#include <memory>

namespace lucid
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
        const std::size_t m = n % alignment;
        return m == 0 ? n : n + alignment - m;
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
} // namespace lucid
