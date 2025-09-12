// -*- C++ -*-
/// @file simd.hpp
/// @brief SIMD backend implementation.

#pragma once

#include "bse.hpp"
#include "bve.hpp"
#include "core.hpp"
#include "macros.hpp"
#include "operators.hpp"

#include <utils/debug.hpp>
#include <utils/memory.hpp>
#include <utils/tmp.hpp>
#include <utils/tuple.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <cstdlib>

#include <immintrin.h>

#if defined(__clang__)
#define _SHUFFLE      __builtin_shufflevector
#define _FORCE_INLINE __attribute__((always_inline, nodebug))
#elif defined(__GNUG__)
#define _SHUFFLE __builtin_shuffle
#define _FORCE_INLINE
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

void
print_info() noexcept
{
#if defined(__AVX512F__)
    constexpr char info[] = "avx512f";
#elif defined(__AVX2__)
    constexpr char info[] = "avx2";
#elif defined(__AVX__)
    constexpr char info[] = "avx";
#else
    constexpr char info[] = "sse4.2 or older";
#endif
    fmt::print("Using x86-64 vector extensions: {}\n", info);
}

template <typename _ProxyView>
struct proxy_view_extent
{
    static constexpr fsize_t value = 1;
};

template <typename Data, typename Helper, typename BPVar, fsize_t N>
struct proxy_view_extent<bve::ProxyView<Data, Helper, BPVar, N>>
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
    static constexpr fsize_t value = simd_align;
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
packed_vector<T, N> _FORCE_INLINE
load(T const* p) noexcept
{
    return *(packed_vector<T, N>*)p;
}

/// @brief Store values from packed vector into aligned memory location @p dst.
template <fsize_t N, typename T>
void _FORCE_INLINE
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

/// @brief Loads packed vector with a mask.
template <typename T, fsize_t N>
struct mask_io_intrinsics_wrangler
{
    using vector_type = packed_vector<T, N>;
    using mask_type   = packed_vector<int_of_size_t<sizeof(T)>, N>;

    /// @brief Fallback implementation for mask load.
    static vector_type _FORCE_INLINE
    load(T const* p, const mask_type mask) noexcept
    {
        return mask ? simd::load<N>(p) : broadcast<N>(static_cast<T>(0));
    }

    /// @brief Fallback implementation for mask store.
    static void _FORCE_INLINE
    store(T* dest, const mask_type mask, const vector_type value) noexcept
    {
        simd::store<N>(dest, mask ? value : simd::load<N>(dest));
    }
};

#define MK_MASK_IO_INTRINSICS_WRANGLER(TYPE, EXT, LOAD_INTR, STORE_INTR)     \
    template <>                                                              \
    struct mask_io_intrinsics_wrangler<TYPE, EXT>                            \
    {                                                                        \
        using vector_type = packed_vector<TYPE, EXT>;                        \
        using mask_type   = packed_vector<int_of_size_t<sizeof(TYPE)>, EXT>; \
        static vector_type _FORCE_INLINE                                     \
        load(TYPE const* p, const mask_type mask) noexcept                   \
        {                                                                    \
            return LOAD_INTR(p, mask);                                       \
        }                                                                    \
        static void _FORCE_INLINE                                            \
        store(TYPE*             dest,                                        \
              const mask_type   mask,                                        \
              const vector_type value) noexcept                              \
        {                                                                    \
            STORE_INTR(dest, mask, value);                                   \
        }                                                                    \
    };

// assume that a CPU with newer extensions has old extensions too
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
// starting from AVX
MK_MASK_IO_INTRINSICS_WRANGLER(double, 2, _mm_maskload_pd, _mm_maskstore_pd)
MK_MASK_IO_INTRINSICS_WRANGLER(float, 4, _mm_maskload_ps, _mm_maskstore_ps)
MK_MASK_IO_INTRINSICS_WRANGLER(double,
                               4,
                               _mm256_maskload_pd,
                               _mm256_maskstore_pd)
MK_MASK_IO_INTRINSICS_WRANGLER(float,
                               8,
                               _mm256_maskload_ps,
                               _mm256_maskstore_ps)

#if defined(__AVX2__) || defined(__AVX512F__)
MK_MASK_IO_INTRINSICS_WRANGLER(std::int64_t,
                               2,
                               _mm_maskload_epi64,
                               _mm_maskstore_epi64)
MK_MASK_IO_INTRINSICS_WRANGLER(std::int32_t,
                               4,
                               _mm_maskload_epi32,
                               _mm_maskstore_epi32)
MK_MASK_IO_INTRINSICS_WRANGLER(std::int64_t,
                               4,
                               _mm256_maskload_epi64,
                               _mm256_maskstore_epi64)
MK_MASK_IO_INTRINSICS_WRANGLER(std::int32_t,
                               8,
                               _mm256_maskload_epi32,
                               _mm256_maskstore_epi32)

template <>
struct mask_io_intrinsics_wrangler<std::uint64_t, 2> :
    mask_io_intrinsics_wrangler<std::int64_t, 2>
{
};

template <>
struct mask_io_intrinsics_wrangler<std::uin32_t, 4> :
    mask_io_intrinsics_wrangler<std::int32_t, 4>
{
};

template <>
struct mask_io_intrinsics_wrangler<std::uint64_t, 4> :
    mask_io_intrinsics_wrangler<std::int64_t, 4>
{
};

