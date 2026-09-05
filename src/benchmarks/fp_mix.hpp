// -*- C++ -*-
// fp_mix.hpp
//

#pragma once

#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

#include <cmath>
#include <concepts>
#include <utility>

class fp_mix_ref
{
    template <std::floating_point T>
    static T
    mix1(const T a, const T b, const T c) noexcept
    {
        const T d = std::fma(a, b, c);
        const T e = std::abs(d - (b / c));
        const T f = std::abs(a * c - d);
        const T g = std::min(e, f);
        return std::sqrt(f) / std::sqrt(g);
    }

    template <std::floating_point T>
    static T
    mix2(const T a, const T b) noexcept
    {
        const T x = (a + b) * (a - b);
        const T y = (x - a) / (x + b);
        return std::max(x, y);
    }

  public:
    template <std::floating_point T>
    T
    operator()(const T a, const T b, const T c) const noexcept
    {
        const T x1 = mix1(a, b, c);
        const T x2 = mix1(a, c, b);
        const T x3 = mix1(b, a, c);
        const T x4 = mix1(c, b, a);

        const T y1 = mix2(x1, x2);
        const T y2 = mix2(x3, x4);

        const T z1 = mix1(x1 + x3, y1 - y2, a * c);
        const T z2 = mix1(x2 + x4, y2 - y1, b * c);

        return y1 < y2 ? z1 * z2 : z2 / z1;
    }
};

// clang-format off
TMACRO8(mix1, dst, a, b, c, d, e, f, g)
    TFMA_CS(d, a, b, c),
    TDIV_CS(e, b, c),
    TSUB_CS(e, d, e),
    TABS_CS(e, e),
    TFMS_CS(f, a, c, d),
    TABS_CS(f, f),
    TMIN_CS(g, e, f),
    TSQRT_CS(f, f),
    TRSQRT_CS(g, g),
    TMUL_CS(dst, f, g)
MACRO_END;

TMACRO7(mix2, dst, a, b, x, y, tmp1, tmp2)
    TADD_CS(tmp1, a, b),
    TSUB_CS(tmp2, a, b),
    TMUL_CS(x, tmp1, tmp2),
    TSUB_CS(tmp1, x, a),
    TADD_CS(tmp2, x, b),
    TDIV_CS(y, tmp1, tmp2),
    TMAX_CS(dst, x, y)
MACRO_END;

template <std::floating_point T>
TKERNEL3(fp_mix_k, a, b, c)
    INSERT_TMACRO(mix1, CS(x1), CS(a), CS(b), CS(c), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3)),
    INSERT_TMACRO(mix1, CS(x2), CS(a), CS(c), CS(b), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3)),
    INSERT_TMACRO(mix1, CS(x3), CS(b), CS(a), CS(c), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3)),
    INSERT_TMACRO(mix1, CS(x4), CS(c), CS(b), CS(a), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3)),

    INSERT_TMACRO(mix2, CS(y1), CS(x1), CS(x2), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3)),
    INSERT_TMACRO(mix2, CS(y2), CS(x3), CS(x4), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3)),

    TADD(tmp0, x1, x3),
    TSUB(tmp1, y1, y2),
    TMUL(tmp2, a, c),
    INSERT_TMACRO(mix1, CS(z1), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3), CS(tmp4), CS(tmp5), CS(tmp6)),

    TADD(tmp0, x2, x4),
    TSUB(tmp1, y2, y1),
    TMUL(tmp2, b, c),
    INSERT_TMACRO(mix1, CS(z2), CS(tmp0), CS(tmp1), CS(tmp2), CS(tmp3), CS(tmp4), CS(tmp5), CS(tmp6)),

    TMUL(tmp0, z1, z2),
    TDIV(tmp1, z2, z1),
    TLT(tmp2, y1, y2),
    TCMOV(a, tmp2, tmp0, tmp1)
TKERNEL_END1(a);
