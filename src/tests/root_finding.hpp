// -*- C++ -*-
// root_finding.hpp
//

#pragma once

#include <lang/core.hpp>
#include <lang/macros.hpp>
#include <utils/functional.hpp>

#include <fmt/core.h>

// clang-format off
// a * x^3 + b * x^2 + c * x + d
TMACRO_BEGIN4(cubic_eval, result, x, t2, t3)
    TMUL_CS(t2, x, x),
    TMUL_CS(t3, t2, x),
    TMUL_CS(result, CS(a), t3),
    TMUL_CS(t3, CS(b), t2),
    TADD_CS(result, result, t3),
    TMUL_CS(t3, CS(c), x),
    TADD_CS(result, result, t3),
    TADD_CS(result, result, CS(d))
MACRO_END;

// root condition
// signbit(eval_at(ks, l)) != signbit(eval_at(ks, r))
TMACRO_BEGIN6(cubic_rc, y, l, r, t1, t2, t3)
    INSERT_TMACRO(cubic_eval, y, l, t2, t3),
    INSERT_TMACRO(cubic_eval, t1, r, t2, t3),
    TLT_CS(y, y, CS(0)),
    TLT_CS(t1, t1, CS(0)),
    TNEQ_CS(y, y, t1)
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
TMACRO_BEGIN4(cubic_fb, rc, t1, t2, t3)
    TMOV_CS(CS(l), CS(-10)),
    TMOV_CS(CS(r), CS(10)),
    INSERT_TMACRO(cubic_rc, rc, CS(l), CS(r), t1, t2, t3),
    TNEQ_CS(rc, rc, CS(0)),
    PJMP2_CS(rc, fb_loop, fb_end),
    LABEL(useless),             // to test label optimizations
    LABEL(fb_loop),
    TMUL_CS(CS(l), CS(l), CS(10)),
    TMUL_CS(CS(r), CS(r), CS(10)),
    INSERT_TMACRO(cubic_rc, rc, CS(l), CS(r), t1, t2, t3),
    TNEQ_CS(rc, rc, CS(0)),
    PJMP2_CS(rc, fb_loop, fb_end),
    LABEL(fb_end)               // will be replaced with HaltLabel
MACRO_END;

// median
// l + (r - l) * 0.5
TMACRO_BEGIN1(median, s)
    TSUB_CS(s, CS(r), CS(l)),
    TMUL_CS(s, s, CS(0.5)),
    TADD_CS(s, s, CS(l))
MACRO_END;

// find zero
TMACRO_BEGIN4(cubic_fz, t1, t2, t3, errt)
    INSERT_TMACRO(cubic_fb, CS(y), t1, t2, t3),
    INSERT_TMACRO(median, CS(s)),
    INSERT_TMACRO(cubic_eval, CS(y), CS(s), t1, t2),
    IMOV(i, 0),
    TABS(y, y),
    TGE_CS(CS(y), CS(y), errt),
    PJMP2(y, fz_end, fz_loop),
    LABEL(fz_loop),
    INSERT_TMACRO(cubic_rc, CS(y), CS(l), CS(s), t1, t2, t3),
    TCHOICE(r, y, s, r),
    TCHOICE(l, y, l, s),
    INSERT_TMACRO(median, CS(s)),
    INSERT_TMACRO(cubic_eval, CS(y), CS(s), t1, t2),
    TABS(y, y),
    TGE_CS(CS(y), CS(y), errt),
    IADD(i, i, 1),
    PJMP2(y, fz_end, fz_loop),
    LABEL(fz_end)
MACRO_END;

template <std::floating_point T>
TBEGIN5(cubic_eval_program, a, b, c, d, x)
    INSERT_TMACRO(cubic_eval, CS(r), CS(x), CS(t1), CS(t2))
TEND1(r);

template <std::floating_point T>
TBEGIN6(cubic_rc_program, a, b, c, d, l, r)
    INSERT_TMACRO(cubic_rc, CS(rc), CS(l), CS(r), CS(t1), CS(t2), CS(t3))
END(IVAR(rc));

template <std::floating_point T>
TBEGIN4(cubic_fb_program, a, b, c, d)
    INSERT_TMACRO(cubic_fb, CS(rc), CS(t1), CS(t2), CS(t3))
TEND2(l, r);

template <std::floating_point T, typename errt>
TBEGIN4(cubic_fz_program, a, b, c, d)
    INSERT_TMACRO(cubic_fz, CS(t1), CS(t2), CS(t3), errt)
END(TVAR(s), IVAR(i));
// clang-format on

template <std::floating_point T>
struct constants;

template <>
struct constants<float>
{
    static constexpr float err_threshold = 1e-1;
    static constexpr float lbound        = -100;
    static constexpr float rbound        = 100;

    using errt = CS(1e-1);
};

template <>
struct constants<double>
{
    static constexpr double err_threshold = 1e-3;
    static constexpr double lbound        = -1e6;
    static constexpr double rbound        = 1e6;

    using errt = CS(1e-3);
};

namespace fn
{
template <std::floating_point T>
constexpr T
val_cubic_eval(T a, T b, T c, T d, T x) noexcept
{
    T x2 = x * x;
    T x3 = x2 * x;
    return a * x3 + b * x2 + c * x + d;
}

template <std::floating_point T>
constexpr bool
val_cubic_rc(T a, T b, T c, T d, T l, T r) noexcept
{
    return std::signbit(val_cubic_eval(a, b, c, d, l)) !=
           std::signbit(val_cubic_eval(a, b, c, d, r));
}

template <std::floating_point T>
std::tuple<T, T>
val_cubic_fb(T a, T b, T c, T d) noexcept
{
    T l = -10;
    T r = 10;

    while(!val_cubic_rc(a, b, c, d, l, r))
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
std::tuple<T, lucid::uint_of_size_t<sizeof(T)>>
val_cubic_fz(T a, T b, T c, T d) noexcept
{
    auto [l, r]                        = val_cubic_fb(a, b, c, d);
    auto                             s = median(l, r);
    auto                             y = abs(val_cubic_eval(a, b, c, d, s));
    lucid::uint_of_size_t<sizeof(T)> i = 0;
    while(y >= constants<T>::err_threshold)
    {
        const bool rc = val_cubic_rc(a, b, c, d, l, s);
        // if (rc)
        //     r = s;
        // else
        //     l = s;
        r = rc ? s : r;
        l = rc ? l : s;

        s = median(l, r);
        y = abs(val_cubic_eval(a, b, c, d, s));
        ++i;
    }

    return {s, i};
    // return s;
}
} // namespace fn

MK_NIEBLOID(val_cubic_eval);
MK_NIEBLOID(val_cubic_rc);
MK_NIEBLOID(val_cubic_fb);
MK_NIEBLOID(val_cubic_fz);
