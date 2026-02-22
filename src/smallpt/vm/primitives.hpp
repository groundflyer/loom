// -*- C++ -*-
// primitives.hpp
//

#include "linal.hpp"

#pragma once

namespace loom::vm
{
// clang-format off

/// @brief Ray-disk intersection.
/// @tparam hit bool is hit: dst
/// @tparam t root: dst
/// @tparam o vec[3] ray origin
/// @tparam d vec[3] ray direction
/// @tparam p vec[3] disk position
/// @tparam n vec[3] disk normal
/// @tparam r disk radius
/// @tparam vtmp vec[3] storage for temp vectors
/// @tparam tmp storage for temp value
template <std::floating_point T>
TMACRO9(isect_disk, hit, t, o, d, p, n, r, vtmp, tmp)
    INSERT_TMACRO(vec_vec3_sub, vtmp, p, o),
    INSERT_TMACRO(vec3_dot, t, vtmp, n),
    INSERT_TMACRO(vec3_dot, tmp, d, n),
    TDIV_CS(t, t, tmp),
    TGE_CS(hit, t, CS(0)),
    IPJMP2_CS(hit, isect_disk_end, isect_disk_rest),
    LABEL(isect_disk_rest),
    INSERT_TMACRO(vec3_scalar_mul, vtmp, d, t),
    INSERT_TMACRO(vec_vec3_add, vtmp, vtmp, o),
    INSERT_TMACRO(vec3_dist, tmp, vtmp, p),
    TLT_CS(hit, tmp, r),
    LABEL(isect_disk_end)
MACRO_END;

/// @brief Solve quadratic equation @f$ax^{2} + bx + c@f$ for non-complex cases.
/// Port of loom::quadratic.
/// @tparam s bool solved?
/// @tparam x root
// all params invalidated
template <std::floating_point T>
TMACRO6(solve_quadratic, s, x, a, b, c, tmp)
    TMUL_CS(tmp, b, b),
    TMUL_CS(c, c, a),
    TMUL_CS(c, c, CS(4)),
    TSUB_CS(c, tmp, c),         // c <- D
    TMUL_CS(tmp, a, CS(0.5)),   // tmp <- factor
    TSQRT_CS(s, c),             // s <- sqrt(D)
    TNEG_CS(b, b),              // b <- (-b)
    TADD_CS(a, b, s),           // a <- (-b)+sqrt(D)
    TMUL_CS(a, a, tmp),         // a <- x1
    TSUB_CS(x, b, s),           // x <- (-b)-sqrt(D)
    TMUL_CS(x, x, tmp),         // x <- x2
    TLT_CS(tmp, a, CS(0)),      // tmp <- x1<0
    IPJMP2_CS(tmp, sq_mid, sq_tail),
    LABEL(sq_mid),
    TLT_CS(tmp, x, CS(0)),      // tmp <- x2<0
    TMIN_CS(b, a, x),           // b <- min(x1, x2)
    TCMOV_CS(x, tmp, a, b),     // x <- (x2<0) ? x1 : x2
    LABEL(sq_tail),
    TMUL_CS(s, c, x),           // s <- D*x
    TGE_CS(s, s, CS(0))         // s <- (D*x) >= 0
MACRO_END;

/// @brief Ray-sphere intersection.
template <std::floating_point T>
TMACRO9(isect_sphere, hit, t, o, d, p, r, tmp1, tmp2, tmp3)
    INSERT_TMACRO(vec_vec3_sub, o, o, p),        // o <- pc
    TMUL_CS(r, r, r),                            // r <- r*r
    INSERT_TMACRO(vec3_length2, t, o),           // t <- length2(pc)
    TSUB_CS(tmp2, t, r),                         // tmp2 <- c
    INSERT_TMACRO(vec3_scalar_mul, o, o, CS(2)), // o <- pc*2
    INSERT_TMACRO(vec3_dot, tmp1, d, o),         // tmp1 <- b
    INSERT_TMACRO(vec3_length2, r, d),           // r <- a
    INSERT_TMACRO(solve_quadratic, hit, t, r, tmp1, tmp2, tmp3)
MACRO_END;
} // namespace loom::vm
