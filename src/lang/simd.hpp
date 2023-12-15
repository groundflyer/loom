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
#include <fmt/ranges.h>
#endif

#include <cstdlib>
#include <memory>

#if defined(__clang__)
#define _SHUFFLE  __builtin_shufflevector
#define _FN_ATTRS __attribute__((always_inline, nodebug))
#elif defined(__GNUG__)
#define _SHUFFLE __builtin_shuffle
#define _FN_ATTRS
#endif

namespace lucid::simd
{
using fsize_t = bve::fsize_t;

constexpr inline fsize_t dynamic_extent = bve::dynamic_extent;

#if defined(__AVX512F__)
constexpr inline fsize_t simd_width = 64; // 512
constexpr inline fsize_t simd_align = 64;
#elif defined(__AVX2__) || defined(__AVX__)
constexpr inline fsize_t simd_width = 32; // 256
constexpr inline fsize_t simd_align = 32;
#else
constexpr inline fsize_t simd_width = 16; // 128
constexpr inline fsize_t simd_align = 16;
#endif

template <typename _ProxyView>
struct proxy_view_extent;

template <typename Data, typename Helper, typename _CP, fsize_t N>
struct proxy_view_extent<bve::ProxyView<Data, Helper, _CP, N>>
{
    static constexpr fsize_t value = N;
};

template <typename _ProxyView>
constexpr inline fsize_t pv_extent = proxy_view_extent<_ProxyView>::value;

template <typename T>
struct get_native_vector_align
{
#if defined(__AVX__) && defined(NARROW_AVX_INT)
    static constexpr fsize_t value =
        std::is_integral_v<T> ? alignof(T) : simd_align;
#else
    static constexpr fsize_t value  = simd_align;
#endif
};

template <typename T, fsize_t N>
struct get_packed_vector
{
    typedef T type __attribute__((vector_size(N * sizeof(T)),
                                  aligned(get_native_vector_align<T>::value)));
};

template <typename T, fsize_t N>
using packed_vector = typename get_packed_vector<T, N>::type;

namespace detail
{
template <typename T>
struct get_native_vector_extent
{
  private:
#if defined(__AVX__) && defined(NARROW_AVX_INT)
    static constexpr fsize_t _width = std::is_integral_v<T> ? 16 : simd_width;
#else
    static constexpr fsize_t _width = simd_width;
#endif
  public:
    static constexpr fsize_t value = _width / sizeof(T);
};

template <typename C, typename index_t>
struct native_instruction_width;

template <typename Dst, typename Src, typename index_t>
struct native_instruction_width<Copy<Dst, Src>, index_t>
{
    static constexpr fsize_t value =
        get_native_vector_extent<vartype<Dst>>::value;
};

template <typename Op,
          typename Dst,
          typename Arg1,
          typename... Args,
          typename index_t>
struct native_instruction_width<Compute<Op, Dst, Arg1, Args...>, index_t>
{
    static constexpr fsize_t value =
        get_native_vector_extent<vartype<Arg1>>::value;
};

template <typename Target, typename index_t>
struct native_instruction_width<Jump<Target>, index_t>
{
    static constexpr fsize_t value = get_native_vector_extent<index_t>::value;
};

template <typename Var, typename Labels, typename index_t>
struct native_instruction_width<ParmJump<Var, Labels>, index_t>
{
    static constexpr fsize_t value = get_native_vector_extent<index_t>::value;
};
} // namespace detail

template <typename T>
constexpr inline fsize_t native_vector_extent =
    detail::get_native_vector_extent<T>::value;

template <typename T>
using native_vector = packed_vector<T, native_vector_extent<T>>;

template <typename C, typename index_t>
constexpr inline fsize_t native_instruction_width =
    detail::native_instruction_width<C, index_t>::value;

/// @brief Load @p N values and pack them into vector of width @p N
/// from aligned memory location @p src.
/// @tparam A memory alignment.
template <fsize_t N, typename T>
packed_vector<T, N> _FN_ATTRS
load(const T* p) noexcept
{
    return *(packed_vector<T, N>*)p;
}

/// @brief Store values from packed vector into aligned memory location @p dst.
/// @tparam A memory alignment.
template <fsize_t N, typename T>
void _FN_ATTRS
store(T* dst, packed_vector<T, N> src) noexcept
{
    *(packed_vector<T, N>*)dst = src;
}

/// @brief Broadcast a value to all elements of the resulting packed vector.
template <fsize_t N, typename T>
constexpr packed_vector<T, N>
broadcast(const T v) noexcept
{
    return std::apply(maker<packed_vector<T, N>, true>, repeat_to_tuple<N>(v));
}

template <fsize_t N, typename Op>
struct remap_op;

#define DEF_REMAP_VEC_CMP(func, op)                          \
    template <fsize_t N, typename T>                         \
    struct remap_op<N, func<T>>                              \
    {                                                        \
        struct type                                          \
        {                                                    \
            constexpr packed_vector<infer_index_t<T>, N>     \
            operator()(packed_vector<T, N> a,                \
                       packed_vector<T, N> b) const noexcept \
            {                                                \
                return a op b;                               \
            }                                                \
        };                                                   \
    };

DEF_REMAP_VEC_CMP(std::equal_to, ==)
DEF_REMAP_VEC_CMP(std::not_equal_to, !=)
DEF_REMAP_VEC_CMP(std::greater, >)
DEF_REMAP_VEC_CMP(std::less, <)
DEF_REMAP_VEC_CMP(std::greater_equal, >=)
DEF_REMAP_VEC_CMP(std::less_equal, <=)

// remaping std::plus, multiply, etc.
// they work with vector extensions
template <fsize_t N, typename T, template <typename> typename Op>
struct remap_op<N, Op<T>>
{
    using type = Op<packed_vector<T, N>>;
};

template <fsize_t N, typename Op>
using remap_op_t = typename remap_op<N, Op>::type;

template <fsize_t N>
struct var_loader
{
    template <typename Var, typename _ProxyView>
        requires(pv_extent<_ProxyView> == N)
    packed_vector<vartype<Var>, N>
    operator()(_ProxyView view) const noexcept
    {
        constexpr auto l = parse_literal<Var>();
        if constexpr(l.has_value())
            return broadcast<N>(l.value());
        else
            return load<N>(view.template get<Var>().data());
    }
};

template <typename C, typename LabelMap>
struct eval;

template <typename Dst, typename Src, typename LabelMap>
struct eval<Copy<Dst, Src>, LabelMap>
{
    using T       = vartype<Dst>;
    using index_t = get_index_t<LabelMap>;
    static constexpr fsize_t n =
        native_instruction_width<Copy<Dst, Src>, index_t>;
    using loader_t = var_loader<n>;
    static constexpr loader_t loader{};

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == n)
    void
    operator()(_ProxyView view) const noexcept
    {
        T* dst = view.template get<Dst>().data();
        if constexpr(std::is_same_v<vartype<Dst>, vartype<Src>>)
            store<n>(dst, loader.template operator()<Src>(view));
        else
            store<n>(
                dst,
                __builtin_convertvector(loader.template operator()<Src>(view),
                                        packed_vector<T, n>));
    }
};

