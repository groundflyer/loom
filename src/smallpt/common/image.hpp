// -*- C++ -*-
/// @file image.hpp
/// @brief Multi-channel raster image.

#pragma once

#include "typedefs.hpp"

#include <iterator>
#include <memory>
#include <ranges>

namespace loom
{
/// @brief Compute a pair of pixel coordinates x, y from its linear offset.
constexpr Vec2u
pixel_coords(const unsigned offset, const unsigned width) noexcept
{
    return Vec2u{offset % width, offset / width};
}

/// @brief Compute linear array offset from pixel coordinates.
constexpr unsigned
pixel_offset(const Vec2u&   coords,
             const unsigned width,
             const unsigned nc) noexcept
{
    const auto [x, y] = coords;
    CHECK_INDEX(x, width);
    return nc * (y * width + x);
}

/// @brief Multi-channel raster image.
/// @tparam T channel value type.
/// @tparam NC number of channels.
///
/// Classes specialized from this template satisfy
/// std::ranges::random_access_range and std::ranges::sized_range.
///
/// When iterating over an image value type is a pair of pixel coordinates and a
/// view to the pixel.
///
/// For example:
/// @code{.cpp}
/// for (auto [pos, pix]: img)
/// {
///     const auto [px, py] = pos;
///     pix = compute_new_pixel_value(px, py);
/// }
/// @endcode
template <typename T, unsigned NC, typename Allocator = std::allocator<T>>
class ScanlineImage
{
    Vec2u       m_res;
    std::size_t m_size  = full_size();
    Allocator   m_alloc = Allocator();
    T*          p_data  = nullptr;

    using _ref        = Vector<T, NC, std::span>;
    using _value_type = std::pair<Vec2u, _ref>;

    std::size_t
    pixel_count() const noexcept
    {
        return product(m_res);
    }

    std::size_t
    full_size() const noexcept
    {
        return pixel_count() * NC;
    }

    class _iterator : public std::random_access_iterator_tag
    {
        ScanlineImage* p_img;
        unsigned       m_size;
        unsigned       m_width;
        unsigned       m_pos;

      public:
        using value_type      = _value_type;
        using difference_type = int;

        _iterator() = default;

        _iterator(ScanlineImage* img, unsigned pos = 0) noexcept :
            p_img(img), m_size(img->size()), m_width(img->width()), m_pos(pos)
        {
        }

        _iterator(const ScanlineImage* img, unsigned pos = 0) noexcept :
            p_img(img), m_size(img->size()), m_width(img->width()), m_pos(pos)
        {
        }

        value_type
        operator*() const noexcept
        {
            return value_type{pixel_coords(m_pos, p_img->width()),
                              p_img->at(m_pos)};
        }

        bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_pos >= m_size;
        }

        bool
        operator==(const _iterator& rhs) const noexcept
        {
            return m_pos == rhs.m_pos;
        }

        std::strong_ordering
        operator<=>(const _iterator& rhs) const noexcept
        {
            return m_pos <=> rhs.m_pos;
        }

        _iterator&
        operator++() noexcept
        {
            ++m_pos;
            return *this;
        }

        _iterator
        operator++(int) noexcept
        {
            _iterator ret(*this);
            operator++();
            return ret;
        }

        _iterator&
        operator--() noexcept
        {
            --m_pos;
            return *this;
        }

        _iterator
        operator--(int) noexcept
        {
            _iterator ret(*this);
            --m_pos;
            return ret;
        }

        _iterator
        operator+(difference_type rhs) const noexcept
        {
            return {p_img, m_pos + static_cast<unsigned>(rhs)};
        }

        _iterator
        operator+(const _iterator& rhs) const noexcept
        {
            return {p_img, m_pos + rhs.m_pos};
        }

        _iterator&
        operator+=(difference_type rhs) noexcept
        {
            m_pos += rhs;
            return *this;
        }

        _iterator
        operator-(difference_type rhs) const noexcept
        {
            return {p_img, static_cast<unsigned>(m_pos - rhs)};
        }

        difference_type
        operator-(const _iterator& rhs) const noexcept
        {
            return static_cast<difference_type>(m_pos) -
                   static_cast<difference_type>(rhs.m_pos);
        }

        _iterator&
        operator-=(difference_type rhs) noexcept
        {
            m_pos -= rhs;
            return static_cast<T&>(*this);
        }

        auto
        operator[](difference_type n) const noexcept
        {
            CHECK_INDEX(n, static_cast<int>(count()));
            const unsigned p = m_pos + n;
            return std::pair{pixel_coords(p, p_img->width()), p_img->at(p)};
        }

        unsigned
        count() const noexcept
        {
            return m_size - m_pos;
        }

        friend _iterator
        operator+(difference_type n, const _iterator& j) noexcept
        {
            return {j.p_img, static_cast<unsigned>(n) + j.m_pos};
        }
    };

