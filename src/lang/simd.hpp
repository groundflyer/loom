// -*- C++ -*-
/// @file simd.hpp
/// SIMD evaluator.

#pragma once

#include "bve.hpp"
#include "lang.hpp"

#include <utils/debug.hpp>
#include <utils/tmp.hpp>

#ifndef NDEBUG
#include <fmt/core.h>
#endif

#include <cstdlib>
#include <memory>
#include <new>

#if defined(__clang__)
#define _SHUFFLE __builtin_shufflevector
#elif defined(__GNUG__)
#define _SHUFFLE __builtin_shuffle
#endif

namespace lucid::simd
{
/// @brief Faster size type enough for our purposes.
using fsize_t = std::uint_fast32_t;

constexpr inline fsize_t dynamic_extent = -1;

#if defined(__AVX512F__)
constexpr inline fsize_t simd_width     = 512;
constexpr inline fsize_t simd_alignment = 64;
#elif defined(__AVX2__) || defined(__AVX__)
constexpr inline fsize_t simd_width     = 256;
constexpr inline fsize_t simd_alignment = 32;
#else
constexpr inline fsize_t simd_width     = 128;
constexpr inline fsize_t simd_alignment = 16;
#endif

template <fsize_t alignment>
constexpr fsize_t
align_to(fsize_t n) noexcept
{
    const fsize_t m = n % alignment;
    return m == 0 ? n : n + alignment - m;
}

template <typename T, fsize_t alignment = simd_alignment>
class AlignedAllocator
{
  public:
    using value_type = T;
    using pointer    = T*;

    template <typename U>
    struct rebind
    {
        using other = AlignedAllocator<U, alignment>;
    };

    AlignedAllocator() noexcept = default;

    pointer
    allocate(fsize_t n)
    {
        const fsize_t an = align_to<alignment>(sizeof(T) * n);
        void*         p  = std::aligned_alloc(alignment, an);
        if(!p) throw std::bad_alloc();

        return static_cast<pointer>(p);
    }

    void
    deallocate(pointer p, fsize_t)
    {
        std::free(p);
    }
};

template <typename T,
          template <typename, typename>
          typename Vector,
          template <typename>
          typename Allocator>
struct vector_union
{
    using type = Vector<T, Allocator<T>>;
};

template <typename... Ts,
          template <typename, typename>
          typename Vector,
          template <typename>
          typename Allocator>
struct vector_union<Union<Ts...>, Vector, Allocator>
{
    using type = UnionVector<type_sequence<Ts...>, Allocator<std::byte>>;
};

template <typename T,
          template <typename, typename>
          typename Vector,
          template <typename>
          typename Allocator>
using vector_union_t = typename vector_union<T, Vector, Allocator>::type;

template <typename Data, typename Helper>
class _proxy_row
{
    Data&   m_data;
    fsize_t m_pos;

    void
    copy_values(_proxy_row rhs) noexcept
    {
        lucid::for_each([&](auto& c) noexcept { c[m_pos] = c[rhs.m_pos]; },
                        m_data);
    }

  public:
    _proxy_row() = delete;

    _proxy_row(const _proxy_row& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos)
    {
    }

    _proxy_row(_proxy_row&& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos)
    {
    }

    _proxy_row(Data& data, const fsize_t pos) noexcept :
        m_data(data), m_pos(pos)
    {
    }

    _proxy_row&
    operator=(const _proxy_row& rhs) noexcept
    {
        copy_values(rhs);
        m_pos = rhs.m_pos;

        return *this;
    }

    _proxy_row&
    operator=(_proxy_row&& rhs) noexcept
    {
        copy_values(rhs);
        m_pos = rhs.m_pos;

        return *this;
    }

    bool
    operator==(const _proxy_row rhs) const noexcept
    {
        return cp() == rhs.cp();
    }

    std::strong_ordering
    operator<=>(const _proxy_row rhs) const noexcept
    {
        return cp() <=> rhs.cp();
    }

