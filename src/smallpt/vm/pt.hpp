// -*- C++ -*-
/// @file pt.hpp
/// @brief VM port of smallpt.

#pragma once

#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/modules/linal.hpp>
#include <vm/operators.hpp>

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

MACRO4(rotl32, x, n, dst, tmp)
    BAND_CS(std::uint32_t, dst, n, CS(31)),
    LSHFT_CS(std::uint32_t, dst, x, dst),
    SUB_CS(std::uint32_t, tmp, CS(32), n),
    BAND_CS(std::uint32_t, tmp, tmp, CS(31)),
    RSHFT_CS(std::uint32_t, tmp, x, tmp),
    BOR_CS(std::uint32_t, dst, dst, tmp)
MACRO_END;

MACRO5(threefry_round, c0, c1, r, tmp1, tmp2)
    ADD_CS(std::uint32_t, c0, c0, c1),
    INSERT_MACRO(rotl32, c1, r, tmp1, tmp2),
    XOR_CS(std::uint32_t, c1, tmp1, c0)
MACRO_END;

MACRO5(threefry_key_injection, kc, ks1, ks2, c0, c1)
    ADD_CS(std::uint32_t, c0, c0, ks1),
    ADD_CS(std::uint32_t, c1, c1, ks2),
    ADD_CS(std::uint32_t, c1, c1, kc)
MACRO_END;

MACRO10(threefry_8rounds, c0, c1, kc1, kc2, ks1, ks2, ks3, ks4, tmp1, tmp2)
    INSERT_MACRO(threefry_round, c0, c1, CS(13), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(15), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(26), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(6), tmp1, tmp2),
    INSERT_MACRO(threefry_key_injection, kc1, ks1, ks2, c0, c1),
    INSERT_MACRO(threefry_round, c0, c1, CS(17), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(29), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(16), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(24), tmp1, tmp2),
    INSERT_MACRO(threefry_key_injection, kc2, ks3, ks4, c0, c1)
MACRO_END;

MACRO7(threefry2x32r32, c0, c1, k0, k1, k2, tmp1, tmp2) // k2 should be constant too: k2 = 0x1bd11bda ^ k0 ^ k1
    ADD_CS(std::uint32_t, c0, c0, k0),
    ADD_CS(std::uint32_t, c1, c1, k1),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(1), CS(2), k1, k2, k2, k0, tmp1, tmp2),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(3), CS(4), k0, k1, k1, k2, tmp1, tmp2),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(5), CS(6), k2, k0, k0, k1, tmp1, tmp2),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(7), CS(8), k1, k2, k2, k0, tmp1, tmp2)
MACRO_END;

TMACRO1(gen_canonical, x)
    RSHFT_CS(std::uint32_t, x, x, CS(9)),
    BOR_CS(std::uint32_t, x, x, CS(0x3f800000)),
    TSUB_CS(x, x, CS(1))
MACRO_END;

MACRO6(bump_ctr, c0, c1, i, tmp1, tmp2, tmp3)
    ADD_CS(std::uint32_t, tmp1, c0, i),
    LT_CS(std::uint32_t, std::uint32_t, tmp2, tmp1, c0),
    ADD_CS(std::uint32_t, tmp3, c1, tmp1), // speculatively tmp3 <- c1 + tmp1
    CMOV_CS(std::uint32_t, std::uint32_t, c1, tmp2, tmp3, c1),
    CMOV_CS(std::uint32_t, std::uint32_t, c0, tmp2, CS(0), tmp1)
MACRO_END;

TMACRO2(sample_u, u0, u1)
    INSERT_TMACRO(gen_canonical, u0),
    INSERT_TMACRO(gen_canonical, u1)
MACRO_END;

/// @tparam width scalar
/// @tparam pos vec[2] saving to it
/// @tparam u vec[2]
/// @tparam tmp scalar
TMACRO4(jitter_sample_pos, width, pos, u, tmp)
    TMUL_CS(tmp, width, CS(0.5)),
    INSERT_TMACRO(vec2_scalar_sub, pos, pos, tmp),
    INSERT_TMACRO(vec2_scalar_mul, u, u, width),
    INSERT_TMACRO(vec_vec2_add, pos, pos, u)
MACRO_END;

using PI_CS = CS(3.141592653589793);
using PI2_CS = CS(6.283185307179586);