template <>
struct mask_io_intrinsics_wrangler<std::uint32_t, 8> :
    mask_io_intrinsics_wrangler<std::int32_t, 8>
{
};

// FIXME: implement for avx-512
#endif
#endif

#undef MK_MASK_IO_INTRINSICS_WRANGLER

template <fsize_t N, typename Op, typename Ret, typename... Args>
struct remap_comp;

template <fsize_t N, typename Dst, typename Src>
struct remap_comp<N, std::identity, Dst, Src>
{
    struct type
    {
        constexpr packed_vector<Dst, N> _FORCE_INLINE
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
struct remap_comp<N, ops::choice, T, I, T, T>
{
    struct type
    {
        constexpr packed_vector<T, N> _FORCE_INLINE
        operator()(const packed_vector<I, N> c,
                   const packed_vector<T, N> a,
                   const packed_vector<T, N> b) const noexcept
        {
            return c ? a : b;
        }
    };
};

template <fsize_t N, std::floating_point T>
struct remap_comp<N, ops::fma, T, T, T, T>
{
    struct type
    {
        constexpr packed_vector<T, N> _FORCE_INLINE
        operator()(const packed_vector<T, N> x,
                   const packed_vector<T, N> y,
                   const packed_vector<T, N> z) const noexcept
        {
            return __builtin_elementwise_fma(x, y, z);
        }
    };
};

#define MK_REMAP_BINOP(FUNC, OP, R)                                \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, FUNC<T>, R, T, T>                         \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<R, N> _FORCE_INLINE            \
            operator()(const packed_vector<T, N> a,                \
                       const packed_vector<T, N> b) const noexcept \
            {                                                      \
                return a OP b;                                     \
            }                                                      \
        };                                                         \
    };

#define MK_REMAP_UNOP(FUNC, OP, R)                                 \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, FUNC<T>, R, T>                            \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<R, N> _FORCE_INLINE            \
            operator()(const packed_vector<T, N> a) const noexcept \
            {                                                      \
                return OP a;                                       \
            }                                                      \
        };                                                         \
    };

#define MK_REMAP_BIN_FUNC(OLD, NEW)                                \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, OLD<T>, T, T, T>                          \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<T, N> _FORCE_INLINE            \
            operator()(const packed_vector<T, N> a,                \
                       const packed_vector<T, N> b) const noexcept \
            {                                                      \
                return NEW(a, b);                                  \
            }                                                      \
        };                                                         \
    };

#define MK_REMAP_UN_FUNC(OLD, NEW)                                 \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, OLD<T>, T, T>                             \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<T, N> _FORCE_INLINE            \
            operator()(const packed_vector<T, N> a) const noexcept \
            {                                                      \
                return NEW(a);                                     \
            }                                                      \
        };                                                         \
    };

#define MK_REMAP_BINOP_T(func, op) MK_REMAP_BINOP(func, op, T)
#define MK_REMAP_BINOP_I(func, op) MK_REMAP_BINOP(func, op, INDEX_TYPE)

#define MK_REMAP_UNOP_T(func, op)  MK_REMAP_UNOP(func, op, T)
#define MK_REMAP_UNOP_I(func, op)  MK_REMAP_UNOP(func, op, INDEX_TYPE)

MK_REMAP_BINOP_T(std::plus, +)
MK_REMAP_BINOP_T(std::minus, -)
MK_REMAP_BINOP_T(std::multiplies, *)
MK_REMAP_BINOP_T(std::divides, /)
MK_REMAP_BINOP_T(std::modulus, %)
MK_REMAP_UNOP_T(std::negate, -)

MK_REMAP_BINOP_I(std::equal_to, ==)
MK_REMAP_BINOP_I(std::not_equal_to, !=)
MK_REMAP_BINOP_I(std::greater, >)
MK_REMAP_BINOP_I(std::less, <)
MK_REMAP_BINOP_I(std::greater_equal, >=)
MK_REMAP_BINOP_I(std::less_equal, <=)

MK_REMAP_BINOP_I(std::logical_and, &&)
MK_REMAP_BINOP_I(std::logical_or, ||)
MK_REMAP_UNOP_I(std::logical_not, !)

MK_REMAP_BINOP_I(std::bit_and, &)
MK_REMAP_BINOP_I(std::bit_or, |)
MK_REMAP_BINOP_I(std::bit_xor, ^)
MK_REMAP_UNOP_I(std::bit_not, ~)

MK_REMAP_BINOP_I(ops::lshift, <<)
MK_REMAP_BINOP_I(ops::rshift, >>)

MK_REMAP_UN_FUNC(ops::abs, __builtin_elementwise_abs)
MK_REMAP_UN_FUNC(ops::round, __builtin_elementwise_round)
MK_REMAP_UN_FUNC(ops::ceil, __builtin_elementwise_ceil)
MK_REMAP_UN_FUNC(ops::floor, __builtin_elementwise_floor)
MK_REMAP_UN_FUNC(ops::trunc, __builtin_elementwise_trunc)
MK_REMAP_UN_FUNC(ops::copysign, __builtin_elementwise_copysign)
MK_REMAP_UN_FUNC(ops::sin, __builtin_elementwise_sin)
MK_REMAP_UN_FUNC(ops::cos, __builtin_elementwise_cos)