    template <typename Var>
    std::add_lvalue_reference_t<vartype<Var>>
    get() const noexcept
    {
        return Helper::template get<Var>(m_data)[m_pos];
    }

    std::add_lvalue_reference_t<vartype<_CP>>
    cp() const noexcept
    {
        return get<_CP>();
    }

    friend void
    swap(_proxy_row a, _proxy_row b) noexcept
    {
        lucid::for_each([&](auto& c) noexcept
                        { std::swap(c[a.m_pos], c[b.m_pos]); },
                        a.m_data);
    }

    friend void
    print(_proxy_row p, unsigned char a = 10) noexcept
    {
        lucid::for_each([&](auto& c) noexcept
                        { fmt::print("{:>{}} |", c[p.m_pos], a); },
                        p.m_data);
        fmt::print("\n");
    }
};

template <typename Data, typename Helper, fsize_t N>
class _proxy_view
{
    Data&   m_data;
    fsize_t m_pos;

    fsize_t
    shift(fsize_t p) const noexcept
    {
        CHECK_INDEX(p, N);
        return m_pos + p;
    }

  public:
    _proxy_view() = delete;
    _proxy_view&
    operator=(const _proxy_view& rhs) = delete;
    _proxy_view&
    operator=(_proxy_view&&) = delete;

    _proxy_view(const _proxy_view& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos)
    {
    }
    _proxy_view(_proxy_view&& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos){};

    _proxy_view(Data& data, fsize_t pos) noexcept : m_data(data), m_pos(pos) {}

    _proxy_row<Data, Helper>
    operator[](const fsize_t n) const noexcept
    {
        return {m_data, shift(n)};
    }

    template <fsize_t N2>
        requires(N2 <= N)
    _proxy_view<Data, Helper, N2>
    view(const fsize_t pos = 0) const noexcept
    {
        ASSERT(N2 <= N - pos, "Extent too big");
        return {m_data, shift(pos)};
    }

    _proxy_view<Data, Helper, dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, shift(pos), ext};
    }

    _proxy_view<Data, Helper, dynamic_extent>
    view(const fsize_t pos = 0) const noexcept
    {
        return view(pos, size() - pos);
    }

    template <typename Var>
    std::span<vartype<Var>, N>
    get() const noexcept
    {
        return std::span<vartype<Var>, N>(
            std::addressof(Helper::template get<Var>(m_data)[m_pos]),
            N);
    }

    std::span<vartype<_CP>, N>
    cp() const noexcept
    {
        return get<_CP>();
    }

    constexpr fsize_t
    size() const noexcept
    {
        return N;
    }
};

template <typename Data, typename Helper>
class _proxy_view<Data, Helper, dynamic_extent>
{
    Data&   m_data;
    fsize_t m_pos;
    fsize_t m_ext;

    fsize_t
    shift(fsize_t p) const noexcept
    {
        CHECK_INDEX(p, m_ext);
        return m_pos + p;
    }

  public:
    _proxy_view() = delete;
    _proxy_view&
    operator=(const _proxy_view& rhs) = delete;
    _proxy_view&
    operator=(_proxy_view&&) = delete;

    _proxy_view(const _proxy_view& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext)
    {
    }
    _proxy_view(_proxy_view&& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext){};

    _proxy_view(Data& data, fsize_t pos, fsize_t ext) noexcept :
        m_data(data), m_pos(pos), m_ext(ext)
    {
    }

    _proxy_row<Data, Helper>
    operator[](const fsize_t n) const noexcept
    {
        return {m_data, shift(n)};
    }

    template <fsize_t N>
    _proxy_view<Data, Helper, N>
    view(const fsize_t pos) const noexcept
    {
        return {m_data, shift(pos)};
    }

