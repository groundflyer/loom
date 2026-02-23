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

/// @brief Ray-triangle intersection.
/// @cite 10.1080/10867651.1997.10487468
template <std::floating_point T>
TMACRO10(isect_triangle, hit, t, o, d, vert0, vert1, vert2, tmp1, tmp2, vtmp)
    INSERT_TMACRO(vec_vec3_sub, vert1, vert1, vert0),            // vert1 <- edge1
    INSERT_TMACRO(vec_vec3_sub, vert2, vert2, vert0),            // vert2 <- edge2
    INSERT_TMACRO(vec3_cross, vtmp, d, vtmp, hit),               // vtmp <- pvec
    INSERT_TMACRO(vec3_dot, t, vert1, vert2),                    // t <- det
    TABS_CS(t, t),                                               // t <- abs(det)
    TGE_CS(hit, t, CS(0.001)),                                   // hit <- abs(det)>=0.001
    IPJMP2_CS(hit, isect_triangle_end, isect_triangle_mid1),     // if hit then mid1 else end
    LABEL(isect_triangle_mid1),
    TDIV_CS(t, CS(1), t),                                        // t <- inv_det
    INSERT_TMACRO(vec_vec3_sub, vert0, o, vert0),                // vert0 <- tvec
    INSERT_TMACRO(vec3_dot, tmp1, vert0, vtmp),                  // tmp1 <- dot(tvec, pvec)
    TMUL_CS(tmp1, tmp1, t),                                      // tmp1 <- u
    TGE_CS(hit, tmp1, CS(0)),                                    // hit <- u>=0
    TLE_CS(tmp2, tmp1, CS(1)),                                   // tmp2 <- u<=1
    ILAND_CS(hit, hit, tmp2),                                    // hit <- (u>=0)&&(u<=1)
    IPJMP2_CS(hit, isect_triangle_end, isect_triangle_mid2),     // if hit then mid2 else end
    LABEL(isect_triangle_mid2),
    INSERT_TMACRO(vec3_cross, vtmp, vert0, vert1, tmp2),         // vtmp <- qvec
    INSERT_TMACRO(vec3_dot, tmp2, d, vtmp),                      // tmp2 <- dot(d, qvec)
    TMUL_CS(tmp2, tmp2, t),                                      // tmp2 <- v
    TGE_CS(hit, tmp2, CS(0)),                                    // hit <- v>=0
    TADD_CS(tmp1, tmp1, tmp2),                                   // tmp1 <- u+v
    TLE_CS(tmp1, tmp1, CS(1)),                                   // tmp1 <- (u+v)<=1
    ILAND_CS(hit, hit, tmp1),                                    // hit <- (v>=0)&&(v+u<=1)
    IPJMP2_CS(hit, isect_triangle_end, isect_triangle_mid3),     // if hit then mid3 else end
    LABEL(isect_triangle_mid3),
    INSERT_TMACRO(vec3_dot, tmp1, vert2, vtmp),                  // tmp1 <- dot(edge2, qvec)
    TMUL_CS(t, tmp1, t),                                         // t <- dot(edge2, qvec)*inv_det
    IMOV_CS(hit, CS(1)),                                         // hit <- true
    LABEL(isect_triangle_end)
MACRO_END;
} // namespace loom::vm
