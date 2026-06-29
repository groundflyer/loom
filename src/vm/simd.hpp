// -*- C++ -*-
/// @file simd.hpp
/// @brief SIMD backend implementation.

#pragma once

#include "bse.hpp"
#include "bve.hpp"
#include "core.hpp"
#include "macros.hpp"
#include "operators.hpp"

#include <common/debug.hpp>
#include <common/memory.hpp>
#include <common/tmp.hpp>
#include <common/tuple.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <bitset>
#include <cstdlib>
#include <ratio>

#include <immintrin.h>

/// @brief Virtual machine SIMD backend.
namespace loom::vm::simd
{
/// @brief Fast size type.
using fsize_t = bve::fsize_t;

/// @brief Constant idicating dynamic size of a bve::ProxyView.
constexpr inline fsize_t dynamic_extent = bve::dynamic_extent;

#if defined(__AVX512F__)
/// @brief SIMD vector size in bytes.
constexpr inline fsize_t simd_width = 64; // 512
/// @brief SIMD vector alignment.
constexpr inline fsize_t simd_align = 64;
#elif defined(__AVX2__) || defined(__AVX__)
constexpr inline fsize_t simd_width = 32; // 256
constexpr inline fsize_t simd_align = 32;
#else
constexpr inline fsize_t simd_width = 16; // 128
constexpr inline fsize_t simd_align = 16;
#endif

/// @brief Current x86-64 vector extension infostring.
struct info
{
#if defined(__AVX512F__)
    static constexpr char value[] = "avx512";
#elif defined(__AVX2__)
    static constexpr char value[] = "avx2";
#elif defined(__AVX__)
    static constexpr char value[] = "avx";
#else
    static constexpr char value[] = "sse4.2 or older";
#endif
};

/// @brief Print current x86-64 vector extension.
static inline void
print_info() noexcept
{
    fmt::print("Using x86-64 vector extensions: {}\n", info::value);
}

/// @brief Test if provided type is bve::ProxyView and provide its size.
template <typename T>
struct proxy_view_extent
{
    static constexpr fsize_t value = 1;
};

/// @brief Get the size of bve::ProxyView.
template <typename Data, typename Helper, typename PCVar, fsize_t N>
struct proxy_view_extent<bve::ProxyView<Data, Helper, PCVar, N>>
{
    static constexpr fsize_t value = N;
};

/// @brief Helper alias to check and get bve::ProxyView size.
template <typename _ProxyView>
constexpr inline fsize_t pv_extent = proxy_view_extent<_ProxyView>::value;

/// @brief Same as simd_align, but takes into account -DNARROW_AVX_INT.
template <typename T>
struct get_native_vector_align
{
#if defined(__AVX__) && defined(NARROW_AVX_INT)
    static constexpr fsize_t value = std::is_integral_v<T> ? 16 : simd_align;
#else
    static constexpr fsize_t value = simd_align;
#endif
};

/// @brief Create aligned SIMD packed vector type that consist of @p N elements
/// of type @p T.
template <typename T, fsize_t N>
struct make_packed_vector
{
    typedef T type __attribute__((vector_size(N * sizeof(T)),
                                  aligned(get_native_vector_align<T>::value)));
};

/// @brief Helper alias.
template <typename T, fsize_t N>
using packed_vector = make_packed_vector<T, N>::type;

/// @brief Compute extent of the widest native SIMD vector for type @p T.
/// Takes into account -DNARROW_AVX_INT definition.
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

/// @brief Get native widest SIMD vector extent for instruction @p I.
template <typename I, typename index_t>
struct native_instruction_extent;

/// @brief vm::Compute specialization.
template <typename Op,
          typename Dst,
          typename Arg1,
          typename... Args,
          typename index_t>
struct native_instruction_extent<Compute<Op, Dst, Arg1, Args...>, index_t>
{
    static constexpr fsize_t value = native_vector_extent<vartype<Arg1>>::value;
};

/// @brief vm::Jump specialization.
template <typename Target, typename index_t>
struct native_instruction_extent<Jump<Target>, index_t>
{
    static constexpr fsize_t value = native_vector_extent<index_t>::value;
};

/// @brief vm::ParmJump specialization.
template <typename Var, typename Labels, typename index_t>
struct native_instruction_extent<ParmJump<Var, Labels>, index_t>
{
    static constexpr fsize_t value = native_vector_extent<index_t>::value;
};

/// @brief Helper alias.
template <typename T>
constexpr inline fsize_t native_vector_extent_v =
    native_vector_extent<T>::value;

/// @brief Helper alias.
template <typename T>
using native_vector = packed_vector<T, native_vector_extent_v<T>>;

/// @brief Helper alias.
template <typename I, typename index_t>
constexpr inline fsize_t native_instruction_extent_v =
    native_instruction_extent<I, index_t>::value;

/// @brief Load @p N values and pack them into vector of width @p N
/// from aligned memory location @p src.
template <fsize_t N, typename T>
packed_vector<T, N> FORCE_INLINE
load(T const* p) noexcept
{
    return *(packed_vector<T, N>*)p;
}

/// @brief Store values from packed vector into aligned memory location @p dst.
template <fsize_t N, typename T>
void FORCE_INLINE
store(T* dst, packed_vector<T, N> src) noexcept
{
    *(packed_vector<T, N>*)dst = src;
}

namespace detail
{
template <fsize_t, typename T>
constexpr T FORCE_INLINE
id(T v) noexcept
{
    return v;
}

template <fsize_t N, typename T, std::size_t... Is>
constexpr packed_vector<T, N> FORCE_INLINE
broadcast_impl(T v, std::index_sequence<Is...>) noexcept
{
    packed_vector<T, N> ret = {id<Is>(v)...};
    return ret;
}

template <typename T, typename... Ts>
static constexpr T FORCE_INLINE
nest_fma(T x2, T k1, T k2, Ts... ks) noexcept
{
    if constexpr(sizeof...(Ts) > 0)
        return __builtin_elementwise_fma(nest_fma(x2, k2, ks...), x2, k1);
    else
        return __builtin_elementwise_fma(x2, k2, k1);
}
} // namespace detail

/// @brief Broadcast a value to all elements of the resulting packed vector.
template <fsize_t N, typename T>
constexpr packed_vector<T, N> FORCE_INLINE
broadcast(const T v) noexcept
{
    // clang broadcast syntax doesn't work
    return detail::broadcast_impl<N>(v, std::make_index_sequence<N>{});
}

/// @brief Make mask type.
template <typename T, fsize_t N>
struct make_mask_type
{
    using type = packed_vector<int_of_size_t<sizeof(T)>, N>;
};

/// @brief Mask type helper typedef.
template <typename T, fsize_t N>
using mask_type = make_mask_type<T, N>::type;

/// @brief Intrinsics wrangler for mask load and store operations.
template <typename T, fsize_t N>
struct mask_io_intrinsics_wrangler
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;

    /// @brief Fallback implementation for mask load.
    static vector_t FORCE_INLINE
    load(T const* p, const mask_t mask) noexcept
    {
        return mask ? simd::load<N>(p) : broadcast<N>(static_cast<T>(0));
    }

    /// @brief Fallback implementation for mask store.
    static void FORCE_INLINE
    store(T* dest, const mask_t mask, const vector_t value) noexcept
    {
        simd::store<N>(dest, mask ? value : simd::load<N>(dest));
    }
};

/// @brief Intrinsics wrangler for comparison operations.
// Further we override it only for AVX-512 because of mask registers.
template <typename T, fsize_t N>
struct cmp_intrinsics_wrangler
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;

    /// @brief Compare for equality.
    static mask_t FORCE_INLINE
    eq(const vector_t lhs, const vector_t rhs) noexcept
    {
        return lhs == rhs;
    }

    /// @brief Compare for equality.
    static mask_t FORCE_INLINE
    neq(const vector_t lhs, const vector_t rhs) noexcept
    {
        return lhs != rhs;
    }
};

/// @brief Intrinsics wrangler for blend operations.
// Further we override it only for AVX-512 because of mask registers.
template <typename T, fsize_t N>
struct blend_intrinsics_wrangler
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;

    /// @brief Blend packed vectors.
    static vector_t FORCE_INLINE
    blend(const mask_t k, const vector_t a, const vector_t b) noexcept
    {
        return k ? b : a;
    }
};

/// @brief Intrinsics wrangler for gather/scatter operations. Only for use with
/// AVX-512.
template <typename T, fsize_t N, std::integral I = fsize_t>
struct gather_scatter_intrinsics_wrangler
{
    using vector_t   = packed_vector<T, N>;
    using index_type = packed_vector<I, N>;

    /// @brief Fallback implementation for gather operation.
    static vector_t FORCE_INLINE
    gather(index_type vindex, T const* base_addr) noexcept
    {
        vector_t ret;
        for(unsigned i = 0; i < N; ++i) ret[i] = *(base_addr + vindex[i]);
        return ret;
    }

    /// @brief Fallback implementation for scatter operation.
    static void FORCE_INLINE
    scatter(T* base_addr, index_type vindex, vector_t a) noexcept
    {
        for(unsigned i = 0; i < N; ++i) *(base_addr + vindex[i]) = a[i];
    }
};

/// @brief Intrinsics wrangler for reciprocal square root @f$f(x) =
/// \frac{1}{\sqrt{x}}@f$.
template <std::floating_point T, fsize_t N>
struct rsqrt_intrinsics_wrangler
{
    using vector_t = packed_vector<T, N>;

    /// @brief Fallback implementation.
    static vector_t FORCE_INLINE
    op(vector_t x) noexcept
    {
        return broadcast<N>(static_cast<T>(1)) / __builtin_elementwise_sqrt(x);
    }
};

/// @brief Intrinsics wrangler for fused multiply-subtract.
template <std::floating_point T, fsize_t N>
struct fms_intrinsics_wranger
{
    using vector_t = packed_vector<T, N>;

    /// @brief Fallback implementation.
    static vector_t FORCE_INLINE
    op(const vector_t x, const vector_t y, const vector_t z) noexcept
    {
        return __builtin_elementwise_fma(x, y, -z);
    }
};

#define MK_MASK_IO_INTRINSICS(TYPE, EXT, LOAD_INTR, STORE_INTR)             \
    template <>                                                             \
    struct mask_io_intrinsics_wrangler<TYPE, EXT>                           \
    {                                                                       \
        using vector_t = packed_vector<TYPE, EXT>;                          \
        using mask_t   = mask_type<TYPE, EXT>;                              \
        static vector_t FORCE_INLINE                                        \
        load(TYPE const* p, const mask_t mask) noexcept                     \
        {                                                                   \
            return LOAD_INTR(p, mask);                                      \
        }                                                                   \
        static void FORCE_INLINE                                            \
        store(TYPE* dest, const mask_t mask, const vector_t value) noexcept \
        {                                                                   \
            STORE_INTR(dest, mask, value);                                  \
        }                                                                   \
    };

#define MK_AVX512_MASK_T(TYPE, EXT, MASK_T) \
    template <>                             \
    struct make_mask_type<TYPE, EXT>        \
    {                                       \
        using type = MASK_T;                \
    };

#define MK_AVX512_MASK8(TYPE)  MK_AVX512_MASK_T(TYPE, 8, __mmask8)
#define MK_AVX512_MASK16(TYPE) MK_AVX512_MASK_T(TYPE, 16, __mmask16)
#define MK_AVX512_MASK32(TYPE) MK_AVX512_MASK_T(TYPE, 32, __mmask32)
#define MK_AVX512_MASK64(TYPE) MK_AVX512_MASK_T(TYPE, 64, __mmask64)

#define MK_AVX512_MASK_IO_INTRINSICS(TYPE, EXT, LOAD_INTR, STORE_INTR)      \
    template <>                                                             \
    struct mask_io_intrinsics_wrangler<TYPE, EXT>                           \
    {                                                                       \
        using vector_t = packed_vector<TYPE, EXT>;                          \
        using mask_t   = mask_type<TYPE, EXT>;                              \
        static constexpr vector_t src{};                                    \
        static vector_t FORCE_INLINE                                        \
        load(TYPE const* p, const mask_t mask) noexcept                     \
        {                                                                   \
            return LOAD_INTR(src, mask, p);                                 \
        }                                                                   \
        static void FORCE_INLINE                                            \
        store(TYPE* dest, const mask_t mask, const vector_t value) noexcept \
        {                                                                   \
            STORE_INTR(dest, mask, value);                                  \
        }                                                                   \
    };

#define MK_AVX512_CMP_INTRINSICS(TYPE, EXT, EQINTR, NEQINTR) \
    template <>                                              \
    struct cmp_intrinsics_wrangler<TYPE, EXT>                \
    {                                                        \
        using vector_t = packed_vector<TYPE, EXT>;           \
        using mask_t   = mask_type<TYPE, EXT>;               \
        static_assert(std::unsigned_integral<mask_t>);       \
        static mask_t FORCE_INLINE                           \
        eq(const vector_t lhs, const vector_t rhs) noexcept  \
        {                                                    \
            return EQINTR(lhs, rhs);                         \
        }                                                    \
        static mask_t FORCE_INLINE                           \
        neq(const vector_t lhs, const vector_t rhs) noexcept \
        {                                                    \
            return NEQINTR(lhs, rhs);                        \
        }                                                    \
    };

#define MK_AVX512_BLEND_INTRINSICS(TYPE, EXT, INTR)                        \
    template <>                                                            \
    struct blend_intrinsics_wrangler<TYPE, EXT>                            \
    {                                                                      \
        using vector_t = packed_vector<TYPE, EXT>;                         \
        using mask_t   = mask_type<TYPE, EXT>;                             \
        static vector_t FORCE_INLINE                                       \
        blend(const mask_t k, const vector_t a, const vector_t b) noexcept \
        {                                                                  \
            return INTR(k, a, b);                                          \
        }                                                                  \
    };