MK_REMAP_BIN_FUNC(ops::min, __builtin_elementwise_min)
MK_REMAP_BIN_FUNC(ops::max, __builtin_elementwise_max)

#undef MK_REMAP_UNOP_I
#undef MK_REMAP_UNOP_T
#undef MK_REMAP_BINOP_I
#undef MK_REMAP_BINOP_T
#undef MK_REMAP_UNOP
#undef MK_REMAP_BINOP_I
#undef MK_REMAP_UN_FUNC
#undef MK_REMAP_BIN_FUNC

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

template <typename RegTypes, typename RegNames, typename BPVar, fsize_t N>
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

    std::add_lvalue_reference_t<vec<vartype<BPVar>>>
    bp() noexcept
    {
        return get<BPVar>();
    }

    template <typename Var>
    vec<vartype<Var>>
    get() const noexcept
    {
        return helper::template get<vec_var<Var>>(m_data);
    }

    vec<vartype<BPVar>>
    bp() const noexcept
    {
        return get<BPVar>();
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
    void _FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).bp().data(), get());
        else
            std::forward<_Memory>(mem).bp() = get();
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
    packed_vector<T, N> _FORCE_INLINE
    compute(_Memory&& mem) const noexcept
    {
        auto vars = typelist::maybe_collect(type_sequence<Args...>{},
                                            loader,
                                            std::forward<_Memory>(mem));
        return invopply(vector_op{}, vars);
    }

  public:
    template <typename _Memory>
    void _FORCE_INLINE
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

    IdxVec _FORCE_INLINE
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

    IdxVec _FORCE_INLINE
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
    void _FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
        {
            index_t* dst = std::forward<_Memory>(mem).bp().data();
            index_t* src =
                std::forward<_Memory>(mem).template get<Var>().data();
            store<N>(dst, op(load<N>(src)));
        }
        else
            std::forward<_Memory>(mem).bp() =
                op(std::forward<_Memory>(mem).template get<Var>());
    }
};

template <typename SIMDRunner,
          typename SerialRunner,
          fsize_t N,
          fsize_t type_size>
class bulk_dispatcher
{
    // range starts with number of elements that can fit into SIMD register,
    // ends with the number of elements that can fit into cache line
    // e. g., for int32 in AVX it will be (32, 64)
    using bulk_sizes = intlist::map_f<
        decltype([](fsize_t i) constexpr noexcept { return i * N; }),
        intlist::range<static_cast<fsize_t>(1),
                       static_cast<fsize_t>(cache_line_size / (type_size * N) +
                                            1)>>;

    static constexpr bulk_sizes   seq{};
    static constexpr SIMDRunner   simd_runner{};
    static constexpr SerialRunner serial_runner{};

    /// @brief Compute aligned offset for the next view.
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

        // we have reached the end of the view
        if(view.size() == no) return;

        auto new_view = view.view(no);
        ASSERT(new_view.offset() % N == 0 || new_view.size() < N,
               "Unaligned access");
        dispatch(new_view, seq);
    }
};

/// @brief Methods of succession of SIMD instructions.
enum STITCH
{
    N_ORDER,    /*!< column-wise execution */
    MINIKERNEL, /*!< minikernels execution */
    MASKED      /*!< masked minikerenel */
};

template <typename Block, typename LabelMap, typename BPVar, fsize_t N>
struct minikernel_helper
{
    using load_vars  = load_list_t<Block, BPVar>;
    using store_vars = store_list_t<Block, BPVar>;

    using vars    = typelist::nub<typelist::join<load_vars, store_vars>>;
    using index_t = vartype<BPVar>;

    using reg_names = typelist::nub<typelist::map<varname, vars>>;
    using reg_types = infer_reg_types_t<vars, reg_names>;

    using serial_kernel  = standard_compiler<Block, LabelMap>;
    using serial_storage = bse::Memory<reg_types, reg_names, BPVar>;

    template <typename I, typename _LabelMap>
    using simd_eval = eval<I, _LabelMap, N>;

    using simd_kernel  = standard_compiler<Block, LabelMap, simd_eval>;
    using simd_storage = KernelMemory<reg_types, reg_names, BPVar, N>;
};

/// @brief Executes a single instruction @p I through the whole view.
template <typename I, typename LabelMap, typename BPVar>
class make_N_order_instruction_runner
{
    static constexpr fsize_t N = native_instruction_width_v<I, vartype<BPVar>>;

  public:
    using type = bulk_dispatcher<eval<I, LabelMap, N>,
                                 standard_eval<I, LabelMap>,
                                 N,
                                 sizeof(vartype<BPVar>)>;
};

template <STITCH stitch, typename... Args>
class make_block_compiler;

template <typename Block, typename LabelMap, typename BPVar>
class make_block_compiler<STITCH::N_ORDER, Block, LabelMap, BPVar>
{
    template <typename I, typename _LabelMap>
    using eval = make_N_order_instruction_runner<I, _LabelMap, BPVar>::type;

  public:
    using type = standard_compiler<Block, LabelMap, eval>;
};

/// @brief Executes @p Block as SIMD kernel.
template <typename Block, typename LabelMap, typename BPVar>
class make_block_compiler<STITCH::MINIKERNEL, Block, LabelMap, BPVar>
{
    using index_t = vartype<BPVar>;

    static constexpr fsize_t N = native_vector_extent_v<index_t>;