    _proxy_view<Data, Helper, dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, shift(pos), ext};
    }

    _proxy_view<Data, Helper, dynamic_extent>
    view(const fsize_t pos = 0) const noexcept
    {
        CHECK_INDEX(pos, size());
        return view(pos, size() - pos);
    }

    template <typename Var>
    std::span<vartype<Var>, std::dynamic_extent>
    get() const noexcept
    {
        return {std::addressof(Helper::template get<Var>(m_data)[m_pos]),
                m_ext};
    }

    std::span<vartype<_CP>, std::dynamic_extent>
    cp() const noexcept
    {
        return get<_CP>();
    }

    fsize_t
    size() const noexcept
    {
        return m_ext;
    }
};

template <typename _ProxyView>
struct _proxy_view_extent;

template <typename Data, typename Helper, fsize_t N>
struct _proxy_view_extent<_proxy_view<Data, Helper, N>>
{
    static constexpr fsize_t value = N;
};

template <typename _ProxyView>
constexpr inline fsize_t pv_extent = _proxy_view_extent<_ProxyView>::value;

template <typename RegTypes,
          typename RegNames,
          fsize_t alignment                              = simd_alignment,
          template <typename, typename> typename _Vector = std::vector>
class Memory
{
    template <typename T>
    using Vector = _Vector<T, AlignedAllocator<T, alignment>>;

    template <typename T>
    using Allocator = AlignedAllocator<T, alignment>;

    template <typename T>
    using f = vector_union_t<T, _Vector, Allocator>;

    using helper = MemoryHelper<RegTypes, RegNames, std::tuple, f>;

    using VecTypes = typelist::map<f, RegTypes>;

    using Data = helper::data;

    using idxs = typelist::indices<RegNames>;

    Data m_data;

    template <fsize_t N>
    using proxy_view = _proxy_view<Data, helper, N>;

    using proxy_row = _proxy_row<Data, helper>;

    class _iterator : public std::contiguous_iterator_tag
    {
        Data*   p_data = nullptr;
        fsize_t m_pos{};
        fsize_t m_size{};

      public:
        using difference_type = std::ptrdiff_t;
        using value_type      = proxy_row;

        _iterator() noexcept = default;

        _iterator(Data* data, fsize_t pos, fsize_t size) noexcept :
            p_data(data), m_pos(pos), m_size(size)
        {
        }
        _iterator(const _iterator& other) noexcept :
            p_data(other.p_data), m_pos(other.m_pos), m_size(other.m_size)
        {
        }
        _iterator(_iterator&& other) noexcept :
            p_data(other.p_data), m_pos(other.m_pos), m_size(other.m_size)
        {
        }

        _iterator&
        operator=(const _iterator& rhs) noexcept
        {
            p_data = rhs.p_data;
            m_pos  = rhs.m_pos;
            m_size = rhs.m_size;

            return *this;
        }

        _iterator&
        operator=(_iterator&& rhs) noexcept
        {
            p_data = rhs.p_data;
            m_pos  = rhs.m_pos;
            m_size = rhs.m_size;

            return *this;
        }

        bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_pos == m_size;
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

        value_type
        operator*() const noexcept
        {
            return {*p_data, m_pos};
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
            ++m_pos;
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
            return {p_data, m_pos + rhs, m_size};
        }

        difference_type
        operator+(_iterator rhs) const noexcept
        {
            return m_pos + rhs.m_pos;
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
            return {p_data, m_pos - rhs, m_size};
        }

        difference_type
        operator-(_iterator rhs) const noexcept
        {
            return m_pos - rhs.m_pos;
        }

        _iterator&
        operator-=(difference_type rhs) noexcept
        {
            m_pos -= rhs;
            return *this;
        }

        value_type
        operator[](difference_type n) const noexcept
        {
            CHECK_INDEX(n, count());
            return {*p_data, m_pos + n};
        }

        fsize_t
        count() const noexcept
        {
            return m_size - m_pos;
        }

        friend _iterator
        operator+(difference_type n, _iterator j) noexcept
        {
            return {j.p_data, n + j.m_pos, j.m_size};
        }
    };

