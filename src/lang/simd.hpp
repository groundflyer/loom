// -*- C++ -*-
/// @file simd.hpp
/// SIMD evaluator.

#pragma once

#include "bse.hpp"
#include "bve.hpp"
#include "lang.hpp"

#include <utils/debug.hpp>
#include <utils/tmp.hpp>

#include <algorithm>
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
struct proxy_view_extent
{
    static constexpr fsize_t value = 1;
};

template <typename Data, typename Helper, typename CPVar, fsize_t N>
struct proxy_view_extent<bve::ProxyView<Data, Helper, CPVar, N>>
{
    static constexpr fsize_t value = N;
};

template <typename _ProxyView>
constexpr inline fsize_t pv_extent = proxy_view_extent<_ProxyView>::value;

template <typename T>
struct get_native_vector_align
{
#if defined(__AVX__) && defined(NARROW_AVX_INT)
    static constexpr fsize_t value = std::is_integral_v<T> ? 16 : simd_align;
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
using packed_vector = get_packed_vector<T, N>::type;

template <typename T>
struct native_vector_extent
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

template <typename I, typename index_t>
struct native_instruction_width;

template <typename Op,
          typename Dst,
          typename Arg1,
          typename... Args,
          typename index_t>
struct native_instruction_width<Compute<Op, Dst, Arg1, Args...>, index_t>
{
    static constexpr fsize_t value = native_vector_extent<vartype<Arg1>>::value;
};

template <typename Target, typename index_t>
struct native_instruction_width<Jump<Target>, index_t>
{
    static constexpr fsize_t value = native_vector_extent<index_t>::value;
};

template <typename Var, typename Labels, typename index_t>
struct native_instruction_width<ParmJump<Var, Labels>, index_t>
{
    static constexpr fsize_t value = native_vector_extent<index_t>::value;
};

template <typename T>
constexpr inline fsize_t native_vector_extent_v =
    native_vector_extent<T>::value;

template <typename T>
using native_vector = packed_vector<T, native_vector_extent_v<T>>;

template <typename I, typename index_t>
constexpr inline fsize_t native_instruction_width_v =
    native_instruction_width<I, index_t>::value;

/// @brief Load @p N values and pack them into vector of width @p N
/// from aligned memory location @p src.
template <fsize_t N, typename T>
packed_vector<T, N> _FN_ATTRS
load(const T* p) noexcept
{
    return *(packed_vector<T, N>*)p;
}

/// @brief Store values from packed vector into aligned memory location @p dst.
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

template <fsize_t N, typename Op, typename Ret, typename... Args>
struct remap_comp;

template <fsize_t N, typename Dst, typename Src>
struct remap_comp<N, std::identity, Dst, Src>
{
    struct type
    {
        constexpr packed_vector<Dst, N>
        operator()(const packed_vector<Src, N> src) const noexcept
        {
            if constexpr(std::is_same_v<Dst, Src>)
                return src;
            else
                return __builtin_convertvector(src, packed_vector<Dst, N>);
        }
    };
};

template <fsize_t N, typename T, typename I>
struct remap_comp<N, choice_op, T, I, T, T>
{
    struct type
    {
        constexpr packed_vector<T, N>
        operator()(const packed_vector<I, N> c,
                   const packed_vector<T, N> a,
                   const packed_vector<T, N> b) const noexcept
        {
            return c ? a : b;
        }
    };
};

#define DEF_REMAP_VEC_BIN(func, op, R)                             \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, func<T>, R, T, T>                         \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<R, N>                          \
            operator()(const packed_vector<T, N> a,                \
                       const packed_vector<T, N> b) const noexcept \
            {                                                      \
                return a op b;                                     \
            }                                                      \
        };                                                         \
    };

#define DEF_REMAP_VEC_UN(func, op, R)                              \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, func<T>, R, T>                            \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<R, N>                          \
            operator()(const packed_vector<T, N> a) const noexcept \
            {                                                      \
                return op a;                                       \
            }                                                      \
        };                                                         \
    };

#define DEF_REMAP_TVEC_BIN(func, op) DEF_REMAP_VEC_BIN(func, op, T)
#define DEF_REMAP_IVEC_BIN(func, op) DEF_REMAP_VEC_BIN(func, op, IDX_TYPE)

