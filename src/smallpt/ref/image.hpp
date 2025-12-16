// -*- C++ -*-
// image.hpp --
//

#pragma once

#include "typedefs.hpp"

#include <iterator>
#include <memory>
#include <ranges>

namespace loom
{
class ScanlineIterator : public std::contiguous_iterator_tag
{
    unsigned m_size;
    unsigned m_width;
    unsigned m_pos;

  public:
    using difference_type = int;
    using value_type      = Vec2u;

    ScanlineIterator() = default;

    ScanlineIterator(unsigned size, unsigned width, unsigned pos) noexcept :
        m_size(size), m_width(width), m_pos(pos)
    {
    }

    ScanlineIterator(const Vec2u& res, unsigned pos = 0u) noexcept :
        m_size(product(res)), m_width(get_x(res)), m_pos(pos)
    {
    }

    value_type
    operator*() const noexcept
    {
        CHECK_INDEX(m_pos, m_size);
        return value_type{m_pos % m_width, m_pos / m_width};
    }

    bool
    operator==(std::default_sentinel_t) const noexcept
    {
        return m_pos == m_size;
    }

    std::strong_ordering
    operator<=>(const ScanlineIterator& rhs) const noexcept
    {
        return m_pos <=> rhs.m_pos;
    }

    ScanlineIterator&
    operator++() noexcept
    {
        ++m_pos;
        return *this;
    }

    ScanlineIterator
    operator++(int) noexcept
    {
        ScanlineIterator tmp(*this);
        operator++();
        return tmp;
    }

    ScanlineIterator&
    operator--() noexcept
    {
        --m_pos;
        return *this;
    }

    ScanlineIterator
    operator--(int) noexcept
    {
        ScanlineIterator ret(*this);
        --m_pos;
        return ret;
    }

    ScanlineIterator
    operator+(difference_type rhs) const noexcept
    {
        return {m_size, m_width, m_pos + static_cast<unsigned>(rhs)};
    }

    difference_type
    operator+(ScanlineIterator rhs) const noexcept
    {
        return m_pos + rhs.m_pos;
    }

    ScanlineIterator&
    operator+=(difference_type rhs) noexcept
    {
        m_pos += rhs;
        return *this;
    }

    ScanlineIterator
    operator-(difference_type rhs) const noexcept
    {
        return {m_size, m_width, static_cast<unsigned>(m_pos - rhs)};
    }

    difference_type
    operator-(ScanlineIterator rhs) const noexcept
    {
        return m_pos - rhs.m_pos;
    }

    ScanlineIterator&
    operator-=(difference_type rhs) noexcept
    {
        m_pos -= rhs;
        return *this;
    }

    value_type
    operator[](difference_type n) const noexcept
    {
        CHECK_INDEX(n, count());
        unsigned pos = m_pos + n;
        return value_type{pos % m_width, pos / m_width};
    }

    unsigned
    count() const noexcept
    {
        return m_size - m_pos;
    }

    friend ScanlineIterator
    operator+(difference_type n, ScanlineIterator j) noexcept
    {
        return {j.m_size, j.m_width, static_cast<unsigned>(n) + j.m_pos};
    }
};

// num_pixels for number of pixels vectors<T, NC>
// size for number of elements of type T
template <typename T, std::size_t NC, typename Allocator = std::allocator<T>>
class ScanlineImage
{
    Vec2u     m_res;
    Allocator m_alloc = Allocator();
    T*        p_data  = nullptr;

    std::size_t
    pos(const std::size_t x, const std::size_t y) const noexcept
    {
        CHECK_INDEX(x, width());
        CHECK_INDEX(y, height());
        return NC * (y * width() + x);
    }

    template <bool Const = false>
    class _iterator : public ScanlineIterator
    {
        using ImageRef =
            std::conditional_t<Const, const ScanlineImage&, ScanlineImage&>;
        using ImagePtr =
            std::conditional_t<Const, const ScanlineImage*, ScanlineImage*>;
        ImagePtr img = nullptr;

        Vec2u
        pos() const noexcept
        {
            return *statoc_cast<const ScanlineIterator&>(this);
        }

      public:
        using value_type = Vector<T, NC, std::span>;

        _iterator() = delete;
        _iterator(ImageRef _img, const unsigned pos = 0) noexcept :
            img(&_img), ScanlineIterator(_img.res(), pos)
        {
        }

        decltype(auto)
        operator*() const noexcept
        {
            CHECK_INDEX(iter.pos, img->size());
            return (*img)[pos()];
        }
    };

    _REINIT void
    reinit() noexcept
    {
        m_res  = Vec2u{0u};
        p_data = nullptr;
    }

  public:
    using value_type     = Vector<T, NC, std::span>;
    using iterator       = _iterator<false>;
    using const_iterator  = _iterator<true>;
    using reference       = value_type;
    using const_reference = Vector<const T, NC, std::span>;