  public:
    using value_type = proxy_row;
    using view_type  = proxy_view<dynamic_extent>;
    using iterator   = _iterator;

    Memory() noexcept = default;

    Memory(const Memory&) = delete;
    Memory(Memory&&)      = delete;

    Memory&
    operator=(const Memory&) = delete;
    Memory&
    operator=(Memory&&) = delete;

    proxy_row
    operator[](const fsize_t n) noexcept
    {
        CHECK_INDEX(n, size());
        return {m_data, n};
    }

    template <fsize_t N>
    proxy_view<N>
    view(const fsize_t pos = 0) noexcept
    {
        CHECK_INDEX(pos, size() - N);
        return {m_data, pos};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) noexcept
    {
        CHECK_INDEX(pos, size());
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, pos, ext};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos = 0) noexcept
    {
        CHECK_INDEX(pos, size());
        return view(pos, size() - pos);
    }

    iterator
    begin() noexcept
    {
        return {&m_data, 0ul, size()};
    }

    iterator
    end() noexcept
    {
        return {&m_data, size(), size()};
    }

    fsize_t
    size() const noexcept
    {
        return std::get<0>(m_data).size();
    }

    template <typename Var>
    std::span<vartype<Var>>
    get() noexcept
    {
        return {helper::template get<Var>(m_data).data(), size()};
    }

    Vector<vartype<_CP>>&
    cp() noexcept
    {
        return get<_CP>();
    }

    void
    resize(fsize_t new_size)
    {
        lucid::for_each([&](auto& c) noexcept { c.resize(new_size); }, m_data);
    }

    friend iterator
    begin(Memory& m) noexcept
    {
        return m.begin();
    }

    friend std::default_sentinel_t
    end(Memory&) noexcept
    {
        return std::default_sentinel;
    }

    friend fsize_t
    size(Memory& m) noexcept
    {
        return m.size();
    }
};

template <typename T, fsize_t N>
struct get_packed_vector
{
    typedef T type __attribute__((vector_size(N * sizeof(T))));
};

template <typename T, fsize_t N>
using packed_vector = typename get_packed_vector<T, N>::type;

namespace detail
{
template <typename T>
    requires(std::is_floating_point_v<T> || std::is_integral_v<T>)
struct get_native_vector_width
{
    static constexpr fsize_t value = simd_width / 8 / sizeof(T);
};

template <typename C>
struct native_instruction_width;

template <typename Dst, typename Src>
struct native_instruction_width<Copy<Dst, Src>>
{
    static constexpr fsize_t value =
        get_native_vector_width<vartype<Dst>>::value;
};

template <typename Op, typename Dst, typename... Args>
struct native_instruction_width<Compute<Op, Dst, Args...>>
{
#ifdef __AVX__
    // AVX doesn't have integer operations - fallback to SSE width.
    using vt                       = vartype<Dst>;
    static constexpr fsize_t w     = get_native_vector_width<vt>::value;
    static constexpr fsize_t value = std::is_integral_v<vt> ? (w / 2) : w;
#else
    static constexpr fsize_t value =
        get_native_vector_width<vartype<Dst>>::value;
#endif
};

template <typename Target>
struct native_instruction_width<Jump<Target>>
{
    static constexpr fsize_t value = get_native_vector_width<index_t>::value;
};

template <typename Var, typename Labels>
struct native_instruction_width<ParmJump<Var, Labels>>
{
#ifdef __AVX__
    // AVX doesn't have integer operations - fallback to SSE width.
    static constexpr fsize_t value =
        get_native_vector_width<index_t>::value / 2;
#else
    static constexpr fsize_t value = get_native_vector_width<index_t>::value;
#endif
};

template <fsize_t I, typename VT, typename T>
void
storei(VT src, T* dst) noexcept
{
    dst[I] = src[I];
}

template <typename VT, typename T, fsize_t... Is>
void
store_impl(VT src, T* dst, std::index_sequence<Is...>) noexcept
{
    (..., storei<Is>(src, dst));
}

template <fsize_t, typename T>
constexpr T
fake_get(T v) noexcept
{
    return v;
}
} // namespace detail