template <typename Target, typename LabelMap>
struct eval<Jump<Target>, LabelMap>
{
    using constant = typename typelist::lookup<Target, LabelMap>;
    using index_t  = get_index_t<LabelMap>;
    static constexpr index_t target = constant{}();
    static constexpr fsize_t n =
        native_instruction_width<Jump<Target>, index_t>;

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == n)
    void
    operator()(_ProxyView view) const noexcept
    {
        index_t*                  dst = view.cp().data();
        packed_vector<index_t, n> t   = broadcast<n>(target);
        store<n>(dst, t);
    }
};

template <typename Op, typename Dst, typename... Args, typename LabelMap>
    requires(... && (sizeof(vartype<Dst>) == sizeof(vartype<Args>)))
struct eval<Compute<Op, Dst, Args...>, LabelMap>
{
    using T       = vartype<Dst>;
    using index_t = get_index_t<LabelMap>;
    static constexpr fsize_t n =
        native_instruction_width<Compute<Op, Dst, Args...>, index_t>;
    using vector_op = remap_op_t<n, Op>;
    using loader_t  = var_loader<n>;
    static constexpr loader_t loader{};

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == n)
    void
    operator()(_ProxyView view) const noexcept
    {
        T*   dst = view.template get<Dst>().data();
        auto vars =
            typelist::maybe_collect(loader, type_sequence<Args...>{}, view);
        packed_vector<T, n> ret = maybe_apply(vector_op{}, vars);
        store<n>(dst, ret);
    }
};

template <typename Idxs, fsize_t n>
struct shuffler;

template <typename index_t, index_t a, index_t b, fsize_t n>
struct shuffler<std::integer_sequence<index_t, a, b>, n>
{
    using IdxVec = packed_vector<index_t, n>;

    static constexpr IdxVec va = broadcast<n>(a);
    static constexpr IdxVec vb = broadcast<n>(b);

    IdxVec
    operator()(IdxVec var) const noexcept
    {
        return var ? vb : va;
    }
};

template <typename index_t, index_t... is, fsize_t n>
    requires(sizeof...(is) <= n)
struct shuffler<std::integer_sequence<index_t, is...>, n>
{
    using IdxVec                  = packed_vector<index_t, n>;
    static constexpr IdxVec table = {is...};

    IdxVec
    operator()(IdxVec var) const noexcept
    {
        return _SHUFFLE(table, var);
    }
};

template <typename Var, typename Labels, typename LabelMap>
struct eval<ParmJump<Var, Labels>, LabelMap>
{
    using index_t = get_index_t<LabelMap>;
    static constexpr fsize_t n =
        native_instruction_width<ParmJump<Var, Labels>, index_t>;