#define DEF_REMAP_TVEC_UN(func, op) DEF_REMAP_VEC_UN(func, op, T)
#define DEF_REMAP_IVEC_UN(func, op)  DEF_REMAP_VEC_UN(func, op, IDX_TYPE)

DEF_REMAP_TVEC_BIN(std::plus, +)
DEF_REMAP_TVEC_BIN(std::minus, -)
DEF_REMAP_TVEC_BIN(std::multiplies, *)
DEF_REMAP_TVEC_BIN(std::divides, /)
DEF_REMAP_TVEC_BIN(std::modulus, %)
DEF_REMAP_TVEC_UN(std::negate, -)

DEF_REMAP_IVEC_BIN(std::equal_to, ==)
DEF_REMAP_IVEC_BIN(std::not_equal_to, !=)
DEF_REMAP_IVEC_BIN(std::greater, >)
DEF_REMAP_IVEC_BIN(std::less, <)
DEF_REMAP_IVEC_BIN(std::greater_equal, >=)
DEF_REMAP_IVEC_BIN(std::less_equal, <=)

DEF_REMAP_IVEC_BIN(std::logical_and, &&)
DEF_REMAP_IVEC_BIN(std::logical_or, ||)
DEF_REMAP_IVEC_UN(std::logical_not, !)

DEF_REMAP_IVEC_BIN(lshift_op, <<)
DEF_REMAP_IVEC_BIN(rshift_op, >>)

#undef DEF_REMAP_IVEC_UN
#undef DEF_REMAP_TVEC_UN
#undef DEF_REMAP_IVEC_BIN
#undef DEF_REMAP_TVEC_BIN
#undef DEF_REMAP_VEC_UN
#undef DEF_REMAP_IVEC_BIN

template <fsize_t N, typename Op, typename Ret, typename... Args>
using remap_comp_t = remap_comp<N, Op, Ret, Args...>::type;

template <typename T, fsize_t N>
struct vectorize_union
{
    using type = packed_vector<T, N>;
};

template <typename... Ts, fsize_t N>
struct vectorize_union<Union<Ts...>, N>
{
    using type = Union<packed_vector<Ts, N>...>;
};

template <typename RegTypes, typename RegNames, typename CPVar, fsize_t N>
class KernelMemory
{
    template <typename T>
    using reg_proj = vectorize_union<T, N>::type;

    template <typename T>
    using vec = packed_vector<T, N>;

    using helper = MemoryHelper<typelist::map<reg_proj, RegTypes>, RegNames>;

    template <typename Var>
    using vec_var = Variable<vec<vartype<Var>>, varname<Var>>;

    using Data = helper::data;

    Data m_data;

  public:
    template <typename Var>
    std::add_lvalue_reference_t<vec<vartype<Var>>>
    get() noexcept
    {
        return helper::template get<vec_var<Var>>(m_data);
    }

    std::add_lvalue_reference_t<vec<vartype<CPVar>>>
    cp() noexcept
    {
        return get<CPVar>();
    }
};

template <fsize_t N>
struct var_loader
{
    template <typename Var, typename _Memory>
    decltype(auto)
    operator()(_Memory&& mem) const noexcept
    {
        constexpr auto l = parse_literal<Var>();
        if constexpr(l.has_value())
            return broadcast<N>(l.value());
        else if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            return load<N>(
                std::forward<_Memory>(mem).template get<Var>().data());
        else
            return std::forward<_Memory>(mem).template get<Var>();
    }
};

template <typename I, typename LabelMap, fsize_t N>
class eval;

template <typename Target, typename LabelMap, fsize_t N>
class eval<Jump<Target>, LabelMap, N>
{
    using constant                  = typelist::lookup<Target, LabelMap>;
    using index_t                   = get_index_t<LabelMap>;
    static constexpr index_t target = constant{}();

    packed_vector<index_t, N>
    get() const noexcept
    {
        return broadcast<N>(target);
    }

  public:
    template <typename _Memory>
    void
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).cp().data(), get());
        else
            std::forward<_Memory>(mem).cp() = get();
    }
};

template <typename Op,
          typename Dst,
          typename... Args,
          typename LabelMap,
          fsize_t N>
    requires(... && (sizeof(vartype<Dst>) == sizeof(vartype<Args>)))