    explicit ScanlineImage(const Allocator& alloc) noexcept : m_alloc(alloc) {}

    ScanlineImage(const Vec2u& res, const Allocator& alloc = Allocator()) :
        m_res(res), m_alloc(alloc)
    {
        p_data = std::allocator_traits<Allocator>::allocate(m_alloc, size());

        for(std::size_t i = 0; i < size(); ++i)
            std::allocator_traits<Allocator>::construct(m_alloc, p_data + i);
    }

    ScanlineImage(const ScanlineImage& rhs) :
        m_res(rhs.m_res), m_alloc(rhs.m_alloc)
    {
        p_data = std::allocator_traits<Allocator>::allocate(m_alloc, size());
        std::copy(rhs.p_data, rhs.p_data + rhs.size(), p_data);
    }

    ScanlineImage(ScanlineImage&& rhs) noexcept :
        m_res(rhs.m_res), m_alloc(std::move(rhs.m_alloc)), p_data(rhs.p_data)
    {
        rhs.reinit();
    }

    ScanlineImage&
    operator=(const ScanlineImage& rhs)
    {
        if(&rhs == this) return *this;

        if(size() != rhs.size())
        {
            if(p_data)
            {
                for(std::size_t i = 0; i < size(); ++i)
                    std::allocator_traits<Allocator>::destroy(m_alloc,
                                                              p_data + i);

                std::allocator_traits<Allocator>::deallocate(m_alloc,
                                                             p_data,
                                                             size());
            }

            m_res = rhs.m_res;
            p_data =
                std::allocator_traits<Allocator>::allocate(m_alloc, size());
        }

        std::copy(rhs.p_data, rhs.p_data + rhs.size(), p_data);

        return *this;
    }

    ScanlineImage&
    operator=(ScanlineImage&& rhs)
    {
        if(&rhs == this) return *this;

        if(size() != rhs.size())
        {
            if(p_data)
            {
                for(std::size_t i = 0; i < size(); ++i)
                    std::allocator_traits<Allocator>::destroy(m_alloc,
                                                              p_data + i);

                std::allocator_traits<Allocator>::deallocate(m_alloc,
                                                             p_data,
                                                             size());
            }

            m_res = rhs.m_res;
        }

        p_data = rhs.p_data;

        rhs.reinit();

        return *this;
    }

    ~ScanlineImage() noexcept
    {
        for(std::size_t i = 0; i < size(); ++i)
            std::allocator_traits<Allocator>::destroy(m_alloc, p_data + i);

        std::allocator_traits<Allocator>::deallocate(m_alloc, p_data, size());
    }

    iterator
    begin() noexcept
    {
        return iterator(*this, 0u);
    }

    iterator
    end() noexcept
    {
        return iterator(*this, num_pixels());
    }

    const_iterator
    begin() const noexcept
    {
        return const_iterator(*this, 0u);
    }

    const_iterator
    end() const noexcept
    {
        return const_iterator(*this, num_pixels());
    }

    const_iterator
    cbegin() const noexcept
    {
        return const_iterator(*this, 0u);
    }

    const_iterator
    cend() const noexcept
    {
        return const_iterator(*this, num_pixels());
    }

    reference
    operator[](const Vec2u& pos) noexcept
    {
        const auto& [x, y] = pos;
        return at(x, y);
    }

    const_reference
    operator[](const Vec2u& pos) const noexcept
    {
        const auto& [x, y] = pos;
        return at(x, y);
    }

    reference
    at(const std::size_t i) noexcept
    {
        CHECK_INDEX(i, size());
        return Vector(std::span<T, NC>(p_data + i, NC));
    }

    const_reference
    at(const std::size_t i) const noexcept
    {
        CHECK_INDEX(i, size());
        return Vector(std::span<const T, NC>(p_data + i, NC));
    }

    reference
    at(const std::size_t x, const std::size_t y) noexcept
    {
        return at(pos(x, y));
    }

    const_reference
    at(const std::size_t x, const std::size_t y) const noexcept
    {
        return at(pos(x, y));
    }

    std::size_t
    width() const noexcept
    {
        return m_res.get<0>();
    }

    std::size_t
    height() const noexcept
    {
        return m_res.get<1>();
    }

    std::size_t
    num_pixels() const noexcept
    {
        return product(m_res);
    }

    std::size_t
    size() const noexcept
    {
        return num_pixels() * NC;
    }

    const Vec2u&
    res() const noexcept
    {
        return m_res;
    }

    const T*
    data() const noexcept
    {
        return p_data;
    }

    friend const_iterator
    begin(const ScanlineImage& img) noexcept
    {
        return img.cbegin();
    }

    friend const_iterator
    end(const ScanlineImage& img) noexcept
    {
        return img.cend();
    }

    friend iterator
    begin(ScanlineImage& img) noexcept
    {
        return img.begin();
    }

    friend iterator
    end(ScanlineImage& img) noexcept
    {
        return img.end();
    }
};
} // namespace loom