#define MK_AVX512_GATHER_SCATTER_INTRINSICS(TYPE,                        \
                                            EXT,                         \
                                            GATHER_INTR,                 \
                                            SCATTER_INTR)                \
    template <>                                                          \
    struct gather_scatter_intrinsics_wrangler<TYPE, EXT, fsize_t>        \
    {                                                                    \
        using vector_t   = packed_vector<TYPE, EXT>;                     \
        using index_type = packed_vector<fsize_t, EXT>;                  \
        static vector_t FORCE_INLINE                                     \
        gather(index_type vindex, TYPE const* base_addr) noexcept        \
        {                                                                \
            return GATHER_INTR(vindex, base_addr, sizeof(TYPE));         \
        }                                                                \
        static void FORCE_INLINE                                         \
        scatter(TYPE* base_addr, index_type vindex, vector_t a) noexcept \
        {                                                                \
            SCATTER_INTR(base_addr, vindex, a, sizeof(TYPE));            \
        }                                                                \
    };

#define MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(TYPE,               \
                                                  EXT,                \
                                                  ADD_INTR,           \
                                                  SUB_INTR,           \
                                                  MIN_INTR,           \
                                                  MAX_INTR)           \
    template <>                                                       \
    struct avx512_mask_add_sub_min_max_intrinsics_wrangler<TYPE, EXT> \
    {                                                                 \
        using vector_t = packed_vector<TYPE, EXT>;                    \
        using mask_t   = mask_type<TYPE, EXT>;                        \
        static vector_t FORCE_INLINE                                  \
        add(const vector_t src,                                       \
            const mask_t   mask,                                      \
            const vector_t a,                                         \
            const vector_t b) noexcept                                \
        {                                                             \
            return ADD_INTR(src, mask, a, b);                         \
        }                                                             \
        static vector_t FORCE_INLINE                                  \
        sub(const vector_t src,                                       \
            const mask_t   mask,                                      \
            const vector_t a,                                         \
            const vector_t b) noexcept                                \
        {                                                             \
            return SUB_INTR(src, mask, a, b);                         \
        }                                                             \
        static vector_t FORCE_INLINE                                  \
        min(const vector_t src,                                       \
            const mask_t   mask,                                      \
            const vector_t a,                                         \
            const vector_t b) noexcept                                \
        {                                                             \
            return MIN_INTR(src, mask, a, b);                         \
        }                                                             \
        static vector_t FORCE_INLINE                                  \
        max(const vector_t src,                                       \
            const mask_t   mask,                                      \
            const vector_t a,                                         \
            const vector_t b) noexcept                                \
        {                                                             \
            return MAX_INTR(src, mask, a, b);                         \
        }                                                             \
    };

#define MK_AVX512_MASK_MUL_DIV_INTRINSICS(TYPE, EXT, MUL_INTR, DIV_INTR) \
    template <>                                                          \
    struct avx512_mask_mul_div_intrinsics_wrangler<TYPE, EXT>            \
    {                                                                    \
        using vector_t = packed_vector<TYPE, EXT>;                       \
        using mask_t   = mask_type<TYPE, EXT>;                           \
        static vector_t FORCE_INLINE                                     \
        mul(const vector_t src,                                          \
            const mask_t   mask,                                         \
            const vector_t a,                                            \
            const vector_t b) noexcept                                   \
        {                                                                \
            return MUL_INTR(src, mask, a, b);                            \
        }                                                                \
        static vector_t FORCE_INLINE                                     \
        div(const vector_t src,                                          \
            const mask_t   mask,                                         \
            const vector_t a,                                            \
            const vector_t b) noexcept                                   \
        {                                                                \
            return DIV_INTR(src, mask, a, b);                            \
        }                                                                \
    };

#define MK_AVX512_MASK_ABS_INTRINSICS(TYPE, EXT, INTR)                         \
    template <>                                                                \
    struct avx512_mask_abs_intrinsics_wrangler<TYPE, EXT>                      \
    {                                                                          \
        using vector_t = packed_vector<TYPE, EXT>;                             \
        using mask_t   = mask_type<TYPE, EXT>;                                 \
        static vector_t FORCE_INLINE                                           \
        abs(const vector_t src, const mask_t mask, const vector_t v2) noexcept \
        {                                                                      \
            return INTR(src, mask, v2);                                        \
        }                                                                      \
    };

#define MK_AVX512_MASK_SQRT_INTRINSICS(TYPE, EXT, SQRT_INTR, RSQRT_INTR) \
    template <>                                                          \
    struct avx512_mask_sqrt_intrinsics_wrangler<TYPE, EXT>               \
    {                                                                    \
        using vector_t = packed_vector<TYPE, EXT>;                       \
        using mask_t   = mask_type<TYPE, EXT>;                           \
        static vector_t FORCE_INLINE                                     \
        sqrt(const vector_t src,                                         \
             const mask_t   mask,                                        \
             const vector_t v2) noexcept                                 \
        {                                                                \
            return SQRT_INTR(src, mask, v2);                             \
        }                                                                \
        static vector_t FORCE_INLINE                                     \
        rsqrt(const vector_t src,                                        \
              const mask_t   mask,                                       \
              const vector_t v2) noexcept                                \
        {                                                                \
            return RSQRT_INTR(src, mask, v2);                            \
        }                                                                \
    };

#define MK_RSQRT_INTRINSICS(TYPE, EXT, INTR)       \
    template <>                                    \
    struct rsqrt_intrinsics_wrangler<TYPE, EXT>    \
    {                                              \
        using vector_t = packed_vector<TYPE, EXT>; \
        static vector_t FORCE_INLINE               \
        op(vector_t x) noexcept                    \
        {                                          \
            return INTR(x);                        \
        }                                          \
    };

#define MK_FMS_INTRINSICS(TYPE, EXT, INTR)                                \
    template <>                                                           \
    struct fms_intrinsics_wranger<TYPE, EXT>                              \
    {                                                                     \
        using vector_t = packed_vector<TYPE, EXT>;                        \
        static vector_t FORCE_INLINE                                      \
        op(const vector_t x, const vector_t y, const vector_t z) noexcept \
        {                                                                 \
            return INTR(x, y, z);                                         \
        }                                                                 \
    };

// SSE
MK_RSQRT_INTRINSICS(float, 4, _mm_rsqrt_ps)

// assume that a CPU with newer extensions has old extensions too
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
// starting from AVX

MK_MASK_IO_INTRINSICS(double, 4, _mm256_maskload_pd, _mm256_maskstore_pd)
MK_MASK_IO_INTRINSICS(float, 8, _mm256_maskload_ps, _mm256_maskstore_ps)

MK_RSQRT_INTRINSICS(float, 8, _mm256_rsqrt_ps)

#if defined(__AVX2__) || defined(__AVX512F__)
MK_MASK_IO_INTRINSICS(long long,
                      4,
                      _mm256_maskload_epi64,
                      _mm256_maskstore_epi64)
MK_MASK_IO_INTRINSICS(std::int32_t,
                      8,
                      _mm256_maskload_epi32,
                      _mm256_maskstore_epi32)

MK_FMS_INTRINSICS(float, 8, _mm256_fmsub_ps)
MK_FMS_INTRINSICS(double, 4, _mm256_fmsub_pd)

#if defined(__AVX512F__)
/// @brief AVX512 intrinsics wrangler for masked addition, substraction, minimum
/// and maximum.
template <typename T, fsize_t N>
struct avx512_mask_add_sub_min_max_intrinsics_wrangler
{
    using vector_t       = packed_vector<T, N>;
    using mask_t         = mask_type<T, N>;
    using blend_wrangler = blend_intrinsics_wrangler<T, N>;

    /// @brief Addition fallback implementation.
    static vector_t FORCE_INLINE
    add(const vector_t src,
        const mask_t   mask,
        const vector_t a,
        const vector_t b) noexcept
    {
        return blend_wrangler::blend(mask, src, a + b);
    }

    /// @brief Subtraction fallback implementation.
    static vector_t FORCE_INLINE
    sub(const vector_t src,
        const mask_t   mask,
        const vector_t a,
        const vector_t b) noexcept
    {
        return blend_wrangler::blend(mask, src, a - b);
    }

    /// @brief Minimum fallback implementation.
    static vector_t FORCE_INLINE
    min(const vector_t src,
        const mask_t   mask,
        const vector_t a,
        const vector_t b) noexcept
    {
        return blend_wrangler::blend(mask,
                                     src,
                                     __builtin_elemintwise_min(a, b));
    }

    /// @brief Maximum fallback implementation.
    static vector_t FORCE_INLINE
    max(const vector_t src,
        const mask_t   mask,
        const vector_t a,
        const vector_t b) noexcept
    {
        return blend_wrangler::blend(mask,
                                     src,
                                     __builtin_elemintwise_max(a, b));
    }
};

/// @brief AVX512 intrinsics wrangler for division.
template <typename T, fsize_t N>
struct avx512_mask_mul_div_intrinsics_wrangler
{
    using vector_t       = packed_vector<T, N>;
    using mask_t         = mask_type<T, N>;
    using blend_wrangler = blend_intrinsics_wrangler<T, N>;

    /// @brief Multiplication fallback implementation.
    static vector_t FORCE_INLINE
    mul(const vector_t src,
        const mask_t   mask,
        const vector_t a,
        const vector_t b) noexcept
    {
        return blend_wrangler::blend(mask, src, a * b);
    }

    /// @brief Division fallback implementation.
    static vector_t FORCE_INLINE
    div(const vector_t src,
        const mask_t   mask,
        const vector_t a,
        const vector_t b) noexcept
    {
        return blend_wrangler::blend(mask, src, a / b);
    }
};

/// @brief AVX512 intrinsics wrangler for abs.
template <typename T, fsize_t N>
struct avx512_mask_abs_intrinsics_wrangler
{
    using vector_t       = packed_vector<T, N>;
    using mask_t         = mask_type<T, N>;
    using blend_wrangler = blend_intrinsics_wrangler<T, N>;

    /// @brief Abs fallback implementation.
    static vector_t FORCE_INLINE
    abs(const vector_t src, const mask_t mask, const vector_t v2) noexcept
    {
        return blend_wrangler::blend(mask, src, __builtin_elementwise_abs(v2));
    }
};

/// @brief AVX512 intrinsics wrangler for square root and reciprocal square
/// root.
template <typename T, fsize_t N>
struct avx512_mask_sqrt_intrinsics_wrangler
{
    using vector_t       = packed_vector<T, N>;
    using mask_t         = mask_type<T, N>;
    using blend_wrangler = blend_intrinsics_wrangler<T, N>;

    /// @brief Square root fallback implementation.
    static vector_t FORCE_INLINE
    sqrt(const vector_t src, const mask_t mask, const vector_t v2) noexcept
    {
        return blend_wrangler::blend(mask, src, __builtin_elementwise_sqrt(v2));
    }

    /// @brief Reciprocal square root fallback implementation.
    static vector_t FORCE_INLINE
    rsqrt(const vector_t src, const mask_t mask, const vector_t v2) noexcept
    {
        return blend_wrangler::blend(mask,
                                     src,
                                     broadcast<N>(static_cast<T>(1)) /
                                         __builtin_elementwise_sqrt(v2));
    }
};

MK_AVX512_MASK8(double)
MK_AVX512_MASK8(std::int64_t)
MK_AVX512_MASK8(std::uint64_t)
MK_AVX512_MASK16(float)
MK_AVX512_MASK16(std::int32_t)
MK_AVX512_MASK16(std::uint32_t)
MK_AVX512_MASK32(std::int16_t)
MK_AVX512_MASK32(std::uint16_t)
MK_AVX512_MASK64(std::int8_t)
MK_AVX512_MASK64(std::uint8_t)

MK_AVX512_MASK_IO_INTRINSICS(float,
                             16,
                             _mm512_mask_load_ps,
                             _mm512_mask_store_ps)
MK_AVX512_MASK_IO_INTRINSICS(double,
                             8,
                             _mm512_mask_load_pd,
                             _mm512_mask_store_pd)
MK_AVX512_MASK_IO_INTRINSICS(std::int32_t,
                             16,
                             _mm512_mask_load_epi32,
                             _mm512_mask_store_epi32)
MK_AVX512_MASK_IO_INTRINSICS(std::int64_t,
                             8,
                             _mm512_mask_load_epi64,
                             _mm512_mask_store_epi64)
MK_AVX512_MASK_IO_INTRINSICS(std::uint32_t,
                             16,
                             _mm512_mask_load_epi32,
                             _mm512_mask_store_epi32)
MK_AVX512_MASK_IO_INTRINSICS(std::uint64_t,
                             8,
                             _mm512_mask_load_epi64,
                             _mm512_mask_store_epi64)

// clang-format 20 fucks up here
// clang-format off
MK_AVX512_CMP_INTRINSICS(float, 16, _mm512_cmpeq_ps_mask, _mm512_cmpneq_ps_mask)
MK_AVX512_CMP_INTRINSICS(double, 8, _mm512_cmpeq_pd_mask, _mm512_cmpneq_pd_mask)
MK_AVX512_CMP_INTRINSICS(std::int32_t, 16, _mm512_cmpeq_epi32_mask, _mm512_cmpneq_epi32_mask)
MK_AVX512_CMP_INTRINSICS(std::uint32_t, 16, _mm512_cmpeq_epu32_mask, _mm512_cmpneq_epu32_mask)
MK_AVX512_CMP_INTRINSICS(std::int64_t, 8, _mm512_cmpeq_epi64_mask, _mm512_cmpneq_epi64_mask)
MK_AVX512_CMP_INTRINSICS(std::uint64_t, 8, _mm512_cmpeq_epu64_mask, _mm512_cmpneq_epu64_mask)