template <typename T>
constexpr inline fsize_t native_vector_width =
    detail::get_native_vector_width<T>::value;

template <typename T>
using native_vector = packed_vector<T, native_vector_width<T>>;

namespace detail
{
template <fsize_t A, fsize_t W, typename T, fsize_t... Is>
packed_vector<T, W>
load_impl(T* src, std::index_sequence<Is...>) noexcept
{
    src                     = std::assume_aligned<A, T>(src);
    packed_vector<T, W> ret = {src[Is]...};
    return ret;
}

template <fsize_t W, typename T, fsize_t... Is>
constexpr packed_vector<T, W>
broadcast_impl(T v, std::index_sequence<Is...>) noexcept
{
    packed_vector<T, W> ret = {fake_get<Is>(v)...};
    return ret;
}
} // namespace detail

template <typename C>
constexpr inline fsize_t native_instruction_width =
    detail::native_instruction_width<C>::value;

/// @brief Load @p N values and pack them into vector of width @p N
/// from aligned memory location @p src.
/// @tparam A memory alignment.
template <fsize_t A, fsize_t W, typename T>
packed_vector<T, W>
load(T* src) noexcept
{
    return detail::load_impl<A, W>(src, std::make_index_sequence<W>{});
}

/// @brief Store values from packed vector into aligned memory location @p dst.
/// @tparam A memory alignment.
template <fsize_t A, fsize_t W, typename T>
void
store(packed_vector<T, W> src, T* dst) noexcept
{
    dst = std::assume_aligned<A, T>(dst);
    detail::store_impl(src, dst, std::make_index_sequence<W>{});
}

/// @brief Broadcast a value to all elements of the resulting packed vector.
template <fsize_t W, typename T>
constexpr packed_vector<T, W>
broadcast(T v) noexcept
{
    return detail::broadcast_impl<W>(v, std::make_index_sequence<W>{});
}

template <fsize_t W, typename Op>
struct remap_op;

template <fsize_t W, typename T>
struct remap_op<W, std::equal_to<T>>
{
    struct type
    {
        constexpr packed_vector<index_t, W>
        operator()(packed_vector<T, W> a, packed_vector<T, W> b) const noexcept
        {
            return a == b;
        }
    };
};

template <fsize_t W, typename T>
struct remap_op<W, std::not_equal_to<T>>
{
    struct type
    {
        constexpr packed_vector<index_t, W>
        operator()(packed_vector<T, W> a, packed_vector<T, W> b) const noexcept
        {
            return a != b;
        }
    };
};

template <fsize_t W, typename T>
struct remap_op<W, std::greater<T>>
{
    struct type
    {
        constexpr packed_vector<index_t, W>
        operator()(packed_vector<T, W> a, packed_vector<T, W> b) const noexcept
        {
            return a > b;
        }
    };
};

template <fsize_t W, typename T>
struct remap_op<W, std::less<T>>
{
    struct type
    {
        constexpr packed_vector<index_t, W>
        operator()(packed_vector<T, W> a, packed_vector<T, W> b) const noexcept
        {
            return a < b;
        }
    };
};

template <fsize_t W, typename T>
struct remap_op<W, std::greater_equal<T>>
{
    struct type
    {
        constexpr packed_vector<index_t, W>
        operator()(packed_vector<T, W> a, packed_vector<T, W> b) const noexcept
        {
            return a >= b;
        }
    };
};

template <fsize_t W, typename T>
struct remap_op<W, std::less_equal<T>>
{
    struct type
    {
        constexpr packed_vector<index_t, W>
        operator()(packed_vector<T, W> a, packed_vector<T, W> b) const noexcept
        {
            return a <= b;
        }
    };
};