// these parameters must be indexed
/// @tparam dst vec[3]
/// @tparam n vec[3]
/// @tparam x vec[3]
/// all of them modified
TMACRO11(sample_hemisphere, dst0, dst1, dst2, n0, n1, n2, u0, u1, x, tmp0, tmp1)
    TSUB_CS(u0, u0, CS(1)),
    TMUL_CS(u0, u0, PI2_CS),
    TABS_CS(tmp1, n0),
    TGT_CS(tmp0, tmp1, CS(0.1)),
    TCMOV_CS(dst0, tmp0, CS(0), CS(1)),
    TCMOV_CS(dst1, tmp0, CS(1), CS(0)),
    TMOV_CS(dst2, CS(0)),
    INSERT_TMACRO(vec3_cross, x, CS_UNIDX(dst0), CS_UNIDX(n0), tmp0),
    INSERT_TMACRO(vec3_norm, x, tmp0),
    INSERT_TMACRO(vec3_cross, CS_UNIDX(dst0), CS_UNIDX(n0), x, tmp0),
    TCOS_CS(tmp0, u0),
    TSQRT_CS(tmp1, u1),
    TMUL_CS(tmp0, tmp0, tmp1),
    INSERT_TMACRO(vec3_scalar_mul, x, x, tmp0),
    TSIN_CS(tmp0, u0),
    TMUL_CS(tmp0, tmp0, tmp1),
    INSERT_TMACRO(vec3_scalar_mul, CS_UNIDX(dst0), CS_UNIDX(dst0), tmp0),
    INSERT_TMACRO(vec_vec3_add, CS_UNIDX(dst0), CS_UNIDX(dst0), x),
    TSUB_CS(tmp0, CS(1), u1),
    TSQRT_CS(tmp0, tmp0),
    INSERT_TMACRO(vec3_scalar_mul, CS_UNIDX(n0), CS_UNIDX(n0), tmp0),
    INSERT_TMACRO(vec_vec3_add, CS_UNIDX(dst0), CS_UNIDX(dst0), CS_UNIDX(n0))
MACRO_END;

/// @tparam sx dest pixel position x, float
/// @tparam sy dest pixel position y, float
/// @tparam w image width, float
/// @tparam h image height, float
/// @tparam r ratio float, gang invariant, we don't compute it in kernel
TMACRO5(to_sample_space, sx, sy, w, h, r)
    TDIV_CS(sx, sx, w),
    TSUB_CS(sx, sx, CS(0.5)),
    TMUL_CS(sx, sx, r),
    TDIV_CS(sy, sy, h),
    TSUB_CS(sy, sy, CS(0.5))
MACRO_END;

struct default_cam
{
    // fov = 60
    using TAN_FOV_HALF = CS(0.5773503780365);

    using POS0 = CS(0);
    using POS1 = CS(0);
    using POS2 = CS(4);

    using DIR0 = CS(0);
    using DIR1 = CS(0);
    using DIR2 = CS(-1);
};

// d is d[3], but we set only x and y, z = -1
template <std::floating_point T>
TMACRO2(gen_ray_dir, d0, d1)
    TMUL_CS(d0, d0, default_cam::TAN_FOV_HALF),
    TMUL_CS(d1, d1, default_cam::TAN_FOV_HALF),
    TNEG_CS(d0, d0),
    TNEG_CS(d1, d1)
    // TSUB_CS(d0, default_cam::DIR0, ndc0),
    // TSUB_CS(d1, default_cam::DIR1, ndc1),
MACRO_END;