MK_AVX512_BLEND_INTRINSICS(float, 16, _mm512_mask_blend_ps)
MK_AVX512_BLEND_INTRINSICS(double, 8, _mm512_mask_blend_pd)
MK_AVX512_BLEND_INTRINSICS(std::int32_t, 16, _mm512_mask_blend_epi32)
MK_AVX512_BLEND_INTRINSICS(std::int64_t, 8, _mm512_mask_blend_epi64)
MK_AVX512_BLEND_INTRINSICS(std::uint32_t, 16, _mm512_mask_blend_epi32)
MK_AVX512_BLEND_INTRINSICS(std::uint64_t, 8, _mm512_mask_blend_epi64)

MK_AVX512_GATHER_SCATTER_INTRINSICS(float, 16, _mm512_i32gather_ps, _mm512_i32scatter_ps)
MK_AVX512_GATHER_SCATTER_INTRINSICS(double, 8, _mm512_i32gather_pd, _mm512_i32scatter_pd)
MK_AVX512_GATHER_SCATTER_INTRINSICS(std::int32_t, 16, _mm512_i32gather_epi32, _mm512_i32scatter_epi32)
MK_AVX512_GATHER_SCATTER_INTRINSICS(std::uint32_t, 16, _mm512_i32gather_epi32, _mm512_i32scatter_epi32)
MK_AVX512_GATHER_SCATTER_INTRINSICS(std::int64_t, 8, _mm512_i32gather_epi64, _mm512_i32scatter_epi64)
MK_AVX512_GATHER_SCATTER_INTRINSICS(std::uint64_t, 8, _mm512_i32gather_epi64, _mm512_i32scatter_epi64)

MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(float, 16,
                                          _mm512_mask_add_ps,
                                          _mm512_mask_sub_ps,
                                          _mm512_mask_min_ps,
                                          _mm512_mask_max_ps)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(double, 8,
                                          _mm512_mask_add_pd,
                                          _mm512_mask_sub_pd,
                                          _mm512_mask_min_pd,
                                          _mm512_mask_max_pd)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::int32_t, 16,
                                          _mm512_mask_add_epi32,
                                          _mm512_mask_sub_epi32,
                                          _mm512_mask_min_epi32,
                                          _mm512_mask_max_epi32)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::uint32_t, 16,
                                          _mm512_mask_add_epi32,
                                          _mm512_mask_sub_epi32,
                                          _mm512_mask_min_epu32,
                                          _mm512_mask_max_epu32)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::int64_t, 8,
                                          _mm512_mask_add_epi64,
                                          _mm512_mask_sub_epi64,
                                          _mm512_mask_min_epi64,
                                          _mm512_mask_max_epi64)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::uint64_t, 8,
                                          _mm512_mask_add_epi64,
                                          _mm512_mask_sub_epi64,
                                          _mm512_mask_min_epu64,
                                          _mm512_mask_max_epu64)

MK_AVX512_MASK_MUL_DIV_INTRINSICS(float, 16, _mm512_mask_mul_ps, _mm512_mask_div_ps)
MK_AVX512_MASK_MUL_DIV_INTRINSICS(double, 8, _mm512_mask_mul_pd, _mm512_mask_div_pd)

MK_AVX512_MASK_ABS_INTRINSICS(float, 16, _mm512_mask_abs_ps)
MK_AVX512_MASK_ABS_INTRINSICS(double, 8, _mm512_mask_abs_pd)
MK_AVX512_MASK_ABS_INTRINSICS(std::int32_t, 16, _mm512_mask_abs_epi32)
MK_AVX512_MASK_ABS_INTRINSICS(std::int64_t, 8, _mm512_mask_abs_epi64)

MK_AVX512_MASK_SQRT_INTRINSICS(float, 16, _mm512_mask_sqrt_ps, _mm512_mask_rsqrt14_ps)
MK_AVX512_MASK_SQRT_INTRINSICS(double, 8, _mm512_mask_sqrt_pd, _mm512_mask_rsqrt14_pd)

MK_RSQRT_INTRINSICS(float, 16, _mm512_rsqrt14_ps)
MK_RSQRT_INTRINSICS(double, 8, _mm512_rsqrt14_pd)

MK_FMS_INTRINSICS(float, 16, _mm512_fmsub_ps)
MK_FMS_INTRINSICS(double, 8, _mm512_fmsub_pd)

#if defined(__AVX512BW__)
MK_AVX512_CMP_INTRINSICS(std::int8_t, 64, _mm512_cmpeq_epi8_mask, _mm512_cmpneq_epi8_mask)
MK_AVX512_CMP_INTRINSICS(std::uint8_t, 64, _mm512_cmpeq_epu8_mask, _mm512_cmpneq_epu8_mask)
MK_AVX512_CMP_INTRINSICS(std::int16_t, 32, _mm512_cmpeq_epi16_mask, _mm512_cmpneq_epi16_mask)
MK_AVX512_CMP_INTRINSICS(std::uint16_t, 32, _mm512_cmpeq_epu16_mask, _mm512_cmpneq_epu16_mask)

MK_AVX512_BLEND_INTRINSICS(std::int8_t, 64, _mm512_mask_blend_epi8)
MK_AVX512_BLEND_INTRINSICS(std::uint8_t, 64, _mm512_mask_blend_epi8)
MK_AVX512_BLEND_INTRINSICS(std::int16_t, 32, _mm512_mask_blend_epi16)
MK_AVX512_BLEND_INTRINSICS(std::uint16_t, 32, _mm512_mask_blend_epi16)

MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::int8_t, 64,
                                          _mm512_mask_add_epi8,
                                          _mm512_mask_sub_epi8,
                                          _mm512_mask_min_epi8,
                                          _mm512_mask_max_epi8)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::uint8_t, 64,
                                          _mm512_mask_add_epi8,
                                          _mm512_mask_sub_epi8,
                                          _mm512_mask_min_epu8,
                                          _mm512_mask_max_epu8)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::int16_t, 32,
                                          _mm512_mask_add_epi16,
                                          _mm512_mask_sub_epi16,
                                          _mm512_mask_min_epi16,
                                          _mm512_mask_max_epi16)
MK_AVX512_MASK_ADD_SUB_MIN_MAX_INTRINSICS(std::uint16_t, 32,
                                          _mm512_mask_add_epi16,
                                          _mm512_mask_sub_epi16,
                                          _mm512_mask_min_epu16,
                                          _mm512_mask_max_epu16)

MK_AVX512_MASK_ABS_INTRINSICS(std::int8_t, 64, _mm512_mask_abs_epi8)
MK_AVX512_MASK_ABS_INTRINSICS(std::int16_t, 32, _mm512_mask_abs_epi16)
#endif
#endif
#endif
#endif

#undef MK_MASK_IO_INTRINSICS
#undef MK_AVX512_MASK_IO_INTRINSICS
#undef MK_AVX512_CMP_INTRINSICS
#undef MK_AVX512_MASK_T
#undef MK_AVX512_MASK8
#undef MK_AVX512_MASK16
#undef MK_AVX512_MASK32
#undef MK_AVX512_MASK64


template <fsize_t N, typename Op, typename Ret, typename... Args>
struct remap_comp;

// clang-format on

template <fsize_t N, typename Dst, typename Src>
struct remap_comp<N, std::identity, Dst, Src>
{
    struct type
    {
        constexpr packed_vector<Dst, N> FORCE_INLINE
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
struct remap_comp<N, ops::cmov, T, I, T, T>
{
    struct type
    {
        constexpr packed_vector<T, N> FORCE_INLINE
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
        constexpr packed_vector<T, N> FORCE_INLINE
        operator()(const packed_vector<T, N> x,
                   const packed_vector<T, N> y,
                   const packed_vector<T, N> z) const noexcept
        {
            return __builtin_elementwise_fma(x, y, z);
        }
    };
};

template <fsize_t N, std::floating_point T>
struct remap_comp<N, ops::fms, T, T, T, T>
{
    using wrangler = fms_intrinsics_wranger<T, N>;

    struct type
    {
        constexpr packed_vector<T, N> FORCE_INLINE
        operator()(const packed_vector<T, N> x,
                   const packed_vector<T, N> y,
                   const packed_vector<T, N> z) const noexcept
        {
            return wrangler::op(x, y, z);
        }
    };
};

// ensure no broadcast at runtime
#define DECLK(NAME, VAL) constexpr vector_t NAME = broadcast<N>(VAL)

/// @brief Fast approximate sine.
///
/// @warning Valid only for range @f$[-\pi, \pi]@f$
///
/// Coefficients taken from @cite Schlor2019
template <fsize_t N, std::floating_point T>
    requires(std::is_same_v<T, float> ||
             std::is_same_v<T, double>) // only 32-bit and 64-bit floating point
                                        // support for now
struct remap_comp<N, ops::sin<T>, T, T>
{
    using vector_t = packed_vector<T, N>;

    struct type
    {
        constexpr vector_t FORCE_INLINE
        operator()(const vector_t x) const noexcept
        {
            const vector_t x2 = x * x;
            if constexpr(std::is_same_v<T, float>) // using degree 15 for float
            {
                DECLK(k1, 0.99999999999999985783856947280351013f);
                DECLK(k2, -0.166666666666659653164780128859839316f);
                DECLK(k3, 0.00833333333327592139676057538106745659f);
                DECLK(k4, -0.000198412698232225093689107237380802637f);
                DECLK(k5, 2.75573164212929639596445204076151988e-6f);
                DECLK(k6, -2.5051870883490902518590017165648945e-8f);
                DECLK(k7, 1.6047844633018114428263064708844387e-10f);
                DECLK(k8, -7.37066278281678179293546235379257048e-13f);
                return x * detail::nest_fma(x2, k1, k2, k3, k4, k5, k6, k7, k8);
            }
            else // degree 33 for double
            {
                DECLK(k1, 1.);
                DECLK(k2, -0.166666666666666666666666666666666667);
                DECLK(k3, 0.00833333333333333333333333333333333333);
                DECLK(k4, -0.000198412698412698412698412698412698391);
                DECLK(k5, 2.75573192239858906525573192239844806e-6);
                DECLK(k6, -2.50521083854417187750521083848472947e-8);
                DECLK(k7, 1.60590438368216145993923769996397832e-10);
                DECLK(k8, -7.64716373181981647590109722915908442e-13);
                DECLK(k9, 2.81145725434552076319376582531091405e-15);
                DECLK(k10, -8.22063524662432971120933376991172232e-18);
                DECLK(k11, 1.95729410633912564384267263771198416e-20);
                DECLK(k12, -3.86817017063038299631930987571173923e-23);
                DECLK(k13, 6.4469502842428826022931795893278841e-26);
                DECLK(k14, -9.18368981471916803573077288615983239e-29);
                DECLK(k15, 1.13099507260187617461053670403005966e-31);
                DECLK(k16, -1.21592124202981493418420109807105932e-34);
                DECLK(k17, 1.1309232145353729608313042496143056e-37);
                return x * detail::nest_fma(x2,
                                            k1,
                                            k2,
                                            k3,
                                            k4,
                                            k5,
                                            k6,
                                            k7,
                                            k8,
                                            k9,
                                            k10,
                                            k11,
                                            k12,
                                            k13,
                                            k14,
                                            k15,
                                            k16,
                                            k17);
            }
        }
    };
};

/// @brief Fast approximate cosine.
///
/// @warning Valid only for range @f$[-\pi, \pi]@f$
///
/// Coefficients taken from @cite Schlor2019
template <fsize_t N, std::floating_point T>
    requires(std::is_same_v<T, float> ||
             std::is_same_v<T, double>) // only 32-bit and 64-bit floating point
                                        // support for now
struct remap_comp<N, ops::cos<T>, T, T>
{
    using vector_t = packed_vector<T, N>;

    struct type
    {
        constexpr vector_t FORCE_INLINE
        operator()(const vector_t x) const noexcept
        {
            const vector_t x2 = x * x;
            if constexpr(std::is_same_v<T, float>) // using degree 16 for float
            {
                DECLK(k1, 0.99999999999999999608981951072301546f);
                DECLK(k2, -0.49999999999999974308667070690271678f);
                DECLK(k3, 0.041666666666663887964497659102665977f);
                DECLK(k4, -0.00138888888887731722447063574018337351f);
                DECLK(k5, 0.0000248015872774439536215582612189964674f);
                DECLK(k6, -2.75573163935355075808996593111486352e-7f);
                DECLK(k7, 2.08765619601383980877940138085106396e-9f);
                DECLK(k8, -1.14629048996344469555493302470243688e-11f);
                DECLK(k9, 4.60900737685258733987906059894416831e-14f);
                return detail::nest_fma(x2, k1, k2, k3, k4, k5, k6, k7, k8, k9);
            }
            else // degree 32 for double
            {
                DECLK(k1, 1.);
                DECLK(k2, -0.5);
                DECLK(k3, 0.0416666666666666666666666666666666667);
                DECLK(k4, -0.00138888888888888888888888888888888864);
                DECLK(k5, 0.0000248015873015873015873015873015852479);
                DECLK(k6, -2.75573192239858906525573192229758788e-7);
                DECLK(k7, 2.08767569878680989792100899936785891e-9);
                DECLK(k8, -1.14707455977297247138516241570198547e-11);
                DECLK(k9, 4.77947733238738529742623766654306629e-14);
                DECLK(k10, -1.5619206968586226447944923010120297e-16);
                DECLK(k11, 4.11031762331216359173758562466598535e-19);
                DECLK(k12, -8.89679139244973290214501480503565676e-22);
                DECLK(k13, 1.61173757105463581590277799117611453e-24);
                DECLK(k14, -2.47959624821373207705239218354539419e-27);
                DECLK(k15, 3.27988537078655827882448844587725832e-30);
                DECLK(k16, -3.76931659747655711635436902424567497e-33);
                DECLK(k17, 3.72999385718624695747058735680768987e-36);
                return detail::nest_fma(x2,
                                        k1,
                                        k2,
                                        k3,
                                        k4,
                                        k5,
                                        k6,
                                        k7,
                                        k8,
                                        k9,
                                        k10,
                                        k11,
                                        k12,
                                        k13,
                                        k14,
                                        k15,
                                        k16,
                                        k17);
            }
        }
    };
};

/// @brief Remap rsqrt operation ops::rsqrt.
template <fsize_t N, typename T>
struct remap_comp<N, ops::rsqrt<T>, T, T>
{
    using rsqrt_wrangler = rsqrt_intrinsics_wrangler<T, N>;