class eval<Compute<Op, Dst, Args...>, LabelMap, N>
{
    using T         = vartype<Dst>;
    using index_t   = get_index_t<LabelMap>;
    using vector_op = remap_comp_t<N, Op, vartype<Dst>, vartype<Args>...>;
    using loader_t  = var_loader<N>;
    static constexpr loader_t loader{};

    template <typename _Memory>
    packed_vector<T, N>
    compute(_Memory&& mem) const noexcept
    {
        auto vars = typelist::maybe_collect(loader,
                                            type_sequence<Args...>{},
                                            std::forward<_Memory>(mem));
        return maybe_apply(vector_op{}, vars);
    }

  public:
    template <typename _Memory>
    void
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).template get<Dst>().data(),
                     compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<Dst>() =
                compute(std::forward<_Memory>(mem));
    }
};

template <typename Idxs, fsize_t N>
struct shuffler;

template <typename index_t, index_t a, index_t b, fsize_t N>
struct shuffler<std::integer_sequence<index_t, a, b>, N>
{
    using IdxVec = packed_vector<index_t, N>;

    static constexpr IdxVec va = broadcast<N>(a);
    static constexpr IdxVec vb = broadcast<N>(b);

    IdxVec
    operator()(IdxVec var) const noexcept
    {
        return var ? vb : va;
    }
};

template <typename index_t, index_t... is, fsize_t N>
    requires(sizeof...(is) <= N)
struct shuffler<std::integer_sequence<index_t, is...>, N>
{
    using IdxVec                  = packed_vector<index_t, N>;
    static constexpr IdxVec table = {is...};

    IdxVec
    operator()(IdxVec var) const noexcept
    {
        return _SHUFFLE(table, var);
    }
};

template <typename Var, typename Labels, typename LabelMap, fsize_t N>
class eval<ParmJump<Var, Labels>, LabelMap, N>
{
    using index_t = get_index_t<LabelMap>;
    using idxs    = pjmp_idxs<ParmJump<Var, Labels>, LabelMap>;
    static constexpr shuffler<idxs, N> op{};

  public:
    template <typename _Memory>
    void
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
        {
            index_t* dst = std::forward<_Memory>(mem).cp().data();
            index_t* src =
                std::forward<_Memory>(mem).template get<Var>().data();
            store<N>(dst, op(load<N>(src)));
        }
        else
            std::forward<_Memory>(mem).cp() =
                op(std::forward<_Memory>(mem).template get<Var>());
    }
};

template <typename SIMDRunner,
          typename SerialRunner,
          fsize_t N,
          fsize_t bulk_number>
class BulkDispatcher
{
    using bulk_sizes =
        intlist::map_f<decltype([](fsize_t n) constexpr noexcept -> fsize_t
                                { return n * N; }),
                       intlist::reverse<intlist::powerof2s<bulk_number>>>;

    static constexpr bulk_sizes   seq{};
    static constexpr SIMDRunner   simd_runner{};
    static constexpr SerialRunner serial_runner{};

    /// @brief Compute aligned offset for the next view.
    ///
    /// @param stride SIMD width.
    template <typename _MemoryView>
    static fsize_t
    next_offset(_MemoryView view) noexcept
    {
        // ensure that we have aligned access to the view
        const fsize_t r  = view.offset() % N; // remainder
        const fsize_t sr = N - r;
        const fsize_t vs = view.size();
        fsize_t       no = std::min(sr, vs); // new offset
        // if reminder doesn't fit SIMD lane we execute it here too
        if(vs - no < N) no = vs;
        return no;
    }

    template <typename _MemoryView>
        requires(pv_extent<_MemoryView> % N == 0 &&
                 pv_extent<_MemoryView> != dynamic_extent)
    static void
    run_bulk(_MemoryView view) noexcept
    {
        constexpr fsize_t num_views = pv_extent<_MemoryView> / N;
        for(fsize_t i = 0; i < num_views; ++i)
            simd_runner(view.template view<N>(i * N));
    }

    template <fsize_t bulk_stride, typename _MemoryView>
    static void
    run_bulks(_MemoryView view, fsize_t num_bulks) noexcept
    {
        for(fsize_t i = 0; i < num_bulks; ++i)
            run_bulk(view.template view<bulk_stride>(i * bulk_stride));
    }