    using helper = minikernel_helper<Block, LabelMap, BPVar, N>;

    class SerialRunner
    {
        using Kernel  = helper::serial_kernel;
        using Storage = helper::serial_storage;
        static constexpr Kernel kernel{};

        template <typename _ProxyRow>
        static Storage _FORCE_INLINE
        alloc(_ProxyRow row) noexcept
        {
            Storage storage;
            typelist::for_each(
                typename helper::load_vars{},
                []<typename Var>(_ProxyRow row, Storage& storage) noexcept
                { storage.template get<Var>() = row.template get<Var>(); },
                row,
                storage);
            return storage;
        }

        template <typename _ProxyRow>
        static void _FORCE_INLINE
        spill(_ProxyRow row, Storage& storage) noexcept
        {
            typelist::for_each(
                typename helper::store_vars{},
                []<typename Var>(_ProxyRow row, Storage& storage) noexcept
                { row.template get<Var>() = storage.template get<Var>(); },
                row,
                storage);
        }

      public:
        template <typename _ProxyRow>
        void _FORCE_INLINE
        operator()(_ProxyRow row) const noexcept
        {
            Storage storage = alloc(row);
            kernel(storage);
            spill(row, storage);
        }
    };

    class SIMDRunner
    {
        using Kernel  = helper::simd_kernel;
        using Storage = helper::simd_storage;

        static constexpr Kernel kernel{};

        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        static Storage _FORCE_INLINE
        alloc(_MemoryView view) noexcept
        {
            Storage storage;
            typelist::for_each(
                typename helper::load_vars{},
                []<typename Var>(_MemoryView view, Storage& storage) noexcept
                {
                    storage.template get<Var>() =
                        load<N>(view.template get<Var>().data());
                },
                view,
                storage);
            return storage;
        }

        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        static void _FORCE_INLINE
        spill(_MemoryView view, Storage& storage) noexcept
        {
            typelist::for_each(
                typename helper::store_vars{},
                []<typename Var>(_MemoryView view, Storage& storage) noexcept
                {
                    store<N>(view.template get<Var>().data(),
                             storage.template get<Var>());
                },
                view,
                storage);
        }

      public:
        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        void _FORCE_INLINE
        operator()(_MemoryView view) const noexcept
        {
            Storage storage = alloc(view);
            kernel(storage);
            spill(view, storage);
        }
    };

  public:
    using type = bulk_dispatcher<SIMDRunner, SerialRunner, N, sizeof(index_t)>;
};

template <typename Block, typename LabelMap, typename BPVar>
class make_block_compiler<STITCH::MASKED, Block, LabelMap, BPVar>
{
    using index_t = vartype<BPVar>;

    static constexpr fsize_t N = native_vector_extent_v<index_t>;

    using helper = minikernel_helper<Block, LabelMap, BPVar, N>;

    using Kernel  = helper::simd_kernel;
    using Storage = helper::simd_storage;

    using mask_type = packed_vector<index_t, N>;

    static constexpr Kernel kernel{};

    template <typename _MemoryView>
        requires(pv_extent<_MemoryView> == N)
    static Storage _FORCE_INLINE
    alloc(_MemoryView view, const mask_type mask) noexcept
    {
        Storage storage;
        typelist::for_each(
            typename helper::load_vars{},
            []<typename Var>(_MemoryView     view,
                             Storage&        storage,
                             const mask_type mask) noexcept
            {
                using wrangler = mask_io_intrinsics_wrangler<vartype<Var>, N>;
                storage.template get<Var>() =
                    wrangler::load(view.template get<Var>().data(), mask);
            },
            view,
            storage,
            mask);
        return storage;
    }

    template <typename _MemoryView>
        requires(pv_extent<_MemoryView> == N)
    static void _FORCE_INLINE
    spill(_MemoryView view, Storage& storage, const mask_type mask) noexcept
    {
        typelist::for_each(
            typename helper::store_vars{},
            []<typename Var>(_MemoryView     view,
                             Storage&        storage,
                             const mask_type mask) noexcept
            {
                using wrangler = mask_io_intrinsics_wrangler<vartype<Var>, N>;
                wrangler::store(view.template get<Var>().data(),
                                mask,
                                storage.template get<Var>());
            },
            view,
            storage,
            mask);
    }

  public:
    struct type
    {
        const mask_type mask;

        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        void _FORCE_INLINE
        operator()(_MemoryView view) const noexcept
        {
            Storage storage = alloc(view, mask);
            kernel(storage);
            spill(view, storage, mask);
        }
    };
};

template <STITCH stitch, typename Block, typename LabelMap, typename BPVar>
using block_compiler =
    make_block_compiler<stitch, Block, LabelMap, BPVar>::type;

/// @brief Count the number of logical 1 in packed vector @p vec.
template <std::integral T, fsize_t N>
_FORCE_INLINE T
popcnt(const packed_vector<T, N> vec) noexcept
{
    // only AVX512VL + AVX512VPOPCNTDQ has dedicated popcnt
    // this is workaround for older vector extensions
    // FIXME: add AVX512 implementation
    return __builtin_reduce_add(
        __builtin_elementwise_min(vec, broadcast<N>(static_cast<T>(1))));
}

