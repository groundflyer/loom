// -*- C++ -*-
/// @file simd.hpp
/// SIMD evaluator.

#pragma once

#include "bve.hpp"
#include "lang.hpp"

#include <utils/debug.hpp>

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
        void* p =
            std::aligned_alloc(alignment, align_to<alignment>(sizeof(T) * n));
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

    using helper = MemoryHelper<RegTypes, RegNames, std::tuple, f, f>;

    using VecTypes = typelist::map<f, RegTypes>;

    using Data = helper::data;

    using idxs = typelist::indices<RegNames>;

    Data m_data;

    /// @brief Helper static function to run over columns of data.
    template <typename F>
    static void
    over(Data& data, F&& f)
    {
        lucid::for_each(std::forward<F>(f), data);
    }

    class proxy_row
    {
        Data&   m_data;
        fsize_t m_pos;

        void
        copy_values(proxy_row rhs) noexcept
        {
            over(m_data, [&](auto& c) noexcept { c[m_pos] = c[rhs.m_pos]; });
        }

      public:
        proxy_row() = delete;

        proxy_row(const proxy_row& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos)
        {
        }

        proxy_row(proxy_row&& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos)
        {
        }

        proxy_row(Data& data, const fsize_t pos) noexcept :
            m_data(data), m_pos(pos)
        {
        }

        proxy_row&
        operator=(const proxy_row& rhs) noexcept
        {
            copy_values(rhs);
            m_pos = rhs.m_pos;

            return *this;
        }

        proxy_row&
        operator=(proxy_row&& rhs) noexcept
        {
            copy_values(rhs);
            m_pos = rhs.m_pos;

            return *this;
        }

        bool
        operator==(const proxy_row rhs) const noexcept
        {
            return cp() == rhs.cp();
        }

        std::strong_ordering
        operator<=>(const proxy_row rhs) const noexcept
        {
            return cp() <=> rhs.cp();
        }

        template <typename Var>
        std::add_lvalue_reference_t<vartype<Var>>
        get() noexcept
        {
            return helper::template get<Var>(m_data)[m_pos];
        }

        std::add_lvalue_reference_t<vartype<_CP>>
        cp() noexcept
        {
            return get<_CP>();
        }

        friend void
        swap(proxy_row a, proxy_row b) noexcept
        {
            over(a.m_data,
                 [&](auto& c) noexcept { std::swap(c[a.m_pos], c[b.m_pos]); });
        }
    };

    template <fsize_t N>
    class proxy_view
    {
        Data&   m_data;
        fsize_t m_pos;

        fsize_t
        shift(fsize_t p) const noexcept
        {
            return m_pos + p;
        }

      public:
        proxy_view() = delete;
        proxy_view&
        operator=(const proxy_view& rhs) = delete;
        proxy_view&
        operator=(proxy_view&&) = delete;

        proxy_view(const proxy_view& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos)
        {
        }
        proxy_view(proxy_view&& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos){};

        proxy_view(Data& data, fsize_t pos) noexcept : m_data(data), m_pos(pos)
        {
        }

        proxy_row
        operator[](const fsize_t n) noexcept
        {
            return {m_data, shift(n)};
        }

        template <fsize_t N2>
            requires(N2 <= N)
        proxy_view<N2>
        view(const fsize_t pos = 0) noexcept
        {
            return {m_data, shift(pos)};
        }

        proxy_view<dynamic_extent>
        view(const fsize_t pos, const fsize_t ext) noexcept
        {
            return {m_data, shift(pos), ext};
        }

        proxy_view<dynamic_extent>
        view(const fsize_t pos = 0) noexcept
        {
            fsize_t np = shift(pos);
            return {m_data, np, size() - np};
        }

        template <typename Var>
        std::span<vartype<Var>, N>
        get() noexcept
        {
            return std::span<vartype<Var>, N>(
                std::addressof(helper::template get<Var>(m_data)[m_pos]),
                N);
        }

        std::span<vartype<_CP>, N>
        cp() noexcept
        {
            return get<_CP>();
        }

        constexpr fsize_t
        size() const noexcept
        {
            return N;
        }
    };

    template <>
    class proxy_view<dynamic_extent>
    {
        Data&   m_data;
        fsize_t m_pos;
        fsize_t m_ext;

        fsize_t
        shift(fsize_t p) const noexcept
        {
            return m_pos + p;
        }

      public:
        proxy_view() = delete;
        proxy_view&
        operator=(const proxy_view& rhs) = delete;
        proxy_view&
        operator=(proxy_view&&) = delete;

        proxy_view(const proxy_view& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext)
        {
        }
        proxy_view(proxy_view&& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext){};

        proxy_view(Data& data, fsize_t pos, fsize_t ext) noexcept :
            m_data(data), m_pos(pos), m_ext(ext)
        {
        }

        proxy_row
        operator[](const fsize_t n) noexcept
        {
            return {m_data, shift(n)};
        }

        template <fsize_t N>
        proxy_view<N>
        view(const fsize_t pos) noexcept
        {
            return {m_data, shift(pos)};
        }

        proxy_view<dynamic_extent>
        view(const fsize_t pos, const fsize_t ext) noexcept
        {
            return {m_data, shift(pos), ext};
        }

        proxy_view<dynamic_extent>
        view(const fsize_t pos = 0) noexcept
        {
            fsize_t np = shift(pos);
            return {m_data, np, size() - np};
        }

        template <typename Var>
        std::span<vartype<Var>, std::dynamic_extent>
        get() noexcept
        {
            return {std::addressof(helper::template get<Var>(m_data))[m_pos],
                    m_ext};
        }

        std::span<vartype<_CP>, std::dynamic_extent>
        cp() noexcept
        {
            return get<_CP>();
        }

        fsize_t
        size() const noexcept
        {
            return m_ext - m_pos;
        }
    };

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
            return {p_data, m_pos + rhs};
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
            return {p_data, m_pos - rhs};
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
        return {m_data, n};
    }

    template <fsize_t N>
    proxy_view<N>
    view(const fsize_t pos) noexcept
    {
        return {m_data, pos};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) noexcept
    {
        return {m_data, pos, ext};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos = 0) noexcept
    {
        return {m_data, pos, size() - pos};
    }

    iterator
    begin() noexcept
    {
        return {&m_data, 0ul};
    }

    iterator
    end() noexcept
    {
        return {&m_data, size()};
    }

    fsize_t
    size() noexcept
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
        over(m_data, [&](auto& c) noexcept { c.resize(new_size); });
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
struct packed_vector
{
    typedef T type __attribute__((vector_size(N * sizeof(T))));
};

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
using native_vector = typename packed_vector<T, native_vector_width<T>>::type;