// pid[out] primitive id
// o[in] v[3] ray origin
// d[in] v[3] ray direction
template <std::floating_point T>
TMACRO3(traverse, pid, o, d)
    TMOV(t, 1e6),               // t <- 1e6
    IMOV_CS(pid, CS(5)),        // pid <- miss
    // prim 0 - floor
    TMOV(vr00, -1),             // vr0 <- v00 = vertex 0 (-1, -1, 1)
    TMOV(vr01, -1),
    TMOV(vr02, 1),
    TMOV(vr10, 1),              // vr1 <- v01 = vertex 3 (1, -1, 1)
    TMOV(vr11, -1),
    TMOV(vr12, 1),
    TMOV(vr20, 1),              // vr2 <- v11 = vertex 2 (1, -1, -1)
    TMOV(vr21, -1),
    TMOV(vr22, -1),
    TMOV(vr30, -1),             // vr3 <- v10 = vertex 1 (-1, -1, -1)
    TMOV(vr31, -1),
    TMOV(vr32, -1),
    IMOV(pp, 0),                // next: set_vtxs_right_wall
    JMP(isect_quad),
    LABEL(set_vtxs_right_wall),
    // prim 1 - right wall
    TMOV(vr00, -1),             // vr0 <- v00 = vertex 0 (-1, -1, 1)
    TMOV(vr01, -1),
    TMOV(vr02, 1),
    TMOV(vr10, -1),             // vr1 <- v01 = vertex 1 (-1, -1, -1)
    TMOV(vr11, -1),
    TMOV(vr12, -1),
    TMOV(vr20, -1),             // vr2 <- v11 = vertex 5 (-1, 1, -1)
    TMOV(vr21, 1),
    TMOV(vr22, -1),
    TMOV(vr30, -1),             // vr3 <- v10 = vertex 4 (-1, 1, 1)
    TMOV(vr31, 1),
    TMOV(vr32, 1),
    IMOV(pp, 1),                // next: set_vtxs_left_wall
    JMP(isect_quad),
    LABEL(set_vtxs_left_wall),
    // prim 2 - left wall
    TMOV(vr00, 1),              // vr0 <- v00 = vertex 2 (1, -1, -1)
    TMOV(vr01, -1),
    TMOV(vr02, -1),
    TMOV(vr10, 1),              // vr1 <- v01 = vertex 3 (1, -1, 1)
    TMOV(vr11, -1),
    TMOV(vr12, 1),
    TMOV(vr20, 1),              // vr2 <- v11 = vertex 7 (1, 1, 1)
    TMOV(vr21, 1),
    TMOV(vr22, 1),
    TMOV(vr30, 1),              // vr3 <- v10 = vertex 6 (1, 1, -1)
    TMOV(vr31, 1),
    TMOV(vr32, -1),
    IMOV(pp, 2),                // next: set_vtxs_ceiling
    JMP(isect_quad),
    LABEL(set_vtxs_ceiling),
    // prim 3 - ceiling
    TMOV(vr00, -1),             // vr0 <- v00 = vertex 4 (-1, 1, 1)
    TMOV(vr01, 1),
    TMOV(vr02, 1),
    TMOV(vr10, -1),             // vr1 <- v01 = vertex 5 (-1, 1, -1)
    TMOV(vr11, 1),
    TMOV(vr12, -1),
    TMOV(vr20, 1),              // vr2 <- v11 = vertex 6 (1, 1, -1)
    TMOV(vr21, 1),
    TMOV(vr22, -1),
    TMOV(vr30, 1),              // vr3 <- v10 = vertex 7 (1, 1, 1)
    TMOV(vr31, 1),
    TMOV(vr32, 1),
    IMOV(pp, 3),                // next: set_vtxs_back_wall
    JMP(isect_quad),
    LABEL(set_vtxs_back_wall),
    // prim 4 - back wall
    TMOV(vr00, -1),             // vr0 <- v00 = vertex 1 (-1, -1, -1)
    TMOV(vr01, -1),
    TMOV(vr02, -1),
    TMOV(vr10, 1),              // vr1 <- v01 = vertex 2 (1, -1, -1)
    TMOV(vr11, -1),
    TMOV(vr12, -1),
    TMOV(vr20, 1),              // vr2 <- v11 = vertex 6 (1, 1, -1)
    TMOV(vr21, 1),
    TMOV(vr22, -1),
    TMOV(vr30, -1),             // vr3 <- v10 = vertex 5 (-1, 1, -1)
    TMOV(vr31, 1),
    TMOV(vr32, -1),
    IMOV(pp, 4),                // next: traverse_room_end
    JMP(isect_quad),
    LABEL(isect_quad),
    INSERT_TMACRO(isect_quad, CS(hit), CS(ct), o, d,
                  CS(vr0), CS(vr1), CS(vr2), CS(vr3),
                  CS(vr4), CS(vr5), CS(vr6),
                  CS(tmp0), CS(tmp1), CS(tmp2)),
    TLT(tmp0, ct, t),           // tmp0 <- ct < t
    ILAND(tmp0, tmp0, hit),
    TCMOV(t, tmp0, ct, t),
    ICMOV_CS(pid, CS(tmp0), CS(pp), pid),
    IPJMP5(pp, set_vtxs_right_wall, set_vtxs_left_wall, set_vtxs_ceiling, set_vtxs_back_wall, traverse_room_end),
    LABEL(traverse_room_end)
MACRO_END;

// set colors and normal
// clr[rgb][out] rgb[3] color
// pid[in] primitive id
template <std::floating_point T>
TMACRO4(set_materials, clrr, clrg, clrb, pid)
    IPJMP6(pid, set_clr_floor, set_clr_right_wall, set_clr_left_wall, set_clr_ceiling, set_clr_back_wall, set_clr_miss),
    LABEL(set_clr_floor),       // white floor
    TMOV_CS(clrr, CS(0.8)),
    TMOV_CS(clrg, CS(0.8)),
    TMOV_CS(clrb, CS(0.8)),
    JMP(set_materials_end),
    LABEL(set_clr_right_wall),   // red wall
    TMOV_CS(clrr, CS(1)),
    TMOV_CS(clrg, CS(0.2)),
    TMOV_CS(clrb, CS(0.2)),
    JMP(set_materials_end),
    LABEL(set_clr_left_wall),  // green wall
    TMOV_CS(clrr, CS(0.2)),
    TMOV_CS(clrg, CS(1)),
    TMOV_CS(clrb, CS(0.2)),
    JMP(set_materials_end),
    LABEL(set_clr_ceiling),     // blue ceiling for now
    TMOV_CS(clrr, CS(0.2)),
    TMOV_CS(clrg, CS(0.2)),
    TMOV_CS(clrb, CS(1)),
    JMP(set_materials_end),
    LABEL(set_clr_back_wall),   // yellow back wall for now
    TMOV_CS(clrr, CS(1)),
    TMOV_CS(clrg, CS(1)),
    TMOV_CS(clrb, CS(0.2)),
    JMP(set_materials_end),
    LABEL(set_clr_miss),
    TMOV_CS(clrr, CS(0)),
    TMOV_CS(clrg, CS(0)),
    TMOV_CS(clrb, CS(0)),
    LABEL(set_materials_end)
MACRO_END;
} // namespace loom::vm