/// @brief Fast and lightweight set for block indices.
///
/// Takes native SIMD vector of block indices as input and optional initial
/// postion in case of sorted input.
/// Iterates over the set and returns a pair of block pointer number and its
/// vector mask. If @p ret_sum is set, the second value is the number of
/// repetitions of the block number in the input.
///
/// @tparam P program to make set for.
/// @tparam N vector width.
/// @tparam ret_sum if enabled iterated value is pair consisting of block
/// pointer and the number of repetitions of this block, otherwise the latter
/// value is vector mask fot this block.
template <typename P, fsize_t N, bool ret_sum = false>
class block_set : public std::ranges::view_interface<block_set<P, N, ret_sum>>
{
    using index_t        = P::index_t;
    using index_vector_t = packed_vector<index_t, N>;

    static constexpr index_t num_blocks = P::num_blocks;
    static constexpr index_t halt_pos   = P::halt_pos;

    const index_vector_t m_bps;
    const index_t        m_init_pos{};

    class _iterator : public std::input_iterator_tag
    {
        using ret_t = std::conditional_t<ret_sum, index_t, index_vector_t>;

        index_t              m_sum{};
        const index_vector_t m_bps;
        index_t              m_pos{};
        ret_t                m_ret;

        static _FORCE_INLINE index_t
        fit_inc(const index_t i) noexcept
        {
            return fit_block_index<P>(i + static_cast<index_t>(1));
        }

        _FORCE_INLINE bool
        sum(const index_t pos) noexcept
        {
            if constexpr(ret_sum)
            {
                const index_vector_t mask = m_bps == broadcast<N>(pos);
                m_ret                     = popcnt<index_t, N>(mask);
                m_sum += m_ret;
                return m_ret;
            }
            else
            {
                m_ret             = m_bps == broadcast<N>(pos);
                const index_t ret = popcnt<index_t, N>(m_ret);
                m_sum += ret;
                return ret;
            }
        }

        _FORCE_INLINE index_t
        next(index_t pos) noexcept
        {
            while(!sum(pos)) pos = fit_inc(pos);
            return pos;
        }

      public:
        using difference_type = std::make_signed_t<index_t>;
        using value_type      = std::pair<index_t, ret_t>;

        _iterator() noexcept = default;
        _iterator(const index_vector_t bps,
                  const index_t        initial_pos = 0) noexcept :
            m_sum(0), m_bps(bps), m_pos(next(initial_pos))
        {
        }

        bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_sum > N;
        }

        value_type
        operator*() const noexcept
        {
            return {m_pos, m_ret};
        }

        _iterator&
        operator++() noexcept
        {
            m_pos = next(fit_inc(m_pos));
            return *this;
        }

        _iterator
        operator++(int) noexcept
        {
            _iterator ret(*this);
            m_pos = next(fit_inc(m_pos));
            return ret;
        }
    };

  public:
    block_set() noexcept = default;
    block_set(const index_vector_t bps, const index_t initial_pos = 0) noexcept
        : m_bps(bps), m_init_pos(initial_pos)
    {
    }

    _iterator
    begin() const noexcept
    {
        return {m_bps, m_init_pos};
    }

    std::default_sentinel_t
    end() const noexcept
    {
        return std::default_sentinel;
    }
};

/// @brief Vectorized counting sort.
/// @return number of halted blocks.
template <typename P, typename _Memory>
fsize_t
counting_sort(_Memory& in, _Memory& out) noexcept
{
    ASSERT(in.size() == out.size(), "Sizes must match");

    using index_t = P::index_t;

    constexpr fsize_t num_bins = P::num_blocks + 1;
    constexpr fsize_t N        = native_vector_extent_v<index_t>;

    ASSERT(in.size() % N == 0, "Size be aligned to vector width");

    using vector_index_t = packed_vector<index_t, N>;

    constexpr index_t halt_pos = P::halt_pos;

    index_t bins[num_bins] = {};

    // histogram
    for(index_t pos = 0; pos < in.size(); pos += N)
    {
        const vector_index_t bps =
            load<N>(in.template view<N>(pos).bp().data());
        for(const auto [bp, sum]: block_set<P, N, true>(bps))
            bins[bve::fit_in_bins<halt_pos, num_bins>(bp)] += sum;
    }

    const fsize_t ret = bins[num_bins - 1];

    // prefix sum
    // no point in vectorizing this - num_bins is usually too small
    for(index_t i = 1; i < num_bins; ++i) bins[i] += bins[i - 1];

    // fill out
    // FIXME: for avx512 implement using scatter
    for(int i = in.size() - 1; i >= 0; --i)
        out[--bins[bve::fit_in_bins<halt_pos, num_bins>(in.bp()[i])]] = in[i];

    return ret;
}

template <typename _Program, typename _Memory>
class CountingSorter
{
    _Memory  m_buf0;
    _Memory  m_buf1;
    _Memory* p_main = &m_buf0;
    _Memory* p_aux  = &m_buf1;

  public:
    CountingSorter() noexcept = default;
    CountingSorter(fsize_t n) : m_buf0(n), m_buf1(n) {}

    _Memory&
    mem() noexcept
    {
        return *p_main;
    }

    const _Memory&
    mem() const noexcept
    {
        return *p_main;
    }

    auto
    sort() noexcept
    {
        const auto ret = counting_sort<_Program>(*p_main, *p_aux);
        std::swap(p_main, p_aux);
        return ret;
    }
};

template <typename _Sorter>
struct is_simd_counting_sorter : std::false_type
{
};