    struct type
    {
        packed_vector<T, N> FORCE_INLINE
        operator()(const packed_vector<T, N> x) const noexcept
        {
            return rsqrt_wrangler::op(x);
        }
    };
};

/// @brief Remap select constant ops::sct.
template <fsize_t N, typename T, typename I, typename L>
    requires(typelist::length<L> <= N)
struct remap_comp<N, ops::sct<T, I, L>, T, I>
{
    using vector_t = packed_vector<T, N>;

    static constexpr vector_t table =
        std::apply([](const auto... vals) constexpr noexcept -> vector_t
                   { return {vals...}; },
                   ops::sct<T, I, L>::values);

    struct type
    {
        vector_t FORCE_INLINE
        operator()(const packed_vector<I, N> i) const noexcept
        {
            return __builtin_shufflevector(table, i);
        }
    };
};

/// @brief Remap select value ops::slt.
template <fsize_t N, typename T, typename I, typename... Args>
struct remap_comp<N, ops::slt<T, I>, T, I, Args...>
{
    using vector_t       = packed_vector<T, N>;
    using index_vector_t = packed_vector<I, N>;

    template <I Idx, typename... Rest>
    static vector_t FORCE_INLINE
    impl(const index_vector_t i,
         const vector_t       head,
         const Rest... tail) noexcept
    {
        constexpr index_vector_t cur_idx = broadcast<N>(Idx);

        if constexpr(sizeof...(Rest) > 0)
            return cur_idx == i ? head : impl<Idx + 1>(i, tail...);
        else
            return head;
    }

    struct type
    {
        template <std::same_as<vector_t>... Vs>
            requires(sizeof...(Vs) >= 2)
        vector_t FORCE_INLINE
        operator()(const index_vector_t i, const Vs... vs) const noexcept
        {
            return impl<0>(i, vs...);
        }
    };
};

/// @brief Remap predicate accumulation ops::pacc.
template <fsize_t N,
          typename T,
          std::integral I,
          template <typename> typename Acc,
          template <typename> typename Cmp>
struct remap_comp<N, ops::pacc<T, I, Acc, Cmp>, I, I, T, T>
{
    static constexpr typename remap_comp<N, Acc<I>, I, I, I>::type acc{};
    static constexpr typename remap_comp<N, Cmp<T>, I, T, T>::type cmp{};

    using vector_t = packed_vector<T, N>;
    using mask_t   = packed_vector<I, N>;

    struct type
    {
        mask_t FORCE_INLINE
        operator()(const mask_t   p,
                   const vector_t x,
                   const vector_t y) const noexcept
        {
            return acc(p, cmp(x, y));
        }
    };
};

#define MK_REMAP_BINOP(FUNC, OP, R)                                \
    template <fsize_t N, typename T>                               \
    struct remap_comp<N, FUNC<T>, R, T, T>                         \
    {                                                              \
        struct type                                                \
        {                                                          \
            constexpr packed_vector<R, N> FORCE_INLINE             \
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
            constexpr packed_vector<R, N> FORCE_INLINE             \
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
            constexpr packed_vector<T, N> FORCE_INLINE             \
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
            constexpr packed_vector<T, N> FORCE_INLINE             \
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
MK_REMAP_UN_FUNC(ops::sqrt, __builtin_elementwise_sqrt)

MK_REMAP_BIN_FUNC(ops::copysign, __builtin_elementwise_copysign)
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
#undef DECL

template <fsize_t N, typename Op, typename Ret, typename... Args>
using remap_comp_t = remap_comp<N, Op, Ret, Args...>::type;

/// @brief Default masked computation.
template <fsize_t N, typename Op, typename Ret, typename... Args>
struct remap_comp_mask
{
    using orig_comp      = remap_comp_t<N, Op, Ret, Args...>;
    using mask_t         = mask_type<Ret, N>;
    using blend_wrangler = blend_intrinsics_wrangler<Ret, N>;

    static constexpr orig_comp comp{};

    struct type
    {
        mask_t mask;

        template <typename... VArgs>
        packed_vector<Ret, N> FORCE_INLINE
        operator()(const packed_vector<Ret, N> src,
                   const VArgs... args) const noexcept
        {
            return blend_wrangler::blend(mask, src, comp(args...));
        }
    };
};

#ifdef __AVX512F__
/// @brief Remap masked addition.
template <fsize_t N, typename T>
struct remap_comp_mask<N, std::plus<T>, T, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_add_sub_min_max_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src,
                   const vector_t a,
                   const vector_t b) const noexcept
        {
            return wrangler::add(src, k, a, b);
        }
    };
};

/// @brief Remap masked subtract.
template <fsize_t N, typename T>
struct remap_comp_mask<N, std::minus<T>, T, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_add_sub_min_max_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src,
                   const vector_t a,
                   const vector_t b) const noexcept
        {
            return wrangler::sub(src, k, a, b);
        }
    };
};

/// @brief Remap masked minimum.
template <fsize_t N, typename T>
struct remap_comp_mask<N, ops::min<T>, T, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_add_sub_min_max_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src,
                   const vector_t a,
                   const vector_t b) const noexcept
        {
            return wrangler::min(src, k, a, b);
        }
    };
};

/// @brief Remap masked maximum.
template <fsize_t N, typename T>
struct remap_comp_mask<N, ops::max<T>, T, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_add_sub_min_max_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src,
                   const vector_t a,
                   const vector_t b) const noexcept
        {
            return wrangler::max(src, k, a, b);
        }
    };
};

/// @brief Remap masked multiplication.
template <fsize_t N, typename T>
struct remap_comp_mask<N, std::multiplies<T>, T, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_mul_div_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src,
                   const vector_t a,
                   const vector_t b) const noexcept
        {
            return wrangler::mul(src, k, a, b);
        }
    };
};

/// @brief Remap masked division.
template <fsize_t N, typename T>
struct remap_comp_mask<N, std::divides<T>, T, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_mul_div_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src,
                   const vector_t a,
                   const vector_t b) const noexcept
        {
            return wrangler::div(src, k, a, b);
        }
    };
};

/// @brief Remap masked abs.
template <fsize_t N, typename T>
struct remap_comp_mask<N, ops::abs<T>, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_abs_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src, const vector_t v2) const noexcept
        {
            return wrangler::abs(src, k, v2);
        }
    };
};

/// @brief Remap masked square root.
template <fsize_t N, typename T>
struct remap_comp_mask<N, ops::sqrt<T>, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_sqrt_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src, const vector_t v2) const noexcept
        {
            return wrangler::sqrt(src, k, v2);
        }
    };
};

/// @brief Remap masked reciprocal square root.
template <fsize_t N, typename T>
struct remap_comp_mask<N, ops::rsqrt<T>, T, T>
{
    using vector_t = packed_vector<T, N>;
    using mask_t   = mask_type<T, N>;
    using wrangler = avx512_mask_sqrt_intrinsics_wrangler<T, N>;

    struct type
    {
        mask_t k;

        vector_t FORCE_INLINE
        operator()(const vector_t src, const vector_t v2) const noexcept
        {
            return wrangler::rsqrt(src, k, v2);
        }
    };
};
#endif

/// @brief Helper alias.
template <fsize_t N, typename Op, typename Ret, typename... Args>
using remap_comp_mask_t = remap_comp_mask<N, Op, Ret, Args...>::type;

/// @brief Ensure loom::Union correctly converted to union of packed vectors.
/// Fundamental type specialisation.
template <typename T, fsize_t N>
struct vectorize_union : make_packed_vector<T, N>
{
};

/// @brief Ensure loom::Union correctly converted to union of packed vectors.
/// Union specialisation.
template <typename... Ts, fsize_t N>
struct vectorize_union<Union<Ts...>, N>
{
    using type = Union<packed_vector<Ts, N>...>;
};

/// @brief Local memory pool of SIMD vectors.
template <typename RegTypes, typename RegNames, typename PCVar, fsize_t N>
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
    std::add_lvalue_reference_t<vec<vartype<Var>>> FORCE_INLINE
    get() noexcept
    {
        return helper::template get<vec_var<Var>>(m_data);
    }

    std::add_lvalue_reference_t<vec<vartype<PCVar>>> FORCE_INLINE
    pc() noexcept
    {
        return get<PCVar>();
    }

    template <typename Var>
    FORCE_INLINE vec<vartype<Var>>
                 get() const noexcept
    {
        return helper::template get<vec_var<Var>>(m_data);
    }

    vec<vartype<PCVar>> FORCE_INLINE
    pc() const noexcept
    {
        return get<PCVar>();
    }
};

/// @brief Helper class that destinguish local memory pool against proxy view
/// and register values against constants.
template <fsize_t N>
struct var_loader
{
    template <typename Var, typename _Memory>
    packed_vector<vartype<Var>, N> FORCE_INLINE
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

/// @brief Defines SIMD evaluation of instruction @p I.
template <typename I, typename LabelMap, fsize_t N>
class eval;

/// @brief vm::Jump evaluation specialization.
template <typename Target, typename LabelMap, fsize_t N>
class eval<Jump<Target>, LabelMap, N>
{
    using constant       = typelist::lookup<Target, LabelMap>;
    using index_t        = get_index_t<LabelMap>;
    using mask_t         = mask_type<index_t, N>;
    using io_wrangler    = mask_io_intrinsics_wrangler<index_t, N>;
    using blend_wrangler = blend_intrinsics_wrangler<index_t, N>;
    using index_vector_t = packed_vector<index_t, N>;

    static constexpr index_t target = constant{}();

    static index_vector_t FORCE_INLINE
    get() noexcept
    {
        return broadcast<N>(target);
    }

  public:
    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).pc().data(), get());
        else
            std::forward<_Memory>(mem).pc() = get();
    }

    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem, const mask_t mask) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            io_wrangler::store(std::forward<_Memory>(mem).pc().data(),
                               mask,
                               get());
        else
            std::forward<_Memory>(mem).pc() =
                blend_wrangler::blend(mask,
                                      std::forward<_Memory>(mem).pc(),
                                      get());
    }
};

/// @brief Generic vm::Compute evaluation specialization.
template <typename Op,
          typename Dst,
          typename... Args,
          typename LabelMap,
          fsize_t N>
    requires(... && (sizeof(vartype<Dst>) == sizeof(vartype<Args>)))
class eval<Compute<Op, Dst, Args...>, LabelMap, N>
{
    using T              = vartype<Dst>;
    using index_t        = get_index_t<LabelMap>;
    using vector_op      = remap_comp_t<N, Op, T, vartype<Args>...>;
    using vector_mask_op = remap_comp_mask_t<N, Op, T, vartype<Args>...>;
    using loader_t       = var_loader<N>;
    using mask_t         = mask_type<T, N>;
    using io_wrangler    = mask_io_intrinsics_wrangler<T, N>;

    static constexpr loader_t  loader{};
    static constexpr vector_op vop{};

    template <typename _Memory>
    static packed_vector<T, N> FORCE_INLINE
    compute(_Memory&& mem) noexcept
    {
        return vop(loader.template operator()<Args>(mem)...);
    }

    template <typename _Memory>
    static packed_vector<T, N> FORCE_INLINE
    compute(_Memory&& mem, const mask_t mask) noexcept
    {
        return vector_mask_op{mask}(loader.template operator()<Dst>(mem),
                                    loader.template operator()<Args>(mem)...);
    }

  public:
    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).template get<Dst>().data(),
                     compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<Dst>() =
                compute(std::forward<_Memory>(mem));
    }

    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem, const mask_t mask) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            io_wrangler::store(
                std::forward<_Memory>(mem).template get<Dst>().data(),
                mask,
                compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<Dst>() =
                compute(std::forward<_Memory>(mem), mask);
    }
};

#ifdef __AVX512F__
/// @brief Special case masked FMA with destructive update of destination
/// register when dst = a.
template <typename A, typename B, typename C, typename LabelMap, fsize_t N>
    requires(std::is_floating_point_v<vartype<A>>)
class eval<Compute<ops::fma, A, A, B, C>, LabelMap, N>
{
    using T              = vartype<A>;
    using index_t        = get_index_t<LabelMap>;
    using vector_op      = remap_comp_t<N, ops::fma, T, T, T, T>;
    using vector_mask_op = remap_comp_mask_t<N, ops::fma, T, T, T, T>;
    using loader_t       = var_loader<N>;
    using mask_t         = mask_type<T, N>;
    using io_wrangler    = mask_io_intrinsics_wrangler<T, N>;

    static constexpr loader_t  loader{};
    static constexpr vector_op vop{};

    template <typename _Memory>
    static packed_vector<T, N> FORCE_INLINE
    compute(_Memory&& mem) noexcept
    {
        return vop(loader.template operator()<A>(mem),
                   loader.template operator()<B>(mem),
                   loader.template operator()<C>(mem));
    }