// remaping std::plus, multiply, etc.
// they work with vector extensions
template <fsize_t W, typename T, template <typename> typename Op>
struct remap_op<W, Op<T>>
{
    using type = Op<packed_vector<T, W>>;
};

template <fsize_t W, typename Op>
using remap_op_t = typename remap_op<W, Op>::type;

template <fsize_t alignment, fsize_t W>
struct var_loader
{
    template <typename Var, typename _ProxyView>
        requires(pv_extent<_ProxyView> == W)
    packed_vector<vartype<Var>, W>
    operator()(_ProxyView view) const noexcept
    {
        constexpr auto l = parse_literal<Var>();
        if constexpr(l.has_value())
            return broadcast<W>(l.value());
        else
            return load<alignment, W>(view.template get<Var>().data());
    }
};

template <typename C, typename LabelMap, fsize_t alignment>
struct eval;

template <typename Dst, typename Src, typename LabelMap, fsize_t alignment>
struct eval<Copy<Dst, Src>, LabelMap, alignment>
{
    using T                    = vartype<Dst>;
    static constexpr fsize_t w = native_instruction_width<Copy<Dst, Src>>;
    using loader_t             = var_loader<alignment, w>;
    static constexpr loader_t loader{};

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == w)
    void
    operator()(_ProxyView view) const noexcept
    {
        T* dst = view.template get<Dst>().data();
        store<alignment, w>(loader.template operator()<Src>(), dst);
    }
};

template <typename Target, typename LabelMap, fsize_t alignment>
struct eval<Jump<Target>, LabelMap, alignment>
{
    using constant = typename typelist::lookup<Target, LabelMap>;
    static constexpr index_t target = constant{}();
    static constexpr fsize_t w      = native_instruction_width<Jump<Target>>;

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == w)
    void
    operator()(_ProxyView view) const noexcept
    {
        index_t*                  dst = view.cp().data();
        packed_vector<index_t, w> t   = broadcast<w>(target);
        store<alignment, w>(t, dst);
    }
};

template <typename Op,
          typename Dst,
          typename... Args,
          typename LabelMap,
          fsize_t alignment>
struct eval<Compute<Op, Dst, Args...>, LabelMap, alignment>
{
    using T = vartype<Dst>;
    static constexpr fsize_t w =
        native_instruction_width<Compute<Op, Dst, Args...>>;
    using vector_op = remap_op_t<w, Op>;
    using loader_t  = var_loader<alignment, w>;
    static constexpr loader_t loader{};

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == w)
    void
    operator()(_ProxyView view) const noexcept
    {
        T*   dst = view.template get<Dst>().data();
        auto vars =
            typelist::maybe_collect(loader, type_sequence<Args...>{}, view);
        packed_vector<T, w> ret = maybe_apply(vector_op{}, vars);
        store<alignment, w>(ret, dst);
    }
};

template <typename Idxs, fsize_t w>
struct shuffler;

template <index_t a, index_t b, fsize_t w>
struct shuffler<std::integer_sequence<index_t, a, b>, w>
{
    using IdxVec = packed_vector<index_t, w>;

    static constexpr IdxVec va = broadcast<w>(a);
    static constexpr IdxVec vb = broadcast<w>(b);

    IdxVec
    operator()(IdxVec var) const noexcept
    {
        return var ? vb : va;
    }
};

template <index_t... is, fsize_t w>
    requires(sizeof...(is) <= w)
struct shuffler<std::integer_sequence<index_t, is...>, w>
{
    using IdxVec                  = packed_vector<index_t, w>;
    static constexpr IdxVec table = {is...};

    IdxVec
    operator()(IdxVec var) const noexcept
    {
        return _SHUFFLE(table, var);
    }
};

template <typename Var, typename Labels, typename LabelMap, fsize_t alignment>
struct eval<ParmJump<Var, Labels>, LabelMap, alignment>
{
    static constexpr fsize_t w =
        native_instruction_width<ParmJump<Var, Labels>>;