template <typename P, typename M>
struct is_simd_counting_sorter<CountingSorter<P, M>> : std::true_type
{
};

enum SORT
{
    bve_INTROSORT     = bve::INTROSORT,
    bve_COUNTING_SORT = bve::COUNTING_SORT,
    COUNTING_SORT
};

template <SORT sort_method>
struct select_sorter;

template <>
struct select_sorter<bve_INTROSORT>
{
    template <typename _Program, typename _Memory>
    using type = bve::SimpleSorter<_Program, _Memory>;
};

template <>
struct select_sorter<bve_COUNTING_SORT>
{
    template <typename _Program, typename _Memory>
    using type = bve::CountingSorter<_Program, _Memory>;
};

template <>
struct select_sorter<COUNTING_SORT>
{
    template <typename _Program, typename _Memory>
    using type = CountingSorter<_Program, _Memory>;
};

/// @brief Stream facade that allows asynchronous exchange/computation.
template <typename _Program,
          template <typename, typename> typename _Vector,
          template <typename> typename Allocator,
          typename Sorter>
    requires(cache_line_size % simd_width == 0)
class AsyncStreamFacade :
    public bve::MemoryFacade<_Program, _Vector, Allocator, Sorter>
{
    using reg_names   = _Program::reg_names;
    using reg_types   = _Program::reg_types;
    using bp_var      = _Program::bp;
    using input_vars  = _Program::input_vars;
    using output_vars = _Program::output_vars;
    using index_t     = _Program::index_t;

    static constexpr index_t halt_pos  = _Program::halt_pos;
    static constexpr fsize_t npos      = static_cast<fsize_t>(-1);
    static constexpr fsize_t step_size = cache_line_capacity_v<index_t>;

    using facade = bve::MemoryFacade<_Program, _Vector, Allocator, Sorter>;

    fsize_t m_pos = npos;

    // we're looking for the first cache-aligned segment in the memory that's
    // filled with halts
    void
    find_halt_segments() noexcept
    {
        const auto& bps = facade::mem().bp();
        m_pos           = npos;
        for(fsize_t pos = 0; pos < bps.size(); pos += step_size)
            if(bps[pos] == halt_pos)
            {
                m_pos = pos;
                return;
            }
    }

  public:
    using facade::facade;

    void
    sort() noexcept
    {
        if constexpr(is_simd_counting_sorter<Sorter>::value)
        {
            // here sort returns the number of halts
            const fsize_t nhalts = facade::sort();

            m_pos = nhalts >= step_size ?
                        align_top(facade::mem().size() - nhalts, step_size) :
                        npos;
        }
        else
        {
            facade::sort();

            // we do this here, because only sort is protected with a lock in
            // multithreaded stream
            find_halt_segments();
        }
    }

    template <std::ranges::input_range Input>
        requires(
            std::is_nothrow_convertible_v<std::ranges::range_value_t<Input>,
                                          input_type<_Program>>)
    auto
    exchange(Input&& input) noexcept
    {
        return bve::exchange_from(static_cast<facade&>(*this),
                                  std::forward<Input>(input),
                                  m_pos == npos ?
                                      facade::mem().end() : // empty exchange
                                      facade::mem().begin() + m_pos);
    }

    template <typename F>
    void
    visit(F&& f) noexcept
    {
        std::invoke(std::forward<F>(f),
                    m_pos == npos ? facade::mem().view() :
                                    facade::mem().view(0, m_pos));
    }
};

template <typename _Memory>
class memory_chunks : public std::ranges::view_interface<memory_chunks<_Memory>>
{
    using proxy_view = _Memory::view_type;

    proxy_view m_view;

    template <typename index_t>
    static fsize_t
    chunk_size(std::span<index_t> bps) noexcept
    {
        const index_t bp = bps.front();
        fsize_t       i  = 0;
        while(i < bps.size() && bps[i] == bp) ++i;
        return i;
    }

    class _iterator : public std::input_iterator_tag
    {
        proxy_view m_view{};
        fsize_t    m_pos{};