namespace detail
{
template <fsize_t A, typename T, fsize_t... Is>
native_vector<T>
load_impl(T* src, std::index_sequence<Is...>) noexcept
{
    src                  = std::assume_aligned<A, T>(src);
    native_vector<T> ret = {src[Is]...};
    return ret;
}

template <typename T, fsize_t... Is>
constexpr native_vector<T>
broadcast_impl(T v, std::index_sequence<Is...>) noexcept
{
    native_vector<T> ret = {fake_get<Is>(v)...};
    return ret;
}
} // namespace detail

template <typename C>
constexpr inline fsize_t native_instruction_width =
    detail::native_instruction_width<C>::value;

/// @brief Load @p N values and pack them into vector of width @p N
/// from aligned memory location @p src.
/// @tparam A memory alignment.
template <fsize_t A, typename T>
native_vector<T>
load(T* src) noexcept
{
    return detail::load_impl<A>(
        src,
        std::make_index_sequence<native_vector_width<T>>{});
}

/// @brief Store values from packed vector into aligned memory location @p dst.
/// @tparam A memory alignment.
template <fsize_t A, typename T>
void
store(native_vector<T> src, T* dst) noexcept
{
    dst = std::assume_aligned<A, T>(dst);
    detail::store_impl(src,
                       dst,
                       std::make_index_sequence<native_vector_width<T>>{});
}

/// @brief Broadcast a value to all elements of the resulting packed vector.
template <typename T>
constexpr native_vector<T>
broadcast(T v) noexcept
{
    return detail::broadcast_impl(
        v,
        std::make_index_sequence<native_vector_width<T>>{});
}

template <typename Op>
struct remap_op;

// remaping std::plus, multiply, etc.
// they work with vector extensions
template <typename T, template <typename> typename Op>
struct remap_op<Op<T>>
{
    using type = Op<native_vector<T>>;
};

template <typename Op>
using remap_op_t = typename remap_op<Op>::type;

template <typename Cell,
          typename LabelMap,
          fsize_t alignment   = simd_alignment,
          fsize_t bulk_number = 8>
struct native_vertical_compiler
{
    using IdxVec = native_vector<index_t>;

    template <typename T>
    static void
    _store(native_vector<T> src, T* dst) noexcept
    {
        store<alignment>(src, dst);
    }

    template <typename T>
    static native_vector<T>
    _load(T* src) noexcept
    {
        return load<alignment>(src);
    }

    struct var_loader
    {
        template <typename Var,
                  template <fsize_t>
                  typename _ProxyView,
                  fsize_t W>
            requires(W == native_vector_width<vartype<Var>>)
        native_vector<vartype<Var>>
        operator()(_ProxyView<W> view) const noexcept
        {
            constexpr auto l = parse_literal<Var>();
            if constexpr(l.has_value())
                return broadcast(l.value());
            else
                return _load(view.template get<Var>().data());
        }
    };

    static constexpr var_loader loader{};

    template <typename C>
    struct eval;

    template <typename Dst, typename Src>
    struct eval<Copy<Dst, Src>>
    {
        using T = vartype<Dst>;

        template <template <fsize_t> typename _ProxyView, fsize_t W>
            requires(W == native_instruction_width<Copy<Dst, Src>>)
        void
        operator()(_ProxyView<W> view) const noexcept
        {
            T* dst = view.template get<Dst>().data();
            _store(loader.template operator()<Src>(), dst);
        }
    };

    template <typename Target>
    struct eval<Jump<Target>>
    {
        using constant = typename typelist::lookup<Target, LabelMap>;
        static constexpr index_t target = constant{}();

