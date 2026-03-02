// -*- C++ -*-
/// @file primitives.hpp
/// @brief Intersection tests ported to the VM.

#pragma once

#include "linal.hpp"

namespace loom::vm
{
// clang-format off

/// @struct loom::vm::isect_disk
/// @brief Ray-disk intersection port.
/// @param[out] hit intersection test value destination
/// @param[out] t root (distance to intersection) destination
/// @param[in] o vec[3] ray origin
/// @param[in] d vec[3] ray direction
/// @param[in] p vec[3] disk position
/// @param[in] n vec[3] disk normal
/// @param[in] r disk radius
/// @param vtmp vec[3] storage for temporal vectors
/// @param tmp storage for temporal values

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

/// @struct loom::vm::solve_quadratic
/// @brief Solve quadratic equation @f$ax^{2} + bx + c@f$ for non-complex cases.
/// Port of loom::quadratic.
/// @param[out] s bool solved?
/// @param[out] x root
/// @param[in] a
/// @param[in] b
/// @param[in] c
/// @param tmp storage for temporal values
///
/// Values bound to @p a, @p b, @p c are invalidated.

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

/// @struct loom::vm::isect_sphere
/// @brief Ray-sphere intersection port.
/// @param[out] hit intersection test value destination
/// @param[out] t root (distance to intersection) destination
/// @param[in] o vec[3] ray origin
/// @param[in] d vec[3] ray direction
/// @param[in] p vec[3] sphere position
/// @param[in] r sphere radius
/// @param tmp1-3 storage for temporary values

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

using EPSILON = CS(0.0001);

/// @struct loom::vm::isect_triangle
/// @brief Ray-triangle intersection port.
/// @cite 10.1080/10867651.1997.10487468
/// @param[out] hit intersection test value destination
/// @param[out] t root (distance to intersection) destination
/// @param[in] o vec[3] ray origin
/// @param[in] d vec[3] ray direction
/// @param[in] vert0-2 triangle vertices
/// @param tmp1-2 storage for temporary values
/// @param vtmp vec[3] storage for temporary vectors

template <std::floating_point T>
TMACRO10(isect_triangle, hit, t, o, d, vert0, vert1, vert2, tmp1, tmp2, vtmp)
    INSERT_TMACRO(vec_vec3_sub, vert1, vert1, vert0),            // vert1 <- edge1
    INSERT_TMACRO(vec_vec3_sub, vert2, vert2, vert0),            // vert2 <- edge2
    INSERT_TMACRO(vec3_cross, vtmp, d, vert2, hit),              // vtmp <- pvec
    INSERT_TMACRO(vec3_dot, t, vert1, vtmp),                     // t <- det
    TABS_CS(tmp1, t),                                            // tmp1 <- abs(det)
    TGE_CS(hit, tmp1, EPSILON),                                  // hit <- abs(det)>=EPSILON
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

/// @struct loom::vm::isect_quad
/// @brief Ray-quadrilateral intersection @cite LD2004AERIT. No UV compute.
/// @param[out] hit intersection test value destination.
/// @param[out] t root (distance to intersection) destination.
/// @param[in] o vec[3] ray origin
/// @param[in] d vec[3] ray direction
/// @param v00-v11 float[3] quad vertices
/// @param vtmp1-3 array[3] storage for temporary vectors
/// @param D storage for temporary values.
/// @param alpha storage for temporary values
/// @param beta storage for temporary values

template <std::floating_point T>
TMACRO14(isect_quad, hit, t, o, d, v00, v01, v11, v10,
         vtmp1, vtmp2, vtmp3, D, alpha, beta)
    INSERT_TMACRO(vec_vec3_sub, vtmp1, v10, v00),           // vtmp1 <- e01
    INSERT_TMACRO(vec_vec3_sub, vtmp2, v01, v00),           // vtmp2 <- e03
    INSERT_TMACRO(vec3_cross, vtmp3, d, vtmp2, hit),        // vtmp3 <- p
    INSERT_TMACRO(vec3_dot, D, vtmp1, vtmp3),               // D <- D
    TABS_CS(alpha, D),                                      // alpha <- abs(D)
    TGE_CS(hit, alpha, EPSILON),                            // hit <- abs(D)>=EPSILON
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l1),          // if hit then l1 else end
    LABEL(isect_quad_l1),
    INSERT_TMACRO(vec_vec3_sub, vtmp1, o, v00),             // vtmp1 <- T
    INSERT_TMACRO(vec3_dot, alpha, vtmp1, vtmp3),           // alpha <- dot(T, p)
    TDIV_CS(alpha, alpha, D),                               // alpha <- alpha
    TGE_CS(hit, alpha, CS(0)),                              // hit <- alpha>=0
    TLE_CS(t, alpha, CS(1)),                                // t <- alpha<=1
    ILAND_CS(hit, hit, t),                                  // hit <- range01(alpha)
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l2),          // if hit then l2 else end
    LABEL(isect_quad_l2),
    INSERT_TMACRO(vec_vec3_sub, vtmp1, v10, v00),           // vtmp1 <- e01
    INSERT_TMACRO(vec_vec3_sub, vtmp2, o, v00),             // vtmp2 <- T
    INSERT_TMACRO(vec3_cross, vtmp3, vtmp2, vtmp1, hit),    // vtmp3 <- Q
    INSERT_TMACRO(vec3_dot, beta, d, vtmp3),                // beta <- dot(d, Q)
    TDIV_CS(beta, beta, D),                                 // beta <- beta
    TGE_CS(hit, beta, CS(0)),                               // hit <- beta>=0
    TLE_CS(t, beta, CS(1)),                                 // t <- beta<=1
    ILAND_CS(hit, hit, t),                                  // hit <- range01(beta)
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l3),          // if hit then l3 else end
    LABEL(isect_quad_l3),
    TADD_CS(t, alpha, beta),                                // t <- alpha + beta
    TGT_CS(hit, t, CS(1)),                                  // hit <- (alpha + beta) > 1
    IPJMP2_CS(hit, isect_quad_l7, isect_quad_l4),           // if (alpha + beta) > 1 then l7 else l3
    LABEL(isect_quad_l4),
    INSERT_TMACRO(vec_vec3_sub, vtmp1, v01, v11),           // vtmp1 <- e23
    INSERT_TMACRO(vec_vec3_sub, vtmp2, v10, v11),           // vtmp2 <- e21
    INSERT_TMACRO(vec3_cross, vtmp3, d, vtmp2, hit),        // vtmp3 <- p_
    INSERT_TMACRO(vec3_dot, t, vtmp1, vtmp3),               // t <- D_
    TABS_CS(hit, t),                                        // hit <- abs(D_)
    TGE_CS(hit, hit, EPSILON),                              // hit <- abs(D_)>=EPSILON
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l5),          // if hit then l5 else end
    LABEL(isect_quad_l5),
    INSERT_TMACRO(vec_vec3_sub, vtmp2, o, v11),             // vtmp2 <- T_
    INSERT_TMACRO(vec3_dot, hit, vtmp2, vtmp3),             // hit <- dot(T_, p_)
    TDIV_CS(hit, hit, t),                                   // hit <- alpha_
    TGE_CS(hit, hit, CS(0)),                                // hit <- alpha_>=0
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l6),          // if hit then l6 else end
    LABEL(isect_quad_l6),
    INSERT_TMACRO(vec3_cross, vtmp3, vtmp2, vtmp1, hit),    // vtmp3 <- Q_
    INSERT_TMACRO(vec3_dot, hit, d, vtmp3),                 // hit <- dot(d, Q_)
    TDIV_CS(hit, hit, t),                                   // hit <- beta_
    TGE_CS(hit, hit, CS(0)),                                // hit <- beta_>=0
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l7),          // if hit then l7 else end
    LABEL(isect_quad_l7),
    INSERT_TMACRO(vec_vec3_sub, v10, v10, v00),             // v10 <- e01
    INSERT_TMACRO(vec_vec3_sub, vtmp2, o, v00),             // vtmp2 <- T
    INSERT_TMACRO(vec3_cross, vtmp3, vtmp2, v10, hit),      // vtmp3 <- Q
    INSERT_TMACRO(vec_vec3_sub, v01, v01, v00),             // v01 <- e03
    INSERT_TMACRO(vec3_dot, t, v01, vtmp3),                 // t <- dot(e03, Q)
    TDIV_CS(t, t, D),                                       // t <- dot(e03, Q)/D
    TGE_CS(hit, t, CS(0)),                                  // hit <- t>=0
    LABEL(isect_quad_end)
MACRO_END;
} // namespace loom::vm