        fsize_t
        offset() const noexcept
        {
            return chunk_size(m_view.bp().subspan(m_pos));
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
    memory_chunks(proxy_view view) noexcept : m_view(view) {}

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

template <typename _Memory, fsize_t N, auto halt_pos>
class aligned_chunks :
    public std::ranges::view_interface<aligned_chunks<_Memory, N, halt_pos>>
{
    using proxy_view = _Memory::view_type;

    proxy_view m_view;

    class _iterator : public std::input_iterator_tag
    {
        proxy_view m_view{};
        fsize_t    m_pos{};

      public:
        using difference_type = std::ptrdiff_t;
        using value_type      = decltype(m_view.template view<N>(m_pos));

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
            return m_pos >= m_view.size() || m_view.bp()[m_pos] == halt_pos;
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
            return m_view.template view<N>(m_pos);
        }

        _iterator&
        operator++() noexcept
        {
            m_pos += N;
            return *this;
        }

        _iterator
        operator++(int) noexcept
        {
            _iterator ret(*this);
            m_pos += N;
            return ret;
        }
    };

  public:
    aligned_chunks() = default;
    aligned_chunks(proxy_view view) noexcept : m_view(view)
    {
        ASSERT(view.size() % N == 0,
               "view size is not divisible by vector size");
    }

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

template <STITCH stitch>
struct visitor_selector
{
    template <typename _Memory, typename _CompiledBlocks, typename...>
    struct type
    {
        using memory_view = _Memory::view_type;

        void
        operator()(memory_view view) const noexcept
        {
            std::ranges::for_each(memory_chunks<_Memory>(view),
                                  [](memory_view view) noexcept
                                  {
                                      typelist::visit_at(
                                          view.bp().front(),
                                          [&]<typename Block>() noexcept
                                          { Block{}(view); },
                                          _CompiledBlocks{},
                                          sink);
                                  });
        }

        void
        operator()(_Memory& mem) const noexcept
        {
            operator()(mem.view());
        }
    };
};

template <>
struct visitor_selector<STITCH::MASKED>
{
    template <typename _Memory, typename _CompiledBlocks, typename _Program>
    struct type
    {
        using memory_view = _Memory::view_type;
        using index_t     = _Program::index_t;

        static constexpr fsize_t N = native_vector_extent_v<index_t>;

        using index_vector_t = packed_vector<index_t, N>;

        void
        operator()(memory_view view) const noexcept
        {
            std::ranges::for_each(
                aligned_chunks<_Memory, N, _Program::halt_pos>(view),
                [](auto view) noexcept
                    requires(pv_extent<decltype(view)> == N)
                {
                    const index_t        start = view.bp()[0];
                    const index_vector_t bps   = load<N>(view.bp().data());

                    for(const auto [bp, mask]:
                        block_set<_Program, N>{bps, start})
                        typelist::visit_at(
                            bp,
                            [&]<typename Block>() noexcept { Block{mask}(view); },
                            _CompiledBlocks{},
                            sink);
                });
        }

        void
        operator()(_Memory& mem) const noexcept
        {
            operator()(mem.view());
        }
    };
};

template <STITCH stitch, typename... Args>
using select_visitor = visitor_selector<stitch>::template type<Args...>;

/// @brief Allocates memory aligned to native SIMD vector size.
template <typename T>
using SIMDAlignedAllocator =
    AlignedAllocator<T, get_native_vector_align<T>::value>;

/// @brief SIMD backend.
/// @tparam _Program program to compile.
/// @tparam stitch execution method.
/// @tparam sort_method sorting algorithm to use.
/// @tparam async allow asynchronous exchange in resulting stream.
template <typename _Program,
          STITCH stitch,
          SORT   sort_method,
          bool   async = false>
    requires(typelist::of_same_size<typename _Program::reg_types>)
class compiler
{
    using labelmap  = _Program::labelmap;
    using blocks     = _Program::blocks;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using bp        = _Program::bp;

    static constexpr bool    singular = _Program::singular;
    static constexpr auto    halt_pos = _Program::halt_pos;
    static constexpr fsize_t align    = simd_align;

    template <typename Block>
    using compile_block = block_compiler<stitch, Block, labelmap, bp>;

    using compiled_blocks = typelist::map<compile_block, blocks>;

    template <typename T>
    using allocator = std::conditional_t<async,
                                         CacheLineAlignedAllocator<T>,
                                         SIMDAlignedAllocator<T>>;

    using memory =
        bve::Memory<reg_types, reg_names, bp, std::vector, allocator>;
    using memory_view = memory::view_type;

    static constexpr SORT final_sort_method =
        singular ? bve_INTROSORT : sort_method;
    using sorter =
        select_sorter<final_sort_method>::template type<_Program, memory>;

    using stream_facade = std::conditional_t<
        async,
        AsyncStreamFacade<_Program, std::vector, allocator, sorter>,
        bve::StreamFacade<_Program, std::vector, allocator, sorter>>;
    using memory_facade =
        bve::MemoryFacade<_Program, std::vector, allocator, sorter>;

    using visitor = select_visitor<stitch, memory, compiled_blocks, _Program>;

    class single_block_evaluator : public memory_facade
    {
        static constexpr typelist::head<compiled_blocks> block{};

      public:
        using program = _Program;
        using memory_facade::memory_facade;

        template <bool zip_outputs = false, typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            memory_facade::set_inputs(std::forward<Args>(args)...);
            memory_facade::visit([](memory& mem) noexcept
                                 { block(mem.view()); });
            if constexpr(zip_outputs)
                return memory_facade::get_output_zipped();
            else
                return memory_facade::get_outputs();
        }
    };

    class evaluator : public memory_facade
    {
#if 0
        using vars = _Program::variables;
        void
        dump() const noexcept
        {
            auto columns = typelist::maybe_collect<std::tuple>(
                vars{},
                [&]<typename Var>() constexpr noexcept
                {
                    return std::views::all(
                        memory_facade::mem().template get<Var>());
                });

            typelist::for_each(vars{},
                               []<typename Var>()
                               {
                                   using name       = varname<Var>;
                                   constexpr auto l = intlist::length<name>;
                                   constexpr auto a = intlist::array<name>;
                                   constexpr std::string_view s(a, l);
                                   fmt::print("{},", s);
                               });
            putchar('\n');
            for(fsize_t i = 0; i < memory_facade::size(); ++i)
            {
                lucid::for_each(columns,
                                [i](const auto& c)
                                { fmt::print("{},", c[i]); });
                putchar('\n');
            }
        }
#endif

      public:
        using program = _Program;
        using memory_facade::memory_facade;

        template <bool zip_outputs = false, typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            memory_facade::set_inputs(std::forward<Args>(args)...);

            while(!memory_facade::done())
            {
#if 0
                dump();
#endif
                memory_facade::visit(visitor{});
                memory_facade::sort();
            }

            if constexpr(zip_outputs)
                return memory_facade::get_output_zipped();
            else
                return memory_facade::get_outputs();
        }
    };