    template <typename _Memory>
    static packed_vector<T, N> FORCE_INLINE
    compute(_Memory&& mem, const mask_t mask) noexcept
    {
        if constexpr(std::is_same_v<T, float>)
            return _mm512_mask_fmadd_ps(loader.template operator()<A>(mem),
                                        mask,
                                        loader.template operator()<B>(mem),
                                        loader.template operator()<C>(mem));
        else if constexpr(std::is_same_v<T, double>)
            return _mm512_mask_fmadd_pd(loader.template operator()<A>(mem),
                                        mask,
                                        loader.template operator()<B>(mem),
                                        loader.template operator()<C>(mem));
        else
            // or _mm512_mask_fmadd_ph for AVX512FP16
            static_assert(false, "Unsupported operation");
    }

  public:
    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).template get<A>().data(),
                     compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<A>() =
                compute(std::forward<_Memory>(mem));
    }

    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem, const mask_t mask) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            io_wrangler::store(
                std::forward<_Memory>(mem).template get<A>().data(),
                mask,
                compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<A>() =
                compute(std::forward<_Memory>(mem), mask);
    }
};

/// @brief Special case masked FMS with destructive update of destination
/// register when dst = a.
template <typename A, typename B, typename C, typename LabelMap, fsize_t N>
    requires(std::is_floating_point_v<vartype<A>>)
class eval<Compute<ops::fms, A, A, B, C>, LabelMap, N>
{
    using T              = vartype<A>;
    using index_t        = get_index_t<LabelMap>;
    using vector_op      = remap_comp_t<N, ops::fms, T, T, T, T>;
    using vector_mask_op = remap_comp_mask_t<N, ops::fms, T, T, T, T>;
    using loader_t       = var_loader<N>;
    using mask_t         = mask_type<T, N>;
    using io_wrangler    = mask_io_intrinsics_wrangler<T, N>;

    static constexpr loader_t  loader{};
    static constexpr vector_op vop{};

    template <typename _Memory>
    static packed_vector<T, N> FORCE_INLINE
    compute(_Memory&& mem) noexcept
    {
        return vop(loader.template operator()<A>(mem),
                   loader.template operator()<B>(mem),
                   loader.template operator()<C>(mem));
    }

    template <typename _Memory>
    static packed_vector<T, N> FORCE_INLINE
    compute(_Memory&& mem, const mask_t mask) noexcept
    {
        if constexpr(std::is_same_v<T, float>)
            return _mm512_mask_fmsub_ps(loader.template operator()<A>(mem),
                                        mask,
                                        loader.template operator()<B>(mem),
                                        loader.template operator()<C>(mem));
        else if constexpr(std::is_same_v<T, double>)
            return _mm512_mask_fmsub_pd(loader.template operator()<A>(mem),
                                        mask,
                                        loader.template operator()<B>(mem),
                                        loader.template operator()<C>(mem));
        else
            // or _mm512_mask_fmsub_ph for AVX512FP16
            static_assert(false, "Unsupported operation");
    }

  public:
    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            store<N>(std::forward<_Memory>(mem).template get<A>().data(),
                     compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<A>() =
                compute(std::forward<_Memory>(mem));
    }

    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem, const mask_t mask) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
            io_wrangler::store(
                std::forward<_Memory>(mem).template get<A>().data(),
                mask,
                compute(std::forward<_Memory>(mem)));
        else
            std::forward<_Memory>(mem).template get<A>() =
                compute(std::forward<_Memory>(mem), mask);
    }
};
#endif

/// @brief Helper class for prametric jumps.
template <typename Idxs, fsize_t N>
struct shuffler;

/// @brief Helper specialization for two-way jump.
template <typename index_t, index_t a, index_t b, fsize_t N>
struct shuffler<std::integer_sequence<index_t, a, b>, N>
{
    using IdxVec = packed_vector<index_t, N>;

    static constexpr IdxVec va = broadcast<N>(a);
    static constexpr IdxVec vb = broadcast<N>(b);

    IdxVec FORCE_INLINE
    operator()(const IdxVec var) const noexcept
    {
        return var ? vb : va;
    }
};

/// @brief Helper specialization for multi-way jump.
template <typename index_t, index_t... is, fsize_t N>
    requires(sizeof...(is) <= N)
struct shuffler<std::integer_sequence<index_t, is...>, N>
{
    using IdxVec                  = packed_vector<index_t, N>;
    static constexpr IdxVec table = {is...};

    IdxVec FORCE_INLINE
    operator()(const IdxVec var) const noexcept
    {
        return __builtin_shufflevector(table, var);
    }
};

/// @brief vm::ParmJump evaluation specialization.
template <typename Var, typename Labels, typename LabelMap, fsize_t N>
class eval<ParmJump<Var, Labels>, LabelMap, N>
{
    using index_t        = get_index_t<LabelMap>;
    using idxs           = jmp_idxs<ParmJump<Var, Labels>, LabelMap>;
    using mask_t         = mask_type<index_t, N>;
    using io_wrangler    = mask_io_intrinsics_wrangler<index_t, N>;
    using blend_wrangler = blend_intrinsics_wrangler<index_t, N>;
    using index_vector_t = packed_vector<index_t, N>;

    static constexpr shuffler<idxs, N> op{};

  public:
    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
        {
            index_t* dst = std::forward<_Memory>(mem).pc().data();
            index_t* src =
                std::forward<_Memory>(mem).template get<Var>().data();
            const index_vector_t ret = load<N>(src);
            store<N>(dst, op(ret));
        }
        else
        {
            const index_vector_t ret =
                std::forward<_Memory>(mem).template get<Var>();
            std::forward<_Memory>(mem).pc() = op(ret);
        }
    }

    template <typename _Memory>
    void FORCE_INLINE
    operator()(_Memory&& mem, const mask_t mask) const noexcept
    {
        if constexpr(pv_extent<std::remove_cvref_t<_Memory>> == N)
        {
            index_t* dst = std::forward<_Memory>(mem).pc().data();
            index_t* src =
                std::forward<_Memory>(mem).template get<Var>().data();
            const index_vector_t ret = load<N>(src);
            io_wrangler::store(dst, mask, op(ret));
        }
        else
        {
            const index_vector_t ret =
                std::forward<_Memory>(mem).template get<Var>();
            std::forward<_Memory>(mem).pc() =
                blend_wrangler::blend(mask,
                                      std::forward<_Memory>(mem).pc(),
                                      op(ret));
        }
    }
};

/// @brief Unrolls loops of evaluations into cache line.
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
    WAVEFRONT,  /*!< column-wise execution */
    MINIKERNEL, /*!< minikernels execution */
    MASKED      /*!< masked minikerenel */
};

/// @brief Helper class for minikernel evaluation.
template <typename Block, typename LabelMap, typename PCVar, fsize_t N>
struct minikernel_helper
{
    using load_vars  = load_list_t<Block, PCVar>;
    using store_vars = store_list_t<Block, PCVar>;

    using vars    = typelist::nub<typelist::join<load_vars, store_vars>>;
    using index_t = vartype<PCVar>;

    using reg_names = typelist::nub<typelist::map<varname, vars>>;
    using reg_types = infer_reg_types_t<vars, reg_names>;

    using serial_kernel  = standard_compiler<Block, LabelMap>;
    using serial_storage = bse::Memory<reg_types, reg_names, PCVar>;

    template <typename I, typename _LabelMap>
    using simd_eval = eval<I, _LabelMap, N>;

    using simd_kernel  = standard_compiler<Block, LabelMap, simd_eval>;
    using simd_storage = KernelMemory<reg_types, reg_names, PCVar, N>;
};

/// @brief Executes a single instruction @p I through the whole view.
template <typename I, typename LabelMap, typename PCVar>
class make_wavefront_instruction_runner
{
    static constexpr fsize_t N = native_instruction_extent_v<I, vartype<PCVar>>;

  public:
    using type = bulk_dispatcher<eval<I, LabelMap, N>,
                                 standard_eval<I, LabelMap>,
                                 N,
                                 sizeof(vartype<PCVar>)>;
};

/// @brief Selects block compiler using @p stitch.
template <STITCH stitch, typename... Args>
class make_block_compiler;

/// @brief Wavefront specialization.
template <typename Block, typename LabelMap, typename PCVar>
class make_block_compiler<STITCH::WAVEFRONT, Block, LabelMap, PCVar>
{
    template <typename I, typename _LabelMap>
    using eval = make_wavefront_instruction_runner<I, _LabelMap, PCVar>::type;

  public:
    using type = standard_compiler<Block, LabelMap, eval>;
};

/// @brief Executes @p Block as SIMD kernel.
template <typename Block, typename LabelMap, typename PCVar>
class make_block_compiler<STITCH::MINIKERNEL, Block, LabelMap, PCVar>
{
    using index_t = vartype<PCVar>;

    static constexpr fsize_t N = native_vector_extent_v<index_t>;

    using helper = minikernel_helper<Block, LabelMap, PCVar, N>;

    class SerialRunner
    {
        using Kernel  = helper::serial_kernel;
        using Storage = helper::serial_storage;
        static constexpr Kernel kernel{};

        template <typename _ProxyRow>
        static Storage FORCE_INLINE
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
        static void FORCE_INLINE
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
        void FORCE_INLINE
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
        static Storage FORCE_INLINE
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
        static void FORCE_INLINE
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
        void FORCE_INLINE
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

/// @brief Masked minikernel specialization.
template <typename Block, typename LabelMap, typename PCVar>
class make_block_compiler<STITCH::MASKED, Block, LabelMap, PCVar>
{
    using index_t = vartype<PCVar>;

    static constexpr fsize_t N = native_vector_extent_v<index_t>;

    using helper = minikernel_helper<Block, LabelMap, PCVar, N>;

    using Kernel  = helper::simd_kernel;
    using Storage = helper::simd_storage;

    using mask_t = mask_type<index_t, N>;

    static constexpr Kernel kernel{};

