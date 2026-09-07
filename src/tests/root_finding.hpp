// -*- C++ -*-
// root_finding.hpp
//

#pragma once

#include <common/functional.hpp>
#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

#include <fmt/core.h>

// clang-format off
// a * x^3 + b * x^2 + c * x + d
TMACRO4(eval, y, x, t2, t3)
    TMUL_CS(t2, x, x),
    TMUL_CS(t3, t2, x),
    TFMA_CS(y, CS(c), x, CS(d)),
    TFMA_CS(y, CS(b), t2, y),
    TFMA_CS(y, CS(a), t3, y)
MACRO_END;

// root condition
// signbit(eval_at(ks, l)) != signbit(eval_at(ks, r))
TMACRO6(root_condition, y, l, r, t1, t2, t3)
    INSERT_TMACRO(eval, y, l, t2, t3),
    INSERT_TMACRO(eval, t1, r, t2, t3),
    TLT_CS(y, y, CS(0)),
    TLT_CS(t1, t1, CS(0)),
    INEQ_CS(y, y, t1)
MACRO_END;

// find_bounds
// saves to: l, r
// l = -10
// r = 10
// rc = root_condition
// rc = rc != 0
// pjmp2 rc loop end
// loop:
// l *= 10
// r *= 10
// rc = root_condition
// rc = rc != 0
// pjmp2 rc loop end
// end
TMACRO4(find_bounds, rc, t1, t2, t3)
    TMOV_CS(CS(l), CS(-10)),
    TMOV_CS(CS(r), CS(10)),
    INSERT_TMACRO(root_condition, rc, CS(l), CS(r), t1, t2, t3),
    IPJMP2_CS(rc, fb_loop, fb_end),
    LABEL(fb_loop),
    TMUL_CS(CS(l), CS(l), CS(10)),
    TMUL_CS(CS(r), CS(r), CS(10)),
    INSERT_TMACRO(root_condition, rc, CS(l), CS(r), t1, t2, t3),
    IPJMP2_CS(rc, fb_loop, fb_end),
    LABEL(fb_end)               // will be replaced with HaltLabel
MACRO_END;

// median
// l + (r - l) * 0.5
TMACRO1(median, s)
    TSUB_CS(s, CS(r), CS(l)),
    TMUL_CS(s, s, CS(0.5)),
    TADD_CS(s, s, CS(l))
MACRO_END;

template <std::floating_point T>
TKERNEL5(eval_program, a, b, c, d, x)
    INSERT_TMACRO(eval, CS(r), CS(x), CS(t1), CS(t2))
TKERNEL_END1(r);

template <std::floating_point T>
TKERNEL6(root_condition_program, a, b, c, d, l, r)
    INSERT_TMACRO(root_condition, CS(rc), CS(l), CS(r), CS(t1), CS(t2), CS(t3))
KERNEL_END(IVAR(rc));

template <std::floating_point T>
TKERNEL4(find_bounds_program, a, b, c, d)
    INSERT_TMACRO(find_bounds, CS(rc), CS(t1), CS(t2), CS(t3))
TKERNEL_END2(l, r);

template <std::floating_point T, typename errt>
TKERNEL4(bisection, a, b, c, d)
    INSERT_TMACRO(find_bounds, CS(y), CS(t1), CS(t2), CS(t3)),
    INSERT_TMACRO(median, CS(s)),
    INSERT_TMACRO(eval, CS(y), CS(s), CS(t1), CS(t2)),
    IMOV(i, 0),
    TABS(y, y),
    TGE_CS(CS(y), CS(y), errt),
    IPJMP2(y, HALT, fz_loop),
    LABEL(fz_loop),
    INSERT_TMACRO(root_condition, CS(y), CS(l), CS(s), CS(t1), CS(t2), CS(t3)),
    TCMOV(r, y, s, r),
    TCMOV(l, y, l, s),
    INSERT_TMACRO(median, CS(s)),
    INSERT_TMACRO(eval, CS(y), CS(s), CS(t1), CS(t2)),
    TABS(y, y),
    TGE_CS(CS(y), CS(y), errt),
    IADD(i, i, 1),
    IPJMP2(y, HALT, fz_loop)
KERNEL_END(TVAR(s), IVAR(i));
// clang-format on

template <std::floating_point T>
struct constants;

// setting error threshold to big value to avoid infinite loops
template <>
struct constants<float>
{
    static constexpr float err_threshold = 0.1f;
    using errt                           = CS(0.1);

    static constexpr float lbound = -100;
    static constexpr float rbound = 100;
};

template <>
struct constants<double>
{
    static constexpr double err_threshold = 0.1;
    using errt = CS(0.1);

    static constexpr double lbound        = -1e5;
    static constexpr double rbound        = 1e5;
};

namespace fn
{
template <std::floating_point T>
constexpr T
val_eval(T a, T b, T c, T d, T x) noexcept
{
    T x2 = x * x;
    T x3 = x2 * x;
    return loom::fma(
        a,
        x3,
        loom::fma(b, x2, loom::fma(c, x, d))); // a * x3 + b * x2 + c * x + d;
}

template <std::floating_point T>
constexpr bool
val_root_condition(T a, T b, T c, T d, T l, T r) noexcept
{
    return std::signbit(val_eval(a, b, c, d, l)) !=
           std::signbit(val_eval(a, b, c, d, r));
}

template <std::floating_point T>
std::tuple<T, T>
val_find_bounds(T a, T b, T c, T d) noexcept
{
    T l = -10;
    T r = 10;

    while(!val_root_condition(a, b, c, d, l, r))
    {
        l *= 10;
        r *= 10;
    }

    return {l, r};
}

template <std::floating_point T>
T
median(const T l, const T r) noexcept
{
    return l + (r - l) * static_cast<T>(0.5);
}

template <std::floating_point T>
std::tuple<T, loom::uint_of_size_t<sizeof(T)>>
val_cubic_fz(T a, T b, T c, T d) noexcept
{
    auto [l, r]                        = val_find_bounds(a, b, c, d);
    auto                             s = median(l, r);
    auto                             y = abs(val_eval(a, b, c, d, s));
    loom::uint_of_size_t<sizeof(T)> i = 0;
    while(y >= constants<T>::err_threshold)
    {
        const bool rc = val_root_condition(a, b, c, d, l, s);
        // if (rc)
        //     r = s;
        // else
        //     l = s;
        r = rc ? s : r;
        l = rc ? l : s;

        s = median(l, r);
        y = abs(val_eval(a, b, c, d, s));
        ++i;
    }

    return {s, i};
}
} // namespace fn

MK_NIEBLOID(val_eval);
MK_NIEBLOID(val_root_condition);
MK_NIEBLOID(val_find_bounds);
MK_NIEBLOID(val_cubic_fz);