    template <typename _MemoryView, fsize_t bulk_stride, fsize_t... rest>
    void
    dispatch(
        _MemoryView view,
        std::integer_sequence<fsize_t, bulk_stride, rest...>) const noexcept
    {
        const fsize_t s = view.size();
        const fsize_t q = s / bulk_stride;
        const fsize_t r = s % bulk_stride;

        if(q) run_bulks<bulk_stride>(view, q);

        // reminder is zero - nothing left
        if(!r) return;

        if constexpr(sizeof...(rest))
            return dispatch(view.view(s - r),
                            std::integer_sequence<fsize_t, rest...>{});

        // execute remained serially
        for(fsize_t i = view.size() - r; i < view.size(); ++i)
            serial_runner(view[i]);
    }

  public:
    template <typename _MemoryView>
    void
    operator()(_MemoryView view) const noexcept
    {
        const fsize_t no = next_offset(view);
        // non-aligned rows executed serially
        for(fsize_t i = 0; i < no; ++i) serial_runner(view[i]);

        // we have reavhed the end of the view
        if(view.size() == no) return;

        auto new_view = view.view(no);
        ASSERT(new_view.offset() % N == 0 || new_view.size() < N,
               "Unaligned access");
        dispatch(new_view, seq);
    }
};

/// @brief Executes a single instruction @p I through the whole view.
template <typename I, typename LabelMap, typename CPVar, fsize_t bulk_number>
class make_vertical_instruction_runner
{
    static constexpr fsize_t N = native_instruction_width_v<I, vartype<CPVar>>;

  public:
    using type = BulkDispatcher<eval<I, LabelMap, N>,
                                standard_eval<I, LabelMap>,
                                N,
                                bulk_number>;
};

template <typename Cell, typename LabelMap, typename CPVar, fsize_t bulk_number>
class make_vertical_runner
{
    template <typename I, typename _LabelMap>
    using eval =
        make_vertical_instruction_runner<I, _LabelMap, CPVar, bulk_number>::
            type;

  public:
    using type = standard_compiler<Cell, LabelMap, eval>;
};

/// @brief Executes @p Cell as SIMD kernel.
template <typename Cell, typename LabelMap, typename CPVar, fsize_t bulk_number>
class make_minikernel_runner
{
    template <typename Var>
    using p = std::negation<is_literal<Var>>;

    using vars    = typelist::filter<p, cell_vars_t<Cell, CPVar>>;
    using index_t = vartype<CPVar>;

    template <typename I>
    using f = native_instruction_width<I, index_t>;

    // SIMD width/stride
    static constexpr fsize_t N =
        *std::ranges::min_element(intlist::stdarray<typelist::map<f, Cell>>);

    using reg_names = typelist::nub<typelist::map<varname, vars>>;
    using reg_types = infer_reg_types_t<vars, reg_names>;

    using load_vars  = load_list_t<Cell, CPVar>;
    using store_vars = store_list_t<Cell, CPVar>;

    class SerialRunner
    {
        using Kernel  = standard_compiler<Cell, LabelMap>;
        using Storage = bse::Memory<reg_types, reg_names, CPVar>;
        static constexpr Kernel kernel{};

        template <typename _ProxyRow>
        static Storage
        alloc(_ProxyRow row) noexcept
        {
            Storage storage;
            typelist::for_each(
                []<typename Var>(_ProxyRow row, Storage& storage) noexcept
                { storage.template get<Var>() = row.template get<Var>(); },
                load_vars{},
                row,
                storage);
            return storage;
        }

        template <typename _ProxyRow>
        static void
        spill(_ProxyRow row, Storage& storage) noexcept
        {
            typelist::for_each(
                []<typename Var>(_ProxyRow row, Storage& storage) noexcept
                { row.template get<Var>() = storage.template get<Var>(); },
                store_vars{},
                row,
                storage);
        }

      public:
        template <typename _ProxyRow>
        void
        operator()(_ProxyRow row) const noexcept
        {
            Storage storage = alloc(row);
            kernel(storage);
            spill(row, storage);
        }
    };

    class SIMDRunner
    {
        template <typename I, typename _LabelMap>
        using simd_eval = eval<I, _LabelMap, N>;