    using idxs = pjmp_idxs<ParmJump<Var, Labels>, LabelMap>;
    static constexpr shuffler<idxs, w> op{};

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == w)
    void
    operator()(_ProxyView view) const noexcept
    {
        index_t* dst = view.cp().data();
        index_t* src = view.template get<Var>().data();
        store<alignment, w>(op(load<alignment, w>(src)), dst);
    }
};

template <typename C, typename LabelMap, fsize_t alignment, fsize_t bulk_number>
struct vertical_runner
{
    static constexpr fsize_t stride = native_instruction_width<C>;
    static constexpr eval<C, LabelMap, alignment> v_eval{};
    static constexpr standard_eval<C, LabelMap>   s_eval{};
    using bulk_sizes =
        intlist::map_f<decltype([](fsize_t n) constexpr noexcept -> fsize_t
                                { return n * stride; }),
                       intlist::reverse<intlist::powerof2s<bulk_number>>>;

    template <typename _MemoryView>
        requires(pv_extent<_MemoryView> % stride == 0 &&
                 pv_extent<_MemoryView> != dynamic_extent)
    static void
    run_view(_MemoryView view) noexcept
    {
        for(fsize_t i = 0; i < pv_extent<_MemoryView>; i += stride)
            v_eval(view.template view<stride>(i));
    }

    template <fsize_t bulk_stride, typename _MemoryView>
    static void
    run_bulk(_MemoryView mem, fsize_t num_bulks) noexcept
    {
        for(fsize_t i = 0; i < num_bulks * bulk_stride; i += bulk_stride)
            run_view(mem.template view<bulk_stride>(i));
    }

    template <typename _MemoryView, fsize_t bulk_stride, fsize_t... rest>
    static void
    dispatch_bulks(
        _MemoryView mem,
        std::integer_sequence<fsize_t, bulk_stride, rest...>) noexcept
    {
        const fsize_t s = mem.size();
        const fsize_t q = s / bulk_stride;
        const fsize_t r = s % bulk_stride;

        if(q) run_bulk<bulk_stride>(mem, q);

        // reminder is zero - nothing left
        if(!r) return;

        if constexpr(sizeof...(rest))
            return dispatch_bulks(mem.view(s - r),
                                  std::integer_sequence<fsize_t, rest...>{});

        // evaluate remained sequentially
        for(fsize_t i = mem.size() - r; i < mem.size(); ++i) s_eval(mem[i]);
    }

    template <typename _MemoryView>
    void
    operator()(_MemoryView mem) const noexcept
    {
        dispatch_bulks(mem, bulk_sizes{});
    }
};

template <typename _Memory>
class memory_chunks : public std::ranges::view_interface<memory_chunks<_Memory>>
{
    using proxy_view = _Memory::view_type;

    proxy_view m_view;

    static fsize_t
    chunk_size(std::span<index_t> cps) noexcept
    {
        const index_t cp = cps.front();
        fsize_t       i  = 0;
        while(cps[i] == cp && i < cps.size()) ++i;
        return i;
    }

    class _iterator : public std::input_iterator_tag
    {
        proxy_view m_view{};
        fsize_t    m_pos{};

        fsize_t
        offset() const noexcept
        {
            return chunk_size(m_view.cp().subspan(m_pos));
        }

        proxy_view
        chunk() const noexcept
        {
            return m_view.view(m_pos, offset());
        }

        void
        next() noexcept
        {
            m_pos += offset();
        }

      public:
        using difference_type = std::ptrdiff_t;
        using value_type      = proxy_view;

        _iterator() noexcept = default;

        _iterator(proxy_view view, fsize_t pos = 0) noexcept :
            m_view(view), m_pos(pos)
        {
        }
        _iterator(const _iterator& other) noexcept :
            m_view(other.m_view), m_pos(other.m_pos)
        {
        }
        _iterator(_iterator&& other) noexcept :
            m_view(other.m_view), m_pos(other.m_pos)
        {
        }