    template <typename _MemoryView>
        requires(pv_extent<_MemoryView> == N)
    static Storage FORCE_INLINE
    alloc(_MemoryView view, const mask_t mask) noexcept
    {
        Storage storage;
        typelist::for_each(
            typename helper::load_vars{},
            []<typename Var>(_MemoryView  view,
                             Storage&     storage,
                             const mask_t mask) noexcept
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
    static Storage FORCE_INLINE
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
    static void FORCE_INLINE
    spill(_MemoryView view, Storage& storage, const mask_t mask) noexcept
    {
        typelist::for_each(
            typename helper::store_vars{},
            []<typename Var>(_MemoryView  view,
                             Storage&     storage,
                             const mask_t mask) noexcept
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
        const mask_t mask;

        template <typename _MemoryView>
            requires(pv_extent<_MemoryView> == N)
        void FORCE_INLINE
        operator()(_MemoryView view) const noexcept
        {
#ifdef ACTUALLY_DO_MASKED_LOAD
            Storage storage = alloc(view, mask);
#else
            Storage storage = alloc(view);
#endif
            kernel(storage);
            spill(view, storage, mask);
        }
    };
};

/// @brief Helper typedef.
template <STITCH stitch, typename Block, typename LabelMap, typename PCVar>
using block_compiler =
    make_block_compiler<stitch, Block, LabelMap, PCVar>::type;

/// @brief Helper to count the number of logical 1 in packed vector @p vec.
template <std::integral T, fsize_t N>
struct popcnt_helper
{
    T FORCE_INLINE
    operator()(const packed_vector<T, N> vec) const noexcept
    {
        return __builtin_reduce_add(
            __builtin_elementwise_min(vec, broadcast<N>(static_cast<T>(1))));
    }

    // AVX-512
    int FORCE_INLINE
    operator()(const uint_of_bits_t<N> k) const noexcept
    {
        return std::popcount(k);
    }
};

/// @brief Helper to perform reduce_and on mask type.
template <std::integral T, fsize_t N>
struct logical_reduce_helper
{
    static bool FORCE_INLINE
    land(const packed_vector<T, N> vec) noexcept
    {
        return __builtin_reduce_and(vec);
    }

    // AVX-512
    static bool FORCE_INLINE
    land(const uint_of_bits_t<N> k) noexcept
    {
        return k == static_cast<uint_of_bits_t<N>>(-1);
    }
};

/// @brief Fit program counter into bins.
template <std::unsigned_integral I>
I FORCE_INLINE
fit_in_bins(const I pc, const I last_bin) noexcept
{
    return pc < last_bin ? pc : last_bin;
}

/// @brief Set iterator for block indices.
template <std::unsigned_integral index_t>
class set_iterator : public std::input_iterator_tag
{
    std::uint64_t m_set;

    index_t FORCE_INLINE
    get_pos() const noexcept
    {
        return std::countr_zero(m_set);
    }

  public:
    using difference_type = std::make_signed_t<index_t>;
    using value_type      = index_t;

    set_iterator() noexcept = default;
    set_iterator(const std::uint64_t set) noexcept : m_set(set) {}

    bool FORCE_INLINE
    operator==(std::default_sentinel_t) const noexcept
    {
        return m_set == static_cast<std::uint64_t>(0);
    }

    value_type FORCE_INLINE
    operator*() const noexcept
    {
        return get_pos();
    }

    set_iterator& FORCE_INLINE
    operator++() noexcept
    {
        m_set ^= 1ul << get_pos();
        return *this;
    }

    set_iterator FORCE_INLINE
    operator++(int) noexcept
    {
        set_iterator ret(*this);
        m_set ^= 1ul << get_pos();
        return ret;
    }

    std::uint64_t FORCE_INLINE
    data() const noexcept
    {
        return m_set;
    }
};

/// @brief Simple bitset range.
/// @tparam index_t PC type.
template <typename index_t>
class simple_set : public std::ranges::view_interface<simple_set<index_t>>
{
    std::uint64_t m_set = 0;

  public:
    using iterator = set_iterator<index_t>;

    simple_set() noexcept = default;
    simple_set(const std::uint64_t set) noexcept : m_set(set) {}

    iterator FORCE_INLINE
    begin() const noexcept
    {
        return {m_set};
    }

    std::default_sentinel_t FORCE_INLINE
    end() const noexcept
    {
        return std::default_sentinel;
    }

    fsize_t FORCE_INLINE
    size() const noexcept
    {
        return std::popcount(m_set);
    }

    std::uint64_t FORCE_INLINE
    data() const noexcept
    {
        return m_set;
    }
};

/// @brief Fast and lightweight set for block indices.
/// @tparam K kernel to make set for.
/// @tparam N vector width.
/// @tparam ignore_halt ignore halt block index.
template <Kernel K, fsize_t N, bool ignore_halt>
class greedy_groups :
    public std::ranges::view_interface<greedy_groups<K, N, ignore_halt>>
{
    using index_t        = K::index_t;
    using index_vector_t = packed_vector<index_t, N>;

    static constexpr index_t num_blocks = K::num_blocks;
    static constexpr index_t halt_pos   = K::halt_pos;

    static_assert(num_blocks <= 63 + ignore_halt);

    std::uint64_t m_set{};

  public:
    using iterator = set_iterator<index_t>;

    greedy_groups() noexcept = default;
    greedy_groups(const std::uint64_t set) noexcept : m_set(set) {}
    greedy_groups(const index_vector_t pcs) noexcept : m_set(0)
    {
        for(fsize_t i = 0; i < N; ++i)
            if constexpr(ignore_halt)
            {
                if(pcs[i] != halt_pos) m_set |= 1ul << pcs[i];
            }
            else
                m_set |= 1ul << fit_in_bins(pcs[i], num_blocks);
    }

    iterator FORCE_INLINE
    begin() const noexcept
    {
        return {m_set};
    }

    std::default_sentinel_t FORCE_INLINE
    end() const noexcept
    {
        return std::default_sentinel;
    }

    fsize_t FORCE_INLINE
    size() const noexcept
    {
        return std::popcount(m_set);
    }
};

/// @brief Range over basic block successors.
template <Kernel K>
class successor_groups : public std::ranges::view_interface<successor_groups<K>>
{
    using index_t    = K::index_t;
    using successors = K::successors;

    static constexpr index_t halt_pos = K::halt_pos;

    static std::uint64_t FORCE_INLINE
    successor_set(const index_t pc) noexcept
    {
        return typelist::visit_at(
            pc,
            []<intlist::List Jdxs>() constexpr noexcept
            {
                constexpr std::uint64_t ret = fold_tuple(
                    map_tuple(
                        intlist::stdarray<Jdxs>,
                        [](const index_t pc) constexpr noexcept -> std::uint64_t
                        { return pc != halt_pos ? 1ul << pc : 0; }),
                    std::bit_or<std::uint64_t>{},
                    static_cast<std::uint64_t>(0));
                return ret;
            },
            successors{},
            constant(0ul));
    }

    std::uint64_t m_set = 1;

  public:
    using iterator = set_iterator<index_t>;

    successor_groups() noexcept = default;

    /// @param pc current basic block.
    successor_groups(const index_t pc) noexcept : m_set(successor_set(pc)) {}

    iterator FORCE_INLINE
    begin() const noexcept
    {
        return {m_set};
    }

    std::default_sentinel_t FORCE_INLINE
    end() const noexcept
    {
        return std::default_sentinel;
    }

    fsize_t FORCE_INLINE
    size() const noexcept
    {
        return std::popcount(m_set);
    }

    std::uint64_t FORCE_INLINE
    data() const noexcept
    {
        return m_set;
    }
};

template <fsize_t N, std::unsigned_integral index_t>
mask_type<index_t, N> FORCE_INLINE
make_mask(const index_t pc, const packed_vector<index_t, N> pcs) noexcept
{
    using cmp = cmp_intrinsics_wrangler<index_t, N>;

    return cmp::eq(broadcast<N>(pc), pcs);
}

/// @brief Get pc of the major group and its mask.
template <Kernel K, fsize_t N, std::ranges::input_range Rng>
std::tuple<typename K::index_t, mask_type<typename K::index_t, N>, unsigned>
    FORCE_INLINE
    major_group(const packed_vector<typename K::index_t, N> pcs,
                Rng&&                                       candidates) noexcept
{
    using index_t = K::index_t;
    using cmp     = cmp_intrinsics_wrangler<index_t, N>;
    using mask_t  = mask_type<index_t, N>;

    constexpr popcnt_helper<index_t, N> popcnt{};

    unsigned pops = 0;
    index_t  pc   = K::halt_pos;
    mask_t   mask{}; // empty mask

    for(index_t cpc: candidates)
    {
        const mask_t cmask =
            cmp::eq(broadcast<N>(static_cast<index_t>(cpc)), pcs);
        const unsigned cpops = popcnt(cmask);
        if(cpops > pops)
        {
            pops = cpops;
            pc   = cpc;
            mask = cmask;
        }
    }

    return {pc, mask, pops};
}

/// @brief Get pc of the first non-embty group and its mask.
template <Kernel K, fsize_t N, std::ranges::input_range Rng>
auto FORCE_INLINE
first_group(const packed_vector<typename K::index_t, N> pcs,
            Rng&&                                       candidates) noexcept
{
    using index_t = K::index_t;
    using cmp     = cmp_intrinsics_wrangler<index_t, N>;
    using mask_t  = mask_type<index_t, N>;

    constexpr popcnt_helper<index_t, N> popcnt{};

    unsigned pops = 0;
    index_t  pc   = K::halt_pos;
    mask_t   mask{}; // empty mask

    set_iterator i = candidates.begin();

    while(pops == 0 && i != candidates.end())
    {
        pc   = *i++;
        mask = cmp::eq(broadcast<N>(pc), pcs);
        pops = popcnt(mask);
    }

    return std::tuple{pc, mask, pops, i.data()};
}

/// @brief Vectorized counting sort.
/// @return number of halted blocks.
template <Kernel K, typename _Memory>
fsize_t
counting_sort(_Memory& in, _Memory& out) noexcept
{
    ASSERT(in.size() == out.size(), "Sizes must match");

    using index_t = K::index_t;

    constexpr fsize_t num_bins = K::num_blocks + 1;
    constexpr fsize_t N        = native_vector_extent_v<index_t>;

    constexpr popcnt_helper<index_t, N> popcnt{};

    ASSERT(in.size() % N == 0, "Size should be aligned to vector width");

    using index_vector_t = packed_vector<index_t, N>;

    constexpr index_t halt_pos = static_cast<index_t>(K::num_blocks);

    index_t bins[num_bins] = {};

    // histogram
    for(index_t pos = 0; pos < in.size(); pos += N)
    {
        const index_vector_t pcs =
            load<N>(in.template view<N>(pos).pc().data());
        for(const index_t pc: greedy_groups<K, N, false>(pcs))
            bins[pc] += popcnt(make_mask<N>(fit_block_index<K>(pc), pcs));
    }

    const fsize_t ret = bins[num_bins - 1];

    // prefix sum
    // no point in vectorizing this - num_bins is usually too small
    for(index_t i = 1; i < num_bins; ++i) bins[i] += bins[i - 1];

    // fill out
#if defined(__AVX512F__)
    using index_vector = packed_vector<index_t, N>;
    for(int i = in.size() - 1; i >= 0; i -= static_cast<int>(N))
    {
        index_vector vindex_out;
        index_vector vindex_in;
        bool         is_halt = true;
        for(int j = 0; j < static_cast<int>(N); ++j)
        {
            const int  ij = i - j;
            const int  j2 = N - j - 1;
            const auto pc = in.pc()[ij];
            is_halt &= pc == halt_pos;
            vindex_out[j2] = --bins[fit_in_bins(pc, halt_pos)];
            vindex_in[j2]  = ij;
        }

        auto f = [&]<typename Var>() noexcept
        {
            using wrangler =
                gather_scatter_intrinsics_wrangler<vartype<Var>, N, index_t>;

            wrangler::scatter(
                out.template get<Var>().data(),
                vindex_out,
                wrangler::gather(vindex_in, in.template get<Var>().data()));
        };

        if(is_halt)
            typelist::for_each(typename K::output_vars{}, f);
        else
            typelist::for_each(typename K::variables1{}, f);
    }
#else
    for(int i = in.size() - 1; i >= 0; --i)
        out[--bins[fit_in_bins(in.pc()[i], halt_pos)]] = in[i];
#endif

    return ret;
}

MK_COUNTING_SORTER(simd)

template <typename _Sorter>
struct is_counting_sorter : std::false_type
{
};

template <Kernel K, typename M>
struct is_counting_sorter<bve::CountingSorter<K, M>> : std::true_type
{
};

template <Kernel K, typename M>
struct is_counting_sorter<CountingSorter<K, M>> : std::true_type
{
};

/// @brief Sort selection enumeration.
enum SORT
{
    bve_INTROSORT     = bve::INTROSORT,     /*!< use bve::introsort */
    bve_COUNTING_SORT = bve::COUNTING_SORT, /*!< use bve::counting_sort */
    COUNTING_SORT                           /*!< use simd::counting_sort */
};

/// @brief Sorter selection helper class.
template <SORT sort_method>
struct select_sorter;

template <>
struct select_sorter<bve_INTROSORT>
{
    template <Kernel K, typename _Memory>
    using type = bve::SimpleSorter<K, _Memory>;
};

template <>
struct select_sorter<bve_COUNTING_SORT>
{
    template <Kernel K, typename _Memory>
    using type = bve::CountingSorter<K, _Memory>;
};

template <>
struct select_sorter<COUNTING_SORT>
{
    template <Kernel K, typename _Memory>
    using type = CountingSorter<K, _Memory>;
};

/// @brief Stream facade that allows concurrent exchange/computation.
template <Kernel K,
          template <typename, typename> typename _Vector,
          template <typename> typename Allocator,
          typename Sorter>
    requires(cache_line_size % simd_width == 0)
class ConcurrentStreamFacade :
    public bve::MemoryFacade<K, _Vector, Allocator, Sorter>
{
    using reg_names   = K::reg_names;
    using reg_types   = K::reg_types;
    using pc_var      = K::pc;
    using input_vars  = K::input_vars;
    using output_vars = K::output_vars;
    using index_t     = K::index_t;

    static constexpr index_t halt_pos  = K::halt_pos;
    static constexpr fsize_t npos      = static_cast<fsize_t>(-1);
    static constexpr fsize_t step_size = cache_line_capacity_v<index_t>;

    using facade = bve::MemoryFacade<K, _Vector, Allocator, Sorter>;

    fsize_t m_pos = npos;

    bool
    update_pos(const fsize_t nhalts) noexcept
    {
        const bool ret = nhalts >= step_size;
        m_pos = ret ? align_up(facade::mem().size() - nhalts, step_size) : npos;
        return ret;
    }

#if 0
    using vars = K::variables;
    void
    dump() const noexcept
    {
        auto columns = typelist::maybe_collect<std::tuple>(
            vars{},
            [&]<typename Var>() constexpr noexcept
            { return std::views::all(facade::mem().template get<Var>()); });

        typelist::for_each(vars{},
                           []<typename Var>()
                           {
                               using name       = varname<Var>;
                               constexpr auto l = intlist::length<name>;
                               constexpr auto a = intlist::array<name>;
                               constexpr std::string_view s(a, l);
                               fmt::print("{:>16},", s);
                           });
        putchar('\n');
        for(fsize_t i = 0; i < facade::size(); ++i)
        {
            // if(std::get<0>(columns)[i] != halt_pos)
            if(std::get<1>(columns)[i] == 0)
            {
                loom::for_each(columns,
                               [i](const auto& c)
                               { fmt::print("{:>16},", c[i]); });
                putchar('\n');
            }
        }
    }
#endif

  public:
    using facade::facade;

    bool
    sort() noexcept
    {
        if constexpr(is_counting_sorter<Sorter>::value)
            return update_pos(facade::sort());
        else
        {
            facade::sort();
            return update_pos(std::ranges::count(facade::mem().pc(), halt_pos));
        }
    }

    template <std::ranges::input_range Input>
        requires(
            std::is_nothrow_convertible_v<std::ranges::range_value_t<Input>,
                                          input_type<K>>)
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
#if 0
        dump();
#endif
    }
};

/// @brief Helper grouper range view over memory pool. Splits proxy views by
/// program counter value. Stops either on halt program counter or end of the
/// memory pool.
template <typename _Memory, auto halt_pos>
class memory_chunks :
    public std::ranges::view_interface<memory_chunks<_Memory, halt_pos>>
{
    using proxy_view = _Memory::view_type;

    proxy_view m_view;

    template <typename index_t>
    static fsize_t
    chunk_size(std::span<index_t> pcs) noexcept
    {
        const index_t pc = pcs.front();
        fsize_t       i  = 0;
        while(i < pcs.size() && pcs[i] == pc) ++i;
        return i;
    }

    class _iterator : public std::input_iterator_tag
    {
        proxy_view m_view{};
        fsize_t    m_pos{};

        fsize_t
        offset() const noexcept
        {
            return chunk_size(m_view.pc().subspan(m_pos));
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
            return m_pos == m_view.size() || m_view.pc()[m_pos] == halt_pos;
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

/// @brief Same as memory_chunks, but iterated proxy views are of constant size
/// @p N.
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
            return m_pos >= m_view.size() || m_view.pc()[m_pos] == halt_pos;
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

/// @brief Compiler for compact masked block execution.
template <typename Block, typename LabelMap, typename PCVar, fsize_t N, bool ts>
class make_compact_masked_block_compiler
{
    using index_t = vartype<PCVar>;

    using helper = minikernel_helper<Block, LabelMap, PCVar, N>;

    using Kernel      = helper::simd_kernel;
    using TempStorage = helper::simd_storage;

    using mask_t = mask_type<index_t, N>;

    static constexpr Kernel kernel{};

    template <typename _MainStorage>
    static TempStorage FORCE_INLINE
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
    static void FORCE_INLINE
    spill(_MainStorage& main_storage,
          TempStorage&  tmp_storage,
          const mask_t  mask) noexcept
    {
        typelist::for_each(
            typename helper::store_vars{},
            []<typename Var>(_MainStorage& main_storage,
                             TempStorage&  tmp_storage,
                             const mask_t  mask) noexcept
            {
                auto& orig     = main_storage.template get<Var>();
                using wrangler = blend_intrinsics_wrangler<vartype<Var>, N>;
                orig           = wrangler::blend(mask,
                                       orig,
                                       tmp_storage.template get<Var>());
            },
            main_storage,
            tmp_storage,
            mask);
    }

  public:
    struct type
    {
        const mask_t mask;

        template <typename _MainStorage>
        void FORCE_INLINE
        operator()(_MainStorage& main_storage) const noexcept
        {
            if constexpr(ts)
            {
                TempStorage tmp_storage = alloc(main_storage);
                kernel(tmp_storage);
                spill(main_storage, tmp_storage, mask);
            }
            else
                kernel(main_storage, mask);
        }
    };
};

/// @brief Helper typedef.
template <typename Block, typename LabelMap, typename PCVar, fsize_t N, bool ts>
using compact_masked_block_compiler =
    make_compact_masked_block_compiler<Block, LabelMap, PCVar, N, ts>::type;

/// @brief Create default step visitor for memory facade.
template <STITCH stitch>
struct visitor_selector
{
    template <typename _Memory, typename _CompiledBlocks, Kernel K, typename _>
    struct type
    {
        using memory_view = _Memory::view_type;

        void
        operator()(memory_view view) const noexcept
        {
            std::ranges::for_each(memory_chunks<_Memory, K::halt_pos>(view),
                                  [](memory_view view) noexcept
                                  {
                                      typelist::visit_at(
                                          view.pc().front(),
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

/// @brief Compute minimum gang size.
/// @tparam N lane size
/// @tparam UTR utilization threshold ratio
template <fsize_t N, typename UTR>
struct get_threshold :
    std::integral_constant<fsize_t, (N * UTR::num + UTR::den - 1) / UTR::den>
{
};

/// @brief Helper variable.
template <fsize_t N, typename UTR>
constexpr inline fsize_t get_threshold_v = get_threshold<N, UTR>::value;

/// @brief Validate utilization threshod.
template <typename UTR>
struct val_utr :
    std::bool_constant<(UTR::num > 0 && UTR::den > 0 && UTR::num <= UTR::den)>
{
};

/// @brief Helper variable.
template <typename UTR>
constexpr inline bool val_utr_v = val_utr<UTR>::value;

/// @brief Create masked execution step visitor.
template <>
struct visitor_selector<STITCH::MASKED>
{
    template <typename _Memory,
              typename _CompiledBlocks,
              Kernel K,
              typename UTR>
    struct type
    {
        using memory_view = _Memory::view_type;
        using index_t     = K::index_t;

        static constexpr fsize_t N         = native_vector_extent_v<index_t>;
        static constexpr fsize_t threshold = get_threshold_v<N, UTR>;
        static constexpr popcnt_helper<index_t, N> popcnt{};

        static_assert(threshold >= 2);

        using index_vector_t = packed_vector<index_t, N>;
        using mask_t         = mask_type<index_t, N>;

        void
        operator()(memory_view view) const noexcept
        {
            std::ranges::for_each(
                aligned_chunks<_Memory, N, K::halt_pos>(view),
                [](auto view) noexcept
                    requires(pv_extent<decltype(view)> == N)
                {
                    index_vector_t pcs    = load<N>(view.pc().data());
                    bool           repeat = true;

                    while(repeat)
                    {
                        fsize_t m = 0;

                        for(const index_t pc: greedy_groups<K, N, true>{pcs})
                        {
                            const mask_t mask = make_mask<N>(pc, pcs);

                            typelist::visit_at(
                                pc,
                                [=]<typename Block>() noexcept
                                { Block{mask}(view); },
                                _CompiledBlocks{},
                                sink);

                            m = std::max(m, static_cast<fsize_t>(popcnt(mask)));
                        }

                        repeat = m >= threshold;
                        m      = 0;
                        pcs    = load<N>(view.pc().data());
                    };
                });
        }

        void
        operator()(_Memory& mem) const noexcept
        {
            operator()(mem.view());
        }
    };
};

/// @brief Helper typedef.
template <STITCH stitch, typename... Args>
using select_visitor = visitor_selector<stitch>::template type<Args...>;

/// @brief Allocates memory aligned to native SIMD vector size.
template <typename T>
using SIMDAlignedAllocator =
    AlignedAllocator<T, get_native_vector_align<T>::value>;

/// @brief Basic SIMD backend.
/// @tparam K program to compile.
/// @tparam stitch execution method.
/// @tparam sort_method sorting algorithm to use.
/// @tparam concurrent allow concurrent computation in the stream.
/// @tparam UTR utilization threshold ratio for masked stitch.
template <Kernel K,
          STITCH stitch,
          SORT   sort_method,
          bool   concurrent = false,
          typename UTR      = std::ratio<1, 2>>
    requires(typelist::of_same_size<typename K::reg_types> && val_utr_v<UTR>)
class compiler
{
    using labelmap  = K::labelmap;
    using blocks    = K::blocks;
    using reg_names = K::reg_names;
    using reg_types = K::reg_types;
    using pc        = K::pc;

    static constexpr bool singular = K::singular;
    static constexpr auto halt_pos = K::halt_pos;

    template <typename Block>
    using compile_block = block_compiler<stitch, Block, labelmap, pc>;

    using compiled_blocks = typelist::map<compile_block, blocks>;

    template <typename T>
    using allocator = std::conditional_t<concurrent,
                                         CacheLineAlignedAllocator<T>,
                                         SIMDAlignedAllocator<T>>;

    using memory =
        bve::Memory<reg_types, reg_names, pc, std::vector, allocator>;
    using memory_view = memory::view_type;

    static constexpr SORT final_sort_method =
        singular ? bve_INTROSORT : sort_method;
    using sorter = select_sorter<final_sort_method>::template type<K, memory>;

    using stream_facade = std::conditional_t<
        concurrent,
        ConcurrentStreamFacade<K, std::vector, allocator, sorter>,
        bve::StreamFacade<K, std::vector, allocator, sorter>>;
    using memory_facade = bve::MemoryFacade<K, std::vector, allocator, sorter>;

    using visitor = select_visitor<stitch, memory, compiled_blocks, K, UTR>;

    class single_block_evaluator : public memory_facade
    {
        static constexpr typelist::head<compiled_blocks> block{};

      public:
        using program = K;
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
        using vars = K::variables;
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
                loom::for_each(columns,
                                [i](const auto& c)
                                { fmt::print("{},", c[i]); });
                putchar('\n');
            }
        }
#endif

      public:
        using program = K;
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
    using function =
        std::conditional_t<K::singular, single_block_evaluator, evaluator>;

    class stream : public stream_facade
    {
        static_assert(!K::singular);

      public:
        using program = K;
        using stream_facade::stream_facade;

        constexpr void
        operator()() noexcept
        {
            stream_facade::visit(visitor{});
        }
    };
};

/// @brief Determines how compact_compiler executes threads.
enum ReconvMethod
{
    GREEDY_FAIR,  /*!< Every PC in gang gets gets its time */
    HYBRID_MAJOR, /*!< Only PCs having the majority are executed selecting
                     between successors and greedy search when utilization
                     hits threshold */
    GREEDY_MAJOR, /*!< Only PCs having the majority are executed selecting from
                    greedy search */
    HYBRID_FAIR,  /*!< Prefers greedy major grouping, when utilization hits
                    threshold, switch to fair */
    STATIC_FIRST, /*!< Select first PC from successors falling back to
                     reminders set */
    STATIC_MAJOR  /*!< Select PC based on the majority from successors
                     falling  back to reminders set */
};

#define MK_CASE(IDX) \
    case IDX: MUSTTAIL return typelist::at<IDX, ccss>::eval(mem, next_mask, rem)

/// @brief threaded block dispatch implementation for SIMD predicated execution.
/// @copydetails vm::bse::threaded_dispatch
template <Kernel              K,
          typename K::index_t pc,
          template <typename> typename Compiler>
    requires(!allow_throws)
class threaded_dispatch
{
    using blocks  = K::blocks;
    using succs   = K::successors;
    using index_t = K::index_t;

    static constexpr index_t halt_pos = K::halt_pos;

    template <index_t I>
    using f = threaded_dispatch<K, I, Compiler>;

    using spec = sd_helper<K,
                           typelist::at<pc, blocks>,
                           typelist::at<pc, succs>,
                           f,
                           Compiler>;

    static constexpr fsize_t N = native_vector_extent_v<index_t>;

    static constexpr popcnt_helper<index_t, N> popcnt{};

    using blend_wrangler = blend_intrinsics_wrangler<index_t, N>;
    using index_vector_t = packed_vector<index_t, N>;

    using cmp    = cmp_intrinsics_wrangler<index_t, N>;
    using mask_t = mask_type<index_t, N>;

  public:
    template <typename _Memory>
    static std::uint64_t
    eval(_Memory& mem, mask_t mask, std::uint64_t rem) noexcept
    {
        if constexpr(pc != halt_pos)
        {
            using bkss   = spec::bkss;
            using ccss   = spec::ccss;
            using kernel = spec::kernel;

            constexpr auto table = intlist::stdarray<bkss>;

            kernel{mask}(mem);

            rem &= ~(1ul << pc); // unset evaluated block

            auto get_mask = [&](const index_t next_pc) noexcept
            { return cmp::eq(broadcast<N>(next_pc), mem.pc()); };

            if constexpr(spec::l == 1)
            {
                const index_t next_pc = table[0];

                if(next_pc != halt_pos)
                {
                    const mask_t   next_mask = get_mask(next_pc);
                    const unsigned next_pops = popcnt(next_mask);

                    if(next_pops) MUSTTAIL
                    return typelist::head<ccss>::eval(mem, next_mask, rem);
                }
            }
            else if constexpr(spec::l >= 2)
            {
                index_t  next_branch{};
                mask_t   next_mask{};
                unsigned next_pops{};

                for(index_t i = 0; i < spec::l; ++i)
                {
                    const index_t next_pc = table[i];

                    if(next_pc != halt_pos)
                    {
                        const mask_t   cmask = get_mask(next_pc);
                        const unsigned cpops = popcnt(cmask);

                        rem |= (1ul & (cpops > 0u)) << next_pc;

                        if(cpops > next_pops)
                        {
                            next_branch = i;
                            next_mask   = cmask;
                            next_pops   = cpops;
                        }
                    }
                }

                if(next_pops)
                {
                    if constexpr(spec::l == 2)
                    {
                        if(next_branch) MUSTTAIL
                        return snd_t<ccss>::eval(mem, next_mask, rem);
                        else MUSTTAIL return fst_t<ccss>::eval(mem,
                                                               next_mask,
                                                               rem);
                    }
                    else if constexpr(spec::l == 3)
                        switch(next_branch)
                        {
                            MK_CASE(0);
                            MK_CASE(1);
                            MK_CASE(2);
                        }
                    else if constexpr(spec::l == 4)
                        switch(next_branch)
                        {
                            MK_CASE(0);
                            MK_CASE(1);
                            MK_CASE(2);
                            MK_CASE(3);
                        }
                    else if constexpr(spec::l == 5)
                        switch(next_branch)
                        {
                            MK_CASE(0);
                            MK_CASE(1);
                            MK_CASE(2);
                            MK_CASE(3);
                            MK_CASE(4);
                        }
                    else if constexpr(spec::l == 6)
                        switch(next_branch)
                        {
                            MK_CASE(0);
                            MK_CASE(1);
                            MK_CASE(2);
                            MK_CASE(3);
                            MK_CASE(4);
                            MK_CASE(5);
                        }
                    else if constexpr(spec::l == 7)
                        switch(next_branch)
                        {
                            MK_CASE(0);
                            MK_CASE(1);
                            MK_CASE(2);
                            MK_CASE(3);
                            MK_CASE(4);
                            MK_CASE(5);
                            MK_CASE(6);
                        }
                    else if constexpr(spec::l == 8)
                        switch(next_branch)
                        {
                            MK_CASE(0);
                            MK_CASE(1);
                            MK_CASE(2);
                            MK_CASE(3);
                            MK_CASE(4);
                            MK_CASE(5);
                            MK_CASE(6);
                            MK_CASE(7);
                        }
                    else
                        static_assert(false, "Add more cases");
                }
            }
        }

        return rem;
    }
};

#undef MK_CASE

/// @brief Compiler for compact SIMD execution.
/// @tparam ts allocate special temporal storage for currently
/// executed basic block
/// @tparam use_tbd use threaded block dispatch
/// @tparam rcnv reconvergence method to use
/// @tparam UTR utilization threshold for HYBRID_MAJOR method
template <Kernel       K,
          bool         ts,
          bool         use_tbd = true,
          ReconvMethod rcnv    = STATIC_MAJOR,
          typename UTR         = std::ratio<1, 16>>
    requires(typelist::of_same_size<typename K::reg_types> && val_utr_v<UTR>)
class compact_compiler
{
    using labelmap  = K::labelmap;
    using blocks    = K::blocks;
    using reg_names = K::reg_names;
    using reg_types = K::reg_types;
    using pc        = K::pc;
    using index_t   = K::index_t;
    using succs     = K::successors;

    static constexpr bool    singular = K::singular;
    static constexpr fsize_t N        = native_vector_extent_v<index_t>;
    static constexpr packed_vector<index_t, N> halt_pos =
        broadcast<N>(K::halt_pos);
    static constexpr fsize_t threshold = get_threshold_v<N, UTR>;

    template <typename... Ts>
    using memory = KernelMemory<Ts..., N>;

    using facade = bse::MemoryFacade<
        K,
        memory,
        decltype([]() constexpr noexcept -> packed_vector<index_t, N>
                 { return broadcast<N>(static_cast<index_t>(0)); })>;

    class single_block_evaluator : facade
    {
        template <typename I, typename _LabelMap>
        using simd_eval = eval<I, _LabelMap, N>;

        using Kernel =
            standard_compiler<typelist::head<blocks>, labelmap, simd_eval>;

      public:
        using program = K;

        template <typename... Args>
        auto
        operator()(const Args... args) noexcept
        {
            facade::set_inputs(args...);
            facade::template step<Kernel>();
            return facade::get_outputs();
        }
    };

    class evaluator : facade
    {
        template <typename Block>
        using compile_block =
            compact_masked_block_compiler<Block, labelmap, pc, N, ts>;

        template <index_t I>
        using spec_block = threaded_dispatch<K, I, compile_block>;

        using compiled_blocks = typelist::map<compile_block, blocks>;

        using cmp    = cmp_intrinsics_wrangler<index_t, N>;
        using lrh    = logical_reduce_helper<index_t, N>;
        using mask_t = mask_type<index_t, N>;

        static constexpr mask_t
        full_mask() noexcept
        {
#ifdef __AVX512F__
            return static_cast<mask_t>(-1);
#else
            return broadcast<N>(static_cast<index_t>(-1));
#endif
        }

        bool FORCE_INLINE
        not_done() const noexcept
        {
            return !lrh::land(cmp::eq(facade::pc(), halt_pos));
        }

        void FORCE_INLINE
        exec(const index_t pc, const mask_t mask) noexcept
        {
            typelist::visit_at(
                pc,
                [&]<typename Runner>() noexcept { facade::step(Runner{mask}); },
                compiled_blocks{},
                sink);
        }

      public:
        template <typename... Args>
        auto
        operator()(const Args... args) noexcept
        {
            facade::set_inputs(args...);

            if constexpr(use_tbd)
            {
                using spec_blocks = intlist::map_t<
                    spec_block,
                    intlist::range<static_cast<index_t>(0),
                                   static_cast<index_t>(K::num_blocks)>>;

                std::uint64_t rem = facade::step(
                    [](auto& mem, mask_t mask, std::uint64_t rem) noexcept
                    { return spec_block<0>::eval(mem, mask, rem); },
                    full_mask(),
                    0ul);

                while(not_done())
                {
                    const auto [pc, mask, pops] =
                        major_group<K, N>(facade::pc(),
                                          simple_set<index_t>(rem));

                    rem = typelist::visit_at(
                        pc,
                        [&, mask]<typename D>(std::uint64_t rem) noexcept
                        {
                            return facade::step(
                                [&](auto&         mem,
                                    mask_t        mask,
                                    std::uint64_t rem) noexcept
                                { return D::eval(mem, mask, rem); },
                                mask,
                                rem);
                        },
                        spec_blocks{},
                        copy_id,
                        rem);
                }
            }
            else if constexpr(rcnv == GREEDY_FAIR)
            {
                while(not_done())
                    for(const index_t pc:
                        greedy_groups<K, N, true>{facade::pc()})
                        exec(pc, make_mask<N>(pc, facade::pc()));
            }
            else if constexpr(rcnv == HYBRID_MAJOR)
            {
                index_t  pc                   = 0;
                mask_t   mask                 = full_mask();
                unsigned pops                 = N;
                bool     use_successor_groups = true;

                while(not_done())
                {
                    exec(pc, mask);

                    if(use_successor_groups)
                        std::tie(pc, mask, pops) =
                            major_group<K, N>(facade::pc(),
                                              successor_groups<K>{pc});
                    else
                        std::tie(pc, mask, pops) = major_group<K, N>(
                            facade::pc(),
                            greedy_groups<K, N, true>{facade::pc()});

                    use_successor_groups = pops >= threshold;
                }
            }
            else if constexpr(rcnv == GREEDY_MAJOR)
            {
                index_t  pc   = 0;
                mask_t   mask = full_mask();
                unsigned pops = N;

                while(not_done())
                {
                    exec(pc, mask);

                    std::tie(pc, mask, pops) = major_group<K, N>(
                        facade::pc(),
                        greedy_groups<K, N, true>{facade::pc()});
                }
            }
            else if constexpr(rcnv == HYBRID_FAIR)
            {
                index_t                   pc       = 0;
                mask_t                    mask     = full_mask();
                unsigned                  pops     = N;
                bool                      do_major = true;
                greedy_groups<K, N, true> g;

                while(not_done())
                {
                    if(do_major)
                        exec(pc, mask);
                    else
                        for(const index_t gpc: g)
                            exec(gpc, make_mask<N>(gpc, facade::pc()));

                    g = greedy_groups<K, N, true>{facade::pc()};
                    std::tie(pc, mask, pops) =
                        major_group<K, N>(facade::pc(), g);
                    do_major = pops >= threshold;
                }
            }
            else if constexpr(rcnv == STATIC_FIRST)
            {
                index_t       pc                   = 0;
                mask_t        mask                 = full_mask();
                unsigned      pops                 = N;
                std::uint64_t rem                  = 0;
                bool          use_successor_groups = true;

                while(not_done())
                {
                    exec(pc, mask);

                    std::uint64_t nrem;
                    if(use_successor_groups)
                        std::tie(pc, mask, pops, nrem) =
                            first_group<K, N>(facade::pc(),
                                              successor_groups<K>{pc});
                    else
                        std::tie(pc, mask, pops, nrem) =
                            first_group<K, N>(facade::pc(),
                                              simple_set<index_t>{rem});

                    rem &= ~(1ul << pc); // unset executed block
                    rem |= nrem;         // add remainders

                    use_successor_groups = pops > 0;
                }
            }
            else if constexpr(rcnv == STATIC_MAJOR)
            {
                index_t       pc                   = 0;
                mask_t        mask                 = full_mask();
                unsigned      pops                 = N;
                std::uint64_t rem                  = 0;
                bool          use_successor_groups = true;

                while(not_done())
                {
                    exec(pc, mask);

                    if(use_successor_groups)
                    {
                        auto ngs = successor_groups<K>{pc};
                        rem |= ngs.data();
                        std::tie(pc, mask, pops) =
                            major_group<K, N>(facade::pc(), ngs);
                    }
                    else
                    {
                        auto sgs = simple_set<index_t>{rem};
                        rem |= sgs.data();
                        std::tie(pc, mask, pops) =
                            major_group<K, N>(facade::pc(), sgs);
                    }

                    rem &= ~(1ul << pc); // unset executed block

                    use_successor_groups = pops > 0;
                }
            }

            return facade::get_outputs();
        }
    };

  public:
    using type =
        std::conditional_t<singular, single_block_evaluator, evaluator>;
};

/// @brief Hybrid masked compiler.
template <Kernel       K,
          bool         ts   = false,
          ReconvMethod rcnv = STATIC_MAJOR,
          typename UTRt     = std::ratio<1, 2>,
          typename UTRh     = UTRt>
    requires(!K::singular && typelist::of_same_size<typename K::reg_types> &&
             val_utr_v<UTRt> && val_utr_v<UTRh>)
class hybrid_compiler
{
    using labelmap  = K::labelmap;
    using blocks    = K::blocks;
    using reg_names = K::reg_names;
    using reg_types = K::reg_types;
    using pc        = K::pc;
    using index_t   = K::index_t;
    using vars      = K::variables;

    static constexpr fsize_t N = native_vector_extent_v<index_t>;

    template <typename T>
    using allocator = CacheLineAlignedAllocator<T>;

    using memory =
        bve::Memory<reg_types, reg_names, pc, std::vector, allocator>;
    using memory_view = memory::view_type;

    using facade = ConcurrentStreamFacade<K,
                                          std::vector,
                                          allocator,
                                          CountingSorter<K, memory>>;

    template <typename View>
        requires(pv_extent<View> == N)
    class LocalStorage : public KernelMemory<reg_types, reg_names, pc, N>
    {
        View m_view;

      public:
        LocalStorage(View view) noexcept : m_view(view)
        {
            typelist::for_each(
                vars{},
                []<typename Var>(View view, LocalStorage& local) noexcept
                {
                    local.template get<Var>() =
                        load<N>(view.template get<Var>().data());
                },
                m_view,
                *this);
        }

        ~LocalStorage()
        {
            typelist::for_each(
                vars{},
                []<typename Var>(View view, LocalStorage& storage) noexcept
                {
                    store<N>(view.template get<Var>().data(),
                             storage.template get<Var>());
                },
                m_view,
                *this);
        }
    };

    struct visitor
    {
        static constexpr fsize_t gthreshold = get_threshold_v<N, UTRt>;
        static constexpr fsize_t sthreshold =
            get_threshold_v<cache_line_capacity_v<index_t> * K::num_blocks,
                            UTRt>;
        static constexpr fsize_t hthreshold = get_threshold_v<N, UTRh>;

        static constexpr popcnt_helper<index_t, N> popcnt{};

        using index_vector_t = packed_vector<index_t, N>;
        using mask_t         = mask_type<index_t, N>;

        template <typename Block>
        using compile_block =
            compact_masked_block_compiler<Block, labelmap, pc, N, ts>;

        using compiled_blocks = typelist::map<compile_block, blocks>;

        void
        operator()(memory_view view) const noexcept
        {
            std::ranges::for_each(
                aligned_chunks<memory, N, K::halt_pos>(view),
                [t = gthreshold * (view.size() > sthreshold)]<typename View>(
                    View view) noexcept
                    requires(pv_extent<View> == N)
                {
                    LocalStorage   storage(view);
                    index_vector_t pcs                  = storage.pc();
                    bool           use_successor_groups = true;

                    auto [pc, mask, pops] =
                        major_group<K, N>(pcs, greedy_groups<K, N, true>{pcs});

                    auto exec = [&]() noexcept
                    {
                        typelist::visit_at(
                            pc,
                            [&]<typename Block>() noexcept
                            { Block{mask}(storage); },
                            compiled_blocks{},
                            sink);
                    };

                    if constexpr(rcnv == HYBRID_MAJOR)
                    {
                        while(pops > t)
                        {
                            exec();

                            pcs = storage.pc();

                            if(use_successor_groups)
                                std::tie(pc, mask, pops) =
                                    major_group<K, N>(pcs,
                                                      successor_groups<K>{pc});
                            else
                                std::tie(pc, mask, pops) = major_group<K, N>(
                                    pcs,
                                    greedy_groups<K, N, true>{pcs});

                            use_successor_groups = pops >= hthreshold;
                        };
                    }
                    else if constexpr(rcnv == STATIC_MAJOR)
                    {
                        std::uint64_t rem{};
                        while(pops > t)
                        {
                            exec();

                            pcs = storage.pc();

                            if(use_successor_groups)
                            {
                                auto ngs = successor_groups<K>{pc};
                                rem |= ngs.data();
                                std::tie(pc, mask, pops) =
                                    major_group<K, N>(pcs, ngs);
                            }
                            else
                            {
                                auto sgs = simple_set<index_t>{rem};
                                rem |= sgs.data();
                                std::tie(pc, mask, pops) =
                                    major_group<K, N>(pcs, sgs);
                            }

                            use_successor_groups = pops >= hthreshold;
                        };
                    }
                });
        }

        void
        operator()(memory& mem) const noexcept
        {
            operator()(mem.view());
        }
    };

  public:
    class function : public facade
    {

      public:
        using program = K;
        using facade::facade;

        template <bool zip_outputs = false, typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            facade::set_inputs(std::forward<Args>(args)...);

            while(!facade::done())
            {
                facade::visit(visitor{});
                facade::sort();
            }

            if constexpr(zip_outputs)
                return facade::get_output_zipped();
            else
                return facade::get_outputs();
        }
    };

    struct stream : public facade
    {
        using program = K;
        using facade::facade;

        void
        operator()() noexcept
        {
            facade::visit(visitor{});
        }
    };
};

/// @brief Alias for function-like invocation.
template <Kernel K,
          STITCH stitch      = MASKED,
          SORT   sort_method = COUNTING_SORT,
          typename UTR       = std::ratio<1, 2>>
using function = compiler<K, stitch, sort_method, false, UTR>::function;

/// @brief Alias for streaming invocation.
template <Kernel K,
          STITCH stitch      = MASKED,
          SORT   sort_method = COUNTING_SORT,
          bool   concurrent  = false,
          typename UTR       = std::ratio<1, 2>>
    requires(!K::singular)
using stream = compiler<K, stitch, sort_method, concurrent, UTR>::stream;

/// @brief Alias for hybrid function-like invocation.
template <Kernel       K,
          bool         ts   = false,
          ReconvMethod rcnv = STATIC_MAJOR,
          typename UTRt     = std::ratio<1, 2>,
          typename UTRh     = std::ratio<1, 8>>
using hybrid_function = hybrid_compiler<K, ts, rcnv, UTRt, UTRh>::function;

/// @brief Alias for hybrid streaming invocation.
template <Kernel       K,
          bool         ts   = false,
          ReconvMethod rcnv = STATIC_MAJOR,
          typename UTRt     = std::ratio<1, 2>,
          typename UTRh     = std::ratio<1, 8>>
using hybrid_stream = hybrid_compiler<K, ts, rcnv, UTRt, UTRh>::stream;

/// @brief Alias for compact invocation.
template <Kernel       K,
          bool         ts      = false,
          bool         use_tbd = true,
          ReconvMethod rcnv    = STATIC_MAJOR,
          typename UTR         = std::ratio<1, 16>>
using compact_function = compact_compiler<K, ts, use_tbd, rcnv, UTR>::type;
} // namespace loom::vm::simd