        using Kernel  = standard_compiler<Cell, LabelMap, simd_eval>;
        using Storage = KernelMemory<reg_types, reg_names, CPVar, N>;

        static constexpr Kernel kernel{};

        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        static Storage
        alloc(_MemoryView view) noexcept
        {
            Storage storage;
            typelist::for_each(
                []<typename Var>(_MemoryView view, Storage& storage) noexcept {
                    storage.template get<Var>() =
                        load<N>(view.template get<Var>().data());
                },
                load_vars{},
                view,
                storage);
            return storage;
        }

        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        static void
        spill(_MemoryView view, Storage& storage) noexcept
        {
            typelist::for_each(
                []<typename Var>(_MemoryView view, Storage& storage) noexcept {
                    store<N>(view.template get<Var>().data(),
                             storage.template get<Var>());
                },
                store_vars{},
                view,
                storage);
        }

      public:
        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        void
        operator()(_MemoryView view) const noexcept
        {
            Storage storage = alloc(view);
            kernel(storage);
            spill(view, storage);
        }
    };

  public:
    using type = BulkDispatcher<SIMDRunner, SerialRunner, N, bulk_number>;
};

template <typename Cell, typename LabelMap, typename CPVar, fsize_t bulk_number>
using vertical_runner =
    make_vertical_runner<Cell, LabelMap, CPVar, bulk_number>::type;

template <typename Cell, typename LabelMap, typename CPVar, fsize_t bulk_number>
using minikernel_runner =
    make_minikernel_runner<Cell, LabelMap, CPVar, bulk_number>::type;

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
        while(i < cps.size() && cps[i] == cp) ++i;
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

template <typename _Program, template <typename...> typename Runner>
class simd_evaluator
{
    using labelmap  = _Program::labelmap;
    using cells     = _Program::cells;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using cp        = _Program::cp;

    static constexpr auto    halt_pos = _Program::halt_pos;
    static constexpr fsize_t align    = simd_align;

    template <typename Cell>
    using compile_cell = Runner<Cell, labelmap, cp>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    using facade =
        bve::MemoryFacade<_Program, std::vector, SIMDAlignedAllocator>;
    using memory = bve::
        Memory<reg_types, reg_names, cp, std::vector, SIMDAlignedAllocator>;

    class single_cell_evaluator : public facade
    {
        static constexpr typelist::head<compiled_cells> cell{};

      public:
        template <typename... Args>
        constexpr auto
        operator()(fsize_t n, Args&&... args) noexcept
        {
            facade::resize(n);
            facade::set_input(std::forward<Args>(args)...);
            facade::step([](memory& mem) noexcept { cell(mem.view()); });
            return facade::get_output();
        }
    };

    class evaluator : public facade
    {
        static constexpr typelist::to<compiled_cells, std::tuple> cells{};

      public:
        template <typename... Args>
        constexpr auto
        operator()(fsize_t n, Args&&... args) noexcept
        {
            facade::resize(n);
            facade::set_input(std::forward<Args>(args)...);

            while(!facade::done())
                facade::step(
                    [](memory& mem) noexcept
                    {
                        std::ranges::for_each(memory_chunks(mem),
                                              [](auto view) noexcept
                                              {
                                                  visit_at(
                                                      view.cp().front(),
                                                      [&](auto cell) noexcept
                                                      { cell(view); },
                                                      cells,
                                                      sink);
                                              });

                        std::sort(mem.begin(), mem.end());
                    });

            return facade::get_output();
        }
    };

  public:
    using type = std::
        conditional_t<_Program::singular, single_cell_evaluator, evaluator>;
};

template <typename _Program, bool use_minikernel, fsize_t bulk_number>
    requires(bulk_number > 0)
class choose_evaluator
{
    template <typename Cell, typename LabelMap, typename CPVar>
    using runner = std::conditional_t<
        use_minikernel,
        minikernel_runner<Cell, LabelMap, CPVar, bulk_number>,
        vertical_runner<Cell, LabelMap, CPVar, bulk_number>>;

  public:
    using type = simd_evaluator<_Program, runner>::type;
};

template <typename _Program,
          bool    use_minikernel = false,
          fsize_t bulk_number    = 8>
using evaluator = choose_evaluator<_Program, use_minikernel, bulk_number>::type;
} // namespace lucid::simd