        _iterator&
        operator=(const _iterator& rhs) noexcept
        {
            m_view = rhs.m_view;
            m_pos  = rhs.m_pos;

            return *this;
        }

        _iterator&
        operator=(_iterator&& rhs) noexcept
        {
            m_view = rhs.m_view;
            m_pos  = rhs.m_pos;

            return *this;
        }

        bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_pos == m_view.size();
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

        value_type
        operator*() const noexcept
        {
            return chunk();
        }

        _iterator&
        operator++() noexcept
        {
            next();
            return *this;
        }

        _iterator
        operator++(int) noexcept
        {
            _iterator ret(*this);
            next();
            return ret;
        }
    };

  public:
    memory_chunks() = default;
    memory_chunks(_Memory& mem) noexcept : m_view(mem.view()) {}

    _iterator
    begin() const
    {
        return _iterator(m_view);
    }

    std::default_sentinel_t
    end() const
    {
        return std::default_sentinel;
    }
};

template <typename _Program, fsize_t bulk_number>
    requires(bulk_number > 0)
class simd_evaluator
{
    using labelmap  = typename _Program::labelmap;
    using cells     = typename _Program::cells;
    using reg_names = typename _Program::reg_names;
    using reg_types = typename _Program::reg_types;

    template <typename C, typename LabelMap>
    using simd_eval = vertical_runner<C, LabelMap, simd_alignment, bulk_number>;

    template <typename Cell>
    using compile_cell = standard_compiler<Cell, labelmap, simd_eval>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    static constexpr bve::input_setter<typename _Program::input_vars> setter{};
    static constexpr bve::output_getter<typename _Program::output_vars>
        getter{};

    using memory = Memory<reg_types, reg_names, simd_alignment>;

    struct single_cell_evaluator
    {
        static constexpr typelist::head<compiled_cells> cell{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(fsize_t n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);
            cell(mem.view());
            return getter(mem);
        }
    };

    struct evaluator
    {
        static constexpr typename typelist::to<compiled_cells, std::tuple>
            cells{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(fsize_t n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);

            const std::span<index_t> cps = mem.view().cp();
            while(cps.front() != end_pos)
            {
                std::ranges::for_each(memory_chunks(mem),
                                      [](auto view) noexcept
                                      {
                                          visit(
                                              view.cp().front(),
                                              [&](auto& cell) noexcept
                                              { cell(view); },
                                              cells,
                                              sink);
                                      });

#ifdef _LIBCPP_VERSION
                std::sort(mem.begin(), mem.end());
#else
                // libstdc++ std::sort doesn't use swap internally
                // so we sort it manually
                for(std::size_t i = 1; i < mem.size(); ++i)
                    for(std::size_t j = i; j > 0 && mem[j - 1] > mem[j]; --j)
                        swap(mem[j], mem[j - 1]);
#endif

#ifndef NDEBUG
                {
                    typelist::for_each(
                        []<typename Name>() noexcept
                        {
                            constexpr std::string_view n =
                                std::string_view(intlist::array<Name>,
                                                 intlist::length<Name>);
                            fmt::print("{:>{}} |", n, 10);
                        },
                        reg_names{});
                    fmt::print("\n");
                    typelist::for_each([]<typename Name>() noexcept
                                       { fmt::print("{:*>{}} |", '*', 10); },
                                       reg_names{});
                    fmt::print("\n");
                    std::ranges::for_each(mem,
                                          [](auto p) noexcept
                                          { print(p, 10); });
                }
#endif
            }

            return getter(mem);
        }
    };

  public:
    using type = std::
        conditional_t<_Program::singular, single_cell_evaluator, evaluator>;
};

template <typename _Program, fsize_t bulk_number = 8>
using evaluator = typename simd_evaluator<_Program, bulk_number>::type;
} // namespace lucid::simd