    _REINIT void
    reinit() noexcept
    {
        m_res  = Vec2u{0u};
        m_size = 0;
        p_data = nullptr;
    }

    void
    allocate()
    {
        p_data = std::allocator_traits<Allocator>::allocate(m_alloc, m_size);

        for(std::size_t i = 0; i < m_size; ++i)
            std::allocator_traits<Allocator>::construct(m_alloc, p_data + i);
    }

    void
    deallocate() noexcept
    {
        for(std::size_t i = 0; i < size(); ++i)
            std::allocator_traits<Allocator>::destroy(m_alloc, p_data + i);

        std::allocator_traits<Allocator>::deallocate(m_alloc, p_data, m_size);
    }

  public:
    using value_type      = _ref;
    using iterator        = _iterator;
    using const_iterator  = const _iterator;
    using reference       = value_type;
    using const_reference = const value_type;

    ScanlineImage() noexcept = default;

    explicit ScanlineImage(const Allocator& alloc) noexcept : m_alloc(alloc) {}

    ScanlineImage(const Vec2u& res, const Allocator& alloc = Allocator()) :
        m_res(res), m_size(full_size()), m_alloc(alloc)
    {
        allocate();
    }

    ScanlineImage(const ScanlineImage& rhs) :
        m_res(rhs.m_res), m_size(full_size()), m_alloc(rhs.m_alloc)
    {
        allocate();
        std::copy(rhs.p_data, rhs.p_data + rhs.size(), p_data);
    }

    ScanlineImage(ScanlineImage&& rhs) :
        m_res(rhs.m_res), m_size(rhs.m_size), m_alloc(std::move(rhs.m_alloc)),
        p_data(rhs.p_data)
    {
        rhs.reinit();
    }

    ScanlineImage&
    operator=(const ScanlineImage& rhs)
    {
        if(&rhs == this) return *this;

        if(m_size != rhs.m_size)
        {
            if(p_data) deallocate();

            m_res  = rhs.m_res;
            m_size = rhs.m_size;

            allocate();
        }

        std::copy(rhs.p_data, rhs.p_data + rhs.m_size, p_data);

        return *this;
    }

    ScanlineImage&
    operator=(ScanlineImage&& rhs)
    {
        if(&rhs == this) return *this;

        if(size() != rhs.size())
        {
            if(p_data) deallocate();

            m_res  = rhs.m_res;
            m_size = rhs.m_size;
        }

        p_data = rhs.p_data;

        rhs.reinit();

        return *this;
    }

    ~ScanlineImage() { deallocate(); }

    iterator
    begin() noexcept
    {
        return iterator(this);
    }

    iterator
    end() noexcept
    {
        return iterator(this, size());
    }

    const_iterator
    begin() const noexcept
    {
        return const_iterator(this);
    }

    const_iterator
    end() const noexcept
    {
        return const_iterator(this, size());
    }

    const_iterator
    cbegin() const noexcept
    {
        return const_iterator(this);
    }

    const_iterator
    cend() const noexcept
    {
        return const_iterator(this, size());
    }

    reference
    operator[](const Vec2u& pos) noexcept
    {
        return at(pos);
    }

    const_reference
    operator[](const Vec2u& pos) const noexcept
    {
        return at(pos);
    }

    reference
    at(const unsigned i) noexcept
    {
        CHECK_INDEX(i, m_size);
        return Vector(std::span<T, NC>(p_data + i * NC, NC));
    }

    const_reference
    at(const unsigned i) const noexcept
    {
        CHECK_INDEX(i, m_size);
        return Vector(std::span<T, NC>(p_data + i * NC, NC));
    }

    reference
    at(const Vec2u& coords) noexcept
    {
        return at(pixel_offset(coords, width(), NC));
    }

    const_reference
    at(const Vec2u& coords) const noexcept
    {
        return at(pixel_offset(coords, width(), NC));
    }

    unsigned
    width() const noexcept
    {
        return m_res.get<0>();
    }

    unsigned
    height() const noexcept
    {
        return m_res.get<1>();
    }

    std::size_t
    size() const noexcept
    {
        return pixel_count();
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

template <typename It>
class OutputAdapter
{
    It m_iter;

  public:
    using difference_type = It::difference_type;
    using value_type      = It::value_type::second_type;

    explicit OutputAdapter(It it) noexcept : m_iter(it) {}

    OutputAdapter&
    operator++() noexcept
    {
        ++m_iter;
        return *this;
    }

    OutputAdapter
    operator++(int) noexcept
    {
        OutputAdapter tmp{*this};
        ++m_iter;
        return tmp;
    }

    value_type
    operator*() const noexcept
    {
        return (*m_iter).second;
    }
};
} // namespace loom