    using idxs = pjmp_idxs<ParmJump<Var, Labels>, LabelMap>;
    static constexpr shuffler<idxs, n> op{};

    template <typename _ProxyView>
        requires(pv_extent<_ProxyView> == n)
    void
    operator()(_ProxyView view) const noexcept
    {
        index_t* dst = view.cp().data();
        index_t* src = view.template get<Var>().data();
        store<n>(dst, op(load<n>(src)));
    }
};

template <typename C, typename LabelMap, fsize_t bulk_number>
struct vertical_runner
{
    using index_t                   = get_index_t<LabelMap>;
    static constexpr fsize_t stride = native_instruction_width<C, index_t>;
    static constexpr eval<C, LabelMap>          v_eval{};
    static constexpr standard_eval<C, LabelMap> s_eval{};
    using bulk_sizes =
        intlist::map_f<decltype([](fsize_t n) constexpr noexcept -> fsize_t
                                { return n * stride; }),
                       intlist::reverse<intlist::powerof2s<bulk_number>>>;

    template <typename _MemoryView>
        requires(pv_extent<_MemoryView> % stride == 0 &&
                 pv_extent<_MemoryView> != dynamic_extent)
    static void
    run_bulk(_MemoryView view) noexcept
    {
        constexpr fsize_t num_views = pv_extent<_MemoryView> / stride;
        for(fsize_t i = 0; i < num_views; ++i)
            v_eval(view.template view<stride>(i * stride));
    }

    template <fsize_t bulk_stride, typename _MemoryView>
    static void
    run_bulks(_MemoryView view, fsize_t num_bulks) noexcept
    {
        for(fsize_t i = 0; i < num_bulks; ++i)
            run_bulk(view.template view<bulk_stride>(i * bulk_stride));
    }

    template <typename _MemoryView, fsize_t bulk_stride, fsize_t... rest>
    static void
    dispatch_bulks(
        _MemoryView view,
        std::integer_sequence<fsize_t, bulk_stride, rest...>) noexcept
    {
        const fsize_t s = view.size();
        const fsize_t q = s / bulk_stride;
        const fsize_t r = s % bulk_stride;

        if(q) run_bulks<bulk_stride>(view, q);

        // reminder is zero - nothing left
        if(!r) return;

        if constexpr(sizeof...(rest))
            return dispatch_bulks(view.view(s - r),
                                  std::integer_sequence<fsize_t, rest...>{});

        // evaluate remained sequentially
        for(fsize_t i = view.size() - r; i < view.size(); ++i) s_eval(view[i]);
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

    template <typename index_t>
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

template <typename T>
class SIMDAlignedAllocator
{
    static constexpr fsize_t align = get_native_vector_align<T>::value;

    static constexpr fsize_t
    align_to(fsize_t n) noexcept
    {
        const fsize_t m = n % align;
        return m == 0 ? n : n + align - m;
    }

  public:
    using value_type = T;
    using pointer    = T*;

    SIMDAlignedAllocator() noexcept = default;

    pointer
    allocate(fsize_t n)
    {
        void* p = std::aligned_alloc(align, align_to(sizeof(T) * n));
        if(!p) throw std::bad_alloc();

        return static_cast<pointer>(p);
    }

    void
    deallocate(pointer p, fsize_t)
    {
        std::free(p);
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
    using cp        = typename _Program::cp;

    static constexpr auto    end_pos = _Program::end_pos;
    static constexpr fsize_t align   = simd_align;

    template <typename C, typename LabelMap>
    using simd_eval = vertical_runner<C, LabelMap, bulk_number>;

    template <typename Cell>
    using compile_cell = standard_compiler<Cell, labelmap, simd_eval>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    static constexpr bve::input_setter<typename _Program::input_vars> setter{};
    static constexpr bve::output_getter<typename _Program::output_vars>
        getter{};

    using memory = bve::
        Memory<reg_types, reg_names, cp, std::vector, SIMDAlignedAllocator>;

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

            const auto cps = mem.view().cp();
            while(cps.front() != end_pos)
            {
                std::ranges::for_each(memory_chunks(mem),
                                      [](auto view) noexcept
                                      {
                                          visit(
                                              view.cp().front(),
                                              [&](auto cell) noexcept
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
                    constexpr unsigned _w = 20;
                    typelist::for_each(
                        [_w]<typename Name>() noexcept
                        {
                            constexpr std::string_view n =
                                std::string_view(intlist::array<Name>,
                                                 intlist::length<Name>);
                            fmt::print("{:>{}} |", n, _w);
                        },
                        reg_names{});
                    fmt::print("\n");
                    typelist::for_each([_w]<typename Name>() noexcept
                                       { fmt::print("{:*>{}} |", '*', _w); },
                                       reg_names{});
                    fmt::print("\n");
                    std::ranges::for_each(mem,
                                          [_w](auto p) noexcept
                                          { print(p, _w); });
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