  public:
    using function = std::
        conditional_t<_Program::singular, single_block_evaluator, evaluator>;

    class stream : public stream_facade
    {
        static_assert(!_Program::singular);

      public:
        using program = _Program;
        using stream_facade::stream_facade;

        constexpr void
        operator()() noexcept
        {
            stream_facade::visit(visitor{});
        }
    };
};

template <typename _Program, STITCH stitch, SORT sort_method>
using function = compiler<_Program, stitch, sort_method, false>::function;

template <typename _Program,
          STITCH stitch,
          SORT   sort_method,
          bool   async = false>
    requires(!_Program::singular)
using stream = compiler<_Program, stitch, sort_method, async>::stream;

template <typename Block, typename LabelMap, typename BPVar, fsize_t N>
class make_embedded_masked_runner
{
    using index_t = vartype<BPVar>;

    using helper = minikernel_helper<Block, LabelMap, BPVar, N>;

    using Kernel      = helper::simd_kernel;
    using TempStorage = helper::simd_storage;

    using mask_type = packed_vector<index_t, N>;

    static constexpr Kernel kernel{};

    template <typename _MainStorage>
    static TempStorage _FORCE_INLINE
    alloc(const _MainStorage& main_storage) noexcept
    {
        TempStorage ret;
        typelist::for_each(
            typename helper::load_vars{},
            []<typename Var>(const _MainStorage& main_storage,
                             TempStorage&        tmp_storage) noexcept
            {
                tmp_storage.template get<Var>() =
                    main_storage.template get<Var>();
            },
            main_storage,
            ret);
        return ret;
    }

    template <typename _MainStorage>
    static void _FORCE_INLINE
    spill(_MainStorage&   main_storage,
          TempStorage&    tmp_storage,
          const mask_type mask) noexcept
    {
        typelist::for_each(
            typename helper::store_vars{},
            []<typename Var>(_MainStorage&   main_storage,
                             TempStorage&    tmp_storage,
                             const mask_type mask) noexcept
            {
                auto& orig = main_storage.template get<Var>();
                orig       = mask ? tmp_storage.template get<Var>() : orig;
            },
            main_storage,
            tmp_storage,
            mask);
    }

  public:
    struct type
    {
        const mask_type mask;

        template <typename _MainStorage>
        void _FORCE_INLINE
        operator()(_MainStorage& main_storage) const noexcept
        {
            TempStorage tmp_storage = alloc(main_storage);
            kernel(tmp_storage);
            spill(main_storage, tmp_storage, mask);
        }
    };
};

template <typename Block, typename LabelMap, typename BPVar, fsize_t N>
using embedded_masked_runner =
    make_embedded_masked_runner<Block, LabelMap, BPVar, N>::type;

template <typename _Program>
    requires(typelist::of_same_size<typename _Program::reg_types>)
class embedded_compiler
{
    using labelmap  = _Program::labelmap;
    using blocks     = _Program::blocks;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using bp        = _Program::bp;
    using index_t   = _Program::index_t;

    static constexpr bool    singular = _Program::singular;
    static constexpr fsize_t N        = native_vector_extent_v<index_t>;
    static constexpr packed_vector<index_t, N> halt_pos =
        broadcast<N>(_Program::halt_pos);

    template <typename... Ts>
    using memory = KernelMemory<Ts..., N>;

    using facade = bse::MemoryFacade<
        _Program,
        memory,
        decltype([]() constexpr noexcept -> packed_vector<index_t, N>
                 { return broadcast<N>(static_cast<index_t>(0)); })>;

    class single_block_evaluator : facade
    {
        template <typename I, typename _LabelMap>
        using simd_eval = eval<I, _LabelMap, N>;

        using Kernel =
            standard_compiler<typelist::head<blocks>, labelmap, simd_eval>;

        static constexpr Kernel block{};

      public:
        using program = _Program;

        template <typename... Args>
        auto
        operator()(const Args... args) noexcept
        {
            facade::set_inputs(args...);
            facade::step(block);
            return facade::get_outputs();
        }
    };

    class evaluator : facade
    {
        template <typename Block>
        using compile_block = embedded_masked_runner<Block, labelmap, bp, N>;

        using compiled_blocks = typelist::map<compile_block, blocks>;

      public:
        template <typename... Args>
        auto
        operator()(const Args... args) noexcept
        {
            facade::set_inputs(args...);
            while(!__builtin_reduce_and(facade::bp() == halt_pos))
                for(const auto [bp, mask]: block_set<_Program, N>{facade::bp()})
                    typelist::visit_at(
                        bp,
                        [&]<typename Runner>() noexcept
                        { facade::step(Runner{mask}); },
                        compiled_blocks{},
                        sink);
            return facade::get_outputs();
        }
    };

  public:
    using type = std::conditional_t<singular, single_block_evaluator, evaluator>;
};

template <typename _Program>
using embedded_function = embedded_compiler<_Program>::type;
} // namespace lucid::simd