        template <template <fsize_t> typename _ProxyView, fsize_t W>
            requires(W == native_instruction_width<Jump<Target>>)
        void
        operator()(_ProxyView<W> view) const noexcept
        {
            index_t* dst = view.cp().data();
            IdxVec   t   = broadcast(target);
            _store(t, dst);
        }
    };

    template <typename Op, typename Dst, typename... Args>
    struct eval<Compute<Op, Dst, Args...>>
    {
        using T         = vartype<Dst>;
        using vector_op = remap_op_t<Op>;

        template <template <fsize_t> typename _ProxyView, fsize_t W>
            requires(W == native_instruction_width<Compute<Op, Dst, Args...>>)
        void
        operator()(_ProxyView<W> view) const noexcept
        {
            T*   dst = view.template get<Dst>().data();
            auto vars =
                typelist::maybe_collect(loader, type_sequence<Args...>{}, view);
            native_vector<T> ret = maybe_apply(vector_op{}, vars);
            _store(ret, dst);
        }
    };

    template <typename Var, typename Labels>
    struct eval<ParmJump<Var, Labels>>
    {
        static constexpr fsize_t iw =
            native_instruction_width<ParmJump<Var, Labels>>;

        template <typename Idxs>
        struct shuffler;

        template <index_t a, index_t b>
        struct shuffler<std::integer_sequence<index_t, a, b>>
        {
            static constexpr IdxVec va = broadcast(a);
            static constexpr IdxVec vb = broadcast(b);

            IdxVec
            operator()(IdxVec var) const noexcept
            {
                return var ? va : vb;
            }
        };

        template <index_t... is>
            requires(sizeof...(is) <= iw)
        struct shuffler<std::integer_sequence<index_t, is...>>
        {
            static constexpr IdxVec table = {is...};

            IdxVec
            operator()(IdxVec var) const noexcept
            {
                return _SHUFFLE(table, var);
            }
        };

        using idxs = pjmp_idxs<ParmJump<Var, Labels>, LabelMap>;
        static constexpr shuffler<idxs> op{};

        template <template <fsize_t> typename _ProxyView, fsize_t W>
            requires(W == iw)
        void
        operator()(_ProxyView<W> view) const noexcept
        {
            index_t* dst = view.cp().data();
            index_t* src = view.template get<Var>().data();
            _store(op(_load(src)), dst);
        }
    };

    template <typename C>
    struct vertical_runner
    {
        static constexpr fsize_t stride = native_instruction_width<C>;
        static constexpr eval<C> v_eval{};
        static constexpr standard_eval<C, LabelMap> s_eval{};
        using bulk_sizes =
            intlist::map_f<decltype([](int n) constexpr noexcept -> int
                                    { return n * stride; }),
                           intlist::reverse<intlist::powerof2s<static_cast<int>(
                               bulk_number)>>>;

        template <template <fsize_t> typename _MemoryView, fsize_t view_size>
            requires(view_size % stride == 0)
        static void
        run_view(_MemoryView<view_size> view) noexcept
        {
            for(fsize_t i = 0; i < view_size; i += stride)
                v_eval(view.template view<stride>(i));
        }

        template <fsize_t bulk_stride, typename _MemoryView>
        static void
        run_bulk(_MemoryView mem, fsize_t num_bulks) noexcept
        {
            for(fsize_t i = 0; i < num_bulks; i += bulk_stride)
                run_view(mem.template view<bulk_stride>(i));
        };

        template <typename _MemoryView, int first, int... rest>
        static fsize_t
        dispatch_bulks(_MemoryView mem,
                       std::integer_sequence<int, first, rest...>) noexcept
        {
            const int        s  = mem.size();
            const std::div_t qr = std::div(s, first);
            const fsize_t    q  = qr.quot;
            const fsize_t    r  = qr.rem;

            if(q) run_bulk<first>(mem, q);

            // reminder is zero - nothing left
            if(!r) return r;

            if constexpr(sizeof...(rest))
                return dispatch_bulks(mem.view(s - r, r),
                                      std::integer_sequence<int, rest...>{});

            return r;
        }

        template <typename _MemoryView>
        void
        operator()(_MemoryView mem) const noexcept
        {
            const fsize_t r = dispatch_bulks(mem, bulk_sizes{});
            if(r)
                for(fsize_t i = mem.size() - r; i < mem.size(); ++i)
                    s_eval(mem[i]);
        };
    };

    using instructions = typelist::map<vertical_runner, Cell>;

    template <typename _MemoryView>
    constexpr void
    operator()(_MemoryView memory) const noexcept
    {
        typelist::unroll(instructions{}, memory);
    }
};

template <typename _Program, fsize_t bulk_number = 8>
struct simd_evaluator
{
    using labelmap  = typename _Program::labelmap;
    using cells     = typename _Program::cells;
    using vars      = typename _Program::variables;
    using reg_names = typename _Program::reg_names;
    using reg_types = typename _Program::reg_types;

    template <typename Cell>
    using compile_cell =
        native_vertical_compiler<Cell, labelmap, simd_alignment, bulk_number>;

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
};
} // namespace lucid::simd
