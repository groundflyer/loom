// -*- C++ -*-
/// @file pt.hpp
/// @brief VM port of smallpt.

#pragma once

#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

#include <vm/lib/linal.hpp>
#include <vm/lib/random.hpp>

namespace loom::vm
{
// clang-format off

using EPSILON = CS(0.0001);

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
/// @param tmp1 storage for temporal values
/// @param tmp2 storage for temporal values
///
/// Values bound to @p a, @p b, @p c are invalidated.

template <std::floating_point T>
TMACRO7(solve_quadratic, s, x, a, b, c, tmp1, tmp2)
    TABS_CS(s, a),              // s <- abs(a)
    TGT_CS(s, s, EPSILON),      // s <- a !~ 0
    IPJMP2_CS(s, sq_head1, sq_mid1),
    LABEL(sq_head1),
    TABS_CS(s, b),              // s <- abs(b)
    TGT_CS(s, s, EPSILON),      // s <- b !~ 0
    IPJMP2_CS(s, sq_end, sq_head2),
    LABEL(sq_head2),
    TNEG_CS(x, c),              // x <- -c
    TDIV_CS(x, x, b),           // x <- -c/b
    JMP(sq_end),
    LABEL(sq_mid1),
    TMUL_CS(tmp1, c, a),
    TMUL_CS(tmp1, tmp1, CS(4)),
    TFMS_CS(tmp1, b, b, tmp1),   // tmp1 <- D
    TGE_CS(s, tmp1, CS(0)),      // s <- D>=0
    IPJMP2_CS(s, sq_end, sq_mid2),
    LABEL(sq_mid2),
    TSQRT_CS(s, tmp1),           // s <- sqrt(D)
    TLT_CS(x, b, CS(0)),         // x <- b<0
    TSUB_CS(tmp2, b, s),         // tmp2 <- b-sqrt(D)
    TADD_CS(b, b, s),            // b <- b+sqrt(D)
    TCMOV_CS(b, x, tmp2, b),     // b <- b<0 ? b-sqrt(D) : b+sqrt(D)
    TMUL_CS(b, b, CS(-0.5)),     // b <- temp
    TDIV_CS(x, b, a),            // x <- x1
    TDIV_CS(c, c, b),            // c <- x2
    TLT_CS(b, x, CS(0)),         // b <- x1<0
    TCMOV_CS(x, b, c, x),        // x <- x1<0 ? x2 : x1
    IPJMP2_CS(b, sq_mid, sq_tail),
    LABEL(sq_mid),
    TLT_CS(b, c, CS(0)),         // b <- x2<0
    TMIN_CS(s, x, c),            // s <- min(x1, x2)
    TCMOV_CS(x, b, x, s),        // x <- (x2<0) ? x1 : min(x1, x2)
    LABEL(sq_tail),
    TMUL_CS(s, tmp1, x),         // s <- D*x
    TGE_CS(s, s, CS(0)),         // s <- (D*x) >= 0
    LABEL(sq_end)
MACRO_END;

/// @struct loom::vm::prep_isect_sphere
/// @brief Ray-sphere intersection port. Prepares coeffs for solve_quadratic.
/// @param[out] a
/// @param[out] b
/// @param[out] c
/// @param[in] o vec[3] ray origin
/// @param[in] d vec[3] ray direction
/// @param[in] p vec[3] sphere position
/// @param[in] r sphere radius
/// @param (v)tmp1-2 storage for temporary values

template <std::floating_point T>
TMACRO10(prep_isect_sphere, a, b, c, o, d, p, r, tmp1, tmp2, vtmp1)
    INSERT_TMACRO(vec_vec3_sub, vtmp1, o, p),               // vtmp1 <- pc
    TMUL_CS(tmp1, r, r),                                    // tmp1 <- r*r
    INSERT_TMACRO(vec3_length2, tmp2, vtmp1),               // tmp2 <- length2(pc)
    TSUB_CS(c, tmp2, tmp1),                                 // c <- c
    INSERT_TMACRO(vec3_scalar_mul, vtmp1, vtmp1, CS(2)),    // vtmp1 <- pc*2
    INSERT_TMACRO(vec3_dot, b, d, vtmp1),                   // b <- b
    INSERT_TMACRO(vec3_length2, a, d)                       // a <- a
MACRO_END;

// cylinder intersection algorithm:
// o = o - p
// cyl_prep_abc
// solve_quadratic
// cyl_fin

// r is constant
template <std::floating_point T>
TMACRO10(cyl_prep_abc, a, b, c, ox, oy, oz, dx, dy, dz, r)
    TMUL_CS(a, dz, dz),
    TFMA_CS(a, dx, dx, a),
    TMUL_CS(b, oz, dz),
    TFMA_CS(b, ox, dx, b),
    TMUL_CS(b, b, CS(2)),
    TMUL_CS(c, r, r),
    TFMS_CS(c, oz, oz, c),
    TFMA_CS(c, ox, ox, c)
MACRO_END;

// h is constant
template <std::floating_point T>
TMACRO6(cyl_fin, hit, t, oy, dy, y, h)
    TFMA_CS(y, dy, t, oy),
    TANDGE_CS(hit, hit, y, CS(0)),
    TANDLE_CS(hit, hit, y, h)
MACRO_END;

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
    TANDLE_CS(hit, hit, tmp1, CS(1)),                            // hit <- (u>=0)&&(u<=1)
    IPJMP2_CS(hit, isect_triangle_end, isect_triangle_mid2),     // if hit then mid2 else end
    LABEL(isect_triangle_mid2),
    INSERT_TMACRO(vec3_cross, vtmp, vert0, vert1, tmp2),         // vtmp <- qvec
    INSERT_TMACRO(vec3_dot, tmp2, d, vtmp),                      // tmp2 <- dot(d, qvec)
    TMUL_CS(tmp2, tmp2, t),                                      // tmp2 <- v
    TGE_CS(hit, tmp2, CS(0)),                                    // hit <- v>=0
    TADD_CS(tmp1, tmp1, tmp2),                                   // tmp1 <- u+v
    TANDLE_CS(hit, hit, tmp1, CS(1)),                            // hit <- (v>=0)&&(v+u<=1)
    IPJMP2_CS(hit, isect_triangle_end, isect_triangle_mid3),     // if hit then mid3 else end
    LABEL(isect_triangle_mid3),
    INSERT_TMACRO(vec3_dot, tmp1, vert2, vtmp),                  // tmp1 <- dot(edge2, qvec)
    TMUL_CS(t, tmp1, t),                                         // t <- dot(edge2, qvec)*inv_det
    TGT_CS(hit, t, CS(0)),                                       // hit <- t>0
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
    TANDLE_CS(hit, hit, alpha, CS(1)),                      // hit <- range01(alpha)
    IPJMP2_CS(hit, isect_quad_end, isect_quad_l2),          // if hit then l2 else end
    LABEL(isect_quad_l2),
    INSERT_TMACRO(vec_vec3_sub, vtmp1, v10, v00),           // vtmp1 <- e01
    INSERT_TMACRO(vec_vec3_sub, vtmp2, o, v00),             // vtmp2 <- T
    INSERT_TMACRO(vec3_cross, vtmp3, vtmp2, vtmp1, hit),    // vtmp3 <- Q
    INSERT_TMACRO(vec3_dot, beta, d, vtmp3),                // beta <- dot(d, Q)
    TDIV_CS(beta, beta, D),                                 // beta <- beta
    TGE_CS(hit, beta, CS(0)),                               // hit <- beta>=0
    TANDLE_CS(hit, hit, beta, CS(1)),                       // hit <- range01(beta)
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

TMACRO2(sample_u, u0, u1)
    INSERT_TMACRO(gen_canonical, u0),
    INSERT_TMACRO(gen_canonical, u1)
MACRO_END;

/// @tparam pw scalar
/// @tparam pos vec[2] saving to it
/// @tparam u vec[2]
/// @tparam tmp scalar
TMACRO4(jitter_sample_pos, pw, pos, u, tmp)
    TMUL_CS(tmp, pw, CS(0.5)),
    INSERT_TMACRO(vec2_scalar_sub, pos, pos, tmp),
    INSERT_TMACRO(vec2_scalar_mul, u, u, pw),
    INSERT_TMACRO(vec_vec2_add, pos, pos, u)
MACRO_END;

using PI_CS = CS(3.141592653589793);
using PI2_CS = CS(6.283185307179586);

TMACRO9(onb, b0, b1, b2, t0, t1, t2, n0, n1, n2)
    TCOPYSIGN_CS(t2, CS(1), n2), // t2 <- sign
    TADD_CS(b0, t2, n2),
    TDIV_CS(b0, CS(-1), b0),    // b0 <- a
    TMUL_CS(t0, n0, n1),
    TMUL_CS(t0, t0, b0),        // t0 <- b
    TMUL_CS(t1, b0, n1),
    TMUL_CS(t1, t1, n1),
    TADD_CS(t1, t1, t2),
    TMUL_CS(b0, b0, n0),
    TMUL_CS(b0, b0, n0),
    TMUL_CS(b0, b0, t2),
    TADD_CS(b0, b0, CS(1)),
    TMUL_CS(b1, t2, t0),
    TNEG_CS(b2, t2),
    TMUL_CS(b2, b2, n0),
    TNEG_CS(t2, n1)
MACRO_END;

// these parameters must be indexed
/// @tparam dst vec[3]
/// @tparam n vec[3]
/// @tparam b vec[3]
/// @tparam t vec[3]
/// all of them modified
TMACRO8(sample_hemisphere, dst, n, b, t, u0, u1, tmp0, tmp1)
    TSUB_CS(u0, u0, CS(0.5)),
    TMUL_CS(u0, u0, PI2_CS),    // u0 <- r1
    TSQRT_CS(tmp0, u1),         // tmp0 <- r2s
    TCOS_CS(tmp1, u0),
    TMUL_CS(tmp1, tmp1, tmp0),
    INSERT_TMACRO(vec3_scalar_mul, b, b, tmp1),
    TSIN_CS(tmp1, u0),
    TMUL_CS(tmp1, tmp1, tmp0),
    INSERT_TMACRO(vec3_scalar_mul, t, t, tmp1),
    TSUB_CS(tmp0, CS(1), u1),
    TSQRT_CS(tmp0, tmp0),
    INSERT_TMACRO(vec3_scalar_mul, n, n, tmp0),
    INSERT_TMACRO(vec_vec3_add, dst, b, t),
    INSERT_TMACRO(vec_vec3_add, dst, dst, n)
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

struct scene
{
    // fov = 110
    using CAM_TAN_FOV_HALF = CS(1.4281482696533203);

    using CAM_Px = CS(0);
    using CAM_Py = CS(0);
    using CAM_Pz = CS(2);

    using CAM_Dx = CS(0);
    using CAM_Dy = CS(0);
    using CAM_Dz = CS(-1);

    // tetrahedron points
    using TH_V0x = CS(-0.7878358364105225);
    using TH_V0y = CS(-1);
    using TH_V0z = CS(-0.5878359079360962);

    using TH_V1x = CS(-0.5419577956199646);
    using TH_V1y = CS(-1);
    using TH_V1z = CS(0.3297936916351318);

    using TH_V2x = CS(0.1297936737537384);
    using TH_V2y = CS(-1);
    using TH_V2z = CS(-0.3419578075408936);

    using TH_V3x = CS(-0.4000000059604645);
    using TH_V3y = CS(-0.1772758364677429);
    using TH_V3z = CS(-0.2000000029802322);

    // tetrahedron normals
    using TH0_Nx = CS(-0.9163577556610107);
    using TH0_Ny = CS(0.3162277340888977);
    using TH0_Nz = CS(0.2455372512340546);

    using TH1_Nx = CS(0.6708204150199890);
    using TH1_Ny = CS(0.3162277936935425);
    using TH1_Nz = TH1_Nx;

    using TH2_Nx = CS(0.2455373108386993);
    using TH2_Ny = TH1_Ny;
    using TH2_Nz = CS(-0.9163576960563660);

    // sphere position
    using SP_Px = CS(0.1);
    using SP_Py = CS(-0.7);
    using SP_Pz = CS(0.3);
    using SP_P = type_sequence<SP_Px, SP_Py, SP_Pz>;

    // cylinder
    using CL_Px = CS(0.5);
    using CL_Py = CS(-1);
    using CL_Pz = CS(-0.35);

    using CL_P = type_sequence<CL_Px, CL_Py, CL_Pz>;

    using CL_R = CS(0.25);
    using CL_H = CS(0.8);
    using CL_C = CS(-0.2);      // cap y

    using CL_CP = type_sequence<CL_Px, CL_C, CL_Pz>; // cap position

    // light
    using L_Px = CS(0);
    using L_Py = CS(0.9999);
    using L_Pz = CS(0);
    using L_P = type_sequence<L_Px, L_Py, L_Pz>;

    using L_Nx = CS(0);
    using L_Ny = CS(-1);
    using L_Nz = CS(0);
    using L_N = type_sequence<L_Nx, L_Ny, L_Nz>;

    using L_B = type_sequence<CS(1), CS(0), CS(0)>;
    using L_T = type_sequence<CS(0), CS(0), CS(1)>;
    using L_E = CS(5);          // emission
    using L_R = CS(0.35);       // radius
    using L_A = CS(0.3848451000647496); // light area

    // threefry keys
    using K0 = CS(0xf00dead);
    using K1 = CS(0xba0beef);
    using K2 = CS(0x4a06042);   // C0 ^ C1

    // minimum pixel samples
    using MINSAMPLES = CS(64);

    // luminance
    using LUM = type_sequence<CS(0.2126), CS(0.7152), CS(0.0722)>;
};

TMACRO5(sample_light_disk, dst, u0, u1, tmp, vtmp)
    TSQRT_CS(u0, u0),
    TMUL_CS(u0, u0, scene::L_R),         // u0 <- sr
    TSUB_CS(u1, u1, CS(0.5)),
    TMUL_CS(u1, u1, PI2_CS),    // u1 <- theta
    TSIN_CS(tmp, u1),
    TMUL_CS(tmp, tmp, u0),
    INSERT_TMACRO(vec3_scalar_mul, dst, scene::L_B, tmp),
    TCOS_CS(tmp, u1),
    TMUL_CS(tmp, tmp, u0),
    INSERT_TMACRO(vec3_scalar_mul, vtmp, scene::L_T, tmp),
    INSERT_TMACRO(vec_vec3_add, dst, dst, vtmp),
    INSERT_TMACRO(vec_vec3_add, dst, dst, scene::L_P)
MACRO_END;

// d is d[3], but we set only x and y, z = -1
template <std::floating_point T>
TMACRO2(gen_ray_dir, d0, d1)
    TMUL_CS(d0, d0, scene::CAM_TAN_FOV_HALF),
    TMUL_CS(d1, d1, scene::CAM_TAN_FOV_HALF),
    TNEG_CS(d0, d0),
    TNEG_CS(d1, d1)
    // TSUB_CS(d0, default_cam::DIR0, ndc0),
    // TSUB_CS(d1, default_cam::DIR1, ndc1),
MACRO_END;

// rp - return position

template <std::floating_point T>
TMACRO(traverse_scene)
    TMOV(t, 1e6),               // t <- 1e6
    IMOV(pid, 12),        // pid <- miss
    IMOV(rp, 0),
    LABEL(traverse_box),        // ray-trace walls loop
// set vertices pos <vr> using wall index <rp>
// 0 - floor, 1 - left wall, 2 - right wall, 3 - ceiling, 4 - back wall
//                   0   1   2   3   4
    TSCT5(vr00, rp, -1, -1,  1, -1, -1),
    TSCT5(vr01, rp, -1, -1, -1,  1, -1),
    TSCT5(vr02, rp,  1,  1, -1,  1, -1),
    TSCT5(vr10, rp,  1, -1,  1, -1,  1),
    TSCT5(vr11, rp, -1, -1, -1,  1, -1),
    TSCT5(vr12, rp,  1, -1,  1, -1, -1),
    TSCT5(vr20, rp,  1, -1,  1,  1,  1),
    TSCT5(vr21, rp, -1,  1,  1,  1,  1),
    TSCT5(vr22, rp, -1, -1,  1, -1, -1),
    TSCT5(vr30, rp, -1, -1,  1,  1, -1),
    TSCT5(vr31, rp, -1,  1,  1,  1,  1),
    TSCT5(vr32, rp, -1,  1, -1,  1, -1),
    INSERT_TMACRO(isect_quad, CS(hit), CS(ct), CS(o), CS(d),
                  CS(vr0), CS(vr1), CS(vr2), CS(vr3),
                  CS(vr4), CS(vr5), CS(vr6),
                  CS(tmp0), CS(tmp1), CS(tmp2)),
    TANDLT(hit, hit, ct, t),    // hit <- hit && ct < t
    TCMOV(t, hit, ct, t),
    ICMOV(pid, hit, rp, pid),
    IEQ(tmp0, rp, 4),
    IADD(rp, rp, 1),
    ICMOV(rp, tmp0, 0, rp),
    IPJMP2(tmp0, traverse_box, traverse_tetrahedron),
    LABEL(traverse_tetrahedron), // ray-trace tetrahedron sides loop
// set vertices pos <vr> using polygon index <rp>
    TSCTN_CS(CS(vr00), CS(rp), scene::TH_V0x, scene::TH_V1x, scene::TH_V2x),
    TSCTN_CS(CS(vr01), CS(rp), scene::TH_V0y, scene::TH_V1y, scene::TH_V2y),
    TSCTN_CS(CS(vr02), CS(rp), scene::TH_V0z, scene::TH_V1z, scene::TH_V2z),
    TSCTN_CS(CS(vr10), CS(rp), scene::TH_V1x, scene::TH_V2x, scene::TH_V0x),
    TSCTN_CS(CS(vr11), CS(rp), scene::TH_V1y, scene::TH_V2y, scene::TH_V0y),
    TSCTN_CS(CS(vr12), CS(rp), scene::TH_V1z, scene::TH_V2z, scene::TH_V0z),
    TMOV_CS(CS(vr20), scene::TH_V3x),
    TMOV_CS(CS(vr21), scene::TH_V3y),
    TMOV_CS(CS(vr22), scene::TH_V3z),
    INSERT_TMACRO(isect_triangle, CS(hit), CS(ct), CS(o), CS(d), CS(vr0), CS(vr1), CS(vr2),
                  CS(tmp0), CS(tmp1), CS(vr3)),
    TANDLT(hit, hit, ct, t),    // hit <- hit && ct < t
    TCMOV(t, hit, ct, t),
    IADD(tmp1, rp, 5),          // real pid
    ICMOV(pid, hit, tmp1, pid),
    IEQ(tmp0, rp, 2),
    IADD(rp, rp, 1),
    IPJMP2(tmp0, traverse_tetrahedron, traverse_sphere),
    LABEL(traverse_sphere),
    TMOV_CS(CS(vr00), scene::SP_Px),
    TMOV_CS(CS(vr01), scene::SP_Py),
    TMOV_CS(CS(vr02), scene::SP_Pz),
    INSERT_TMACRO(prep_isect_sphere, CS(vr10), CS(vr11), CS(vr12),
                  CS(o), CS(d), CS(vr0), CS(0.3), CS(tmp0), CS(tmp1), CS(vr2)),
    IMOV(rp, 8),                // sphere pid
    LABEL(traverse_solve_quadratic),
    INSERT_TMACRO(solve_quadratic, CS(hit), CS(ct), CS(vr10), CS(vr11), CS(vr12), CS(tmp0), CS(tmp1)),
    IEQ(tmp0, rp, 8),           // tracing sphere?
    IPJMP2(tmp0, traverse_cylinder_fin, traverse_sphere_fin),
    LABEL(traverse_sphere_fin),
    TANDLT(hit, hit, ct, t),    // hit <- hit && ct < t
    TCMOV(t, hit, ct, t),
    ICMOV(pid, hit, rp, pid),
    LABEL(traverse_cylinder),
    INSERT_TMACRO(vec_vec3_sub, CS(vr0), CS(o), scene::CL_P),
    INSERT_TMACRO(cyl_prep_abc, CS(vr10), CS(vr11), CS(vr12), CS(vr00), CS(vr01), CS(vr02), CS(d0), CS(d1), CS(d2), scene::CL_R),
    IMOV(rp, 9),
    JMP(traverse_solve_quadratic),
    LABEL(traverse_cylinder_fin),
    INSERT_TMACRO(cyl_fin, CS(hit), CS(ct), CS(vr01), CS(d1), CS(tmp0), scene::CL_H),
    TANDLT(hit, hit, ct, t),    // hit <- hit && ct < t
    TCMOV(t, hit, ct, t),
    ICMOV(pid, hit, rp, pid),
    IMOV(rp, 10),
    TMOV_CS(CS(vr00), scene::CL_Px),
    TMOV_CS(CS(vr01), scene::CL_C),
    TMOV_CS(CS(vr02), scene::CL_Pz),
    TMOV(vr10, 0),
    TMOV(vr11, 1),
    TMOV(vr12, 0),
    TMOV_CS(CS(tmp0), scene::CL_R),
    LABEL(traverse_disk),
    INSERT_TMACRO(isect_disk, CS(hit), CS(ct), CS(o), CS(d), CS(vr0), CS(vr1), CS(tmp0), CS(vr2), CS(tmp1)),
    TANDLT(hit, hit, ct, t),    // hit <- hit && ct < t
    TCMOV(t, hit, ct, t),
    ICMOV(pid, hit, rp, pid),
    IEQ(tmp1, rp, 11),          // tracing light?
    IMOV(rp, 11),               // set light parameters
    TMOV_CS(CS(vr00), scene::L_Px),
    TMOV_CS(CS(vr01), scene::L_Py),
    TMOV_CS(CS(vr02), scene::L_Pz),
    TMOV_CS(CS(vr10), scene::L_Nx),
    TMOV_CS(CS(vr11), scene::L_Ny),
    TMOV_CS(CS(vr12), scene::L_Nz),
    TMOV_CS(CS(tmp0), scene::L_R),
    IPJMP2(tmp1, traverse_disk, traverse_room_end),
    LABEL(traverse_room_end)
MACRO_END;

// n[3] <- normal
// t[3] <- tangent

// set colors and normal
template <std::floating_point T>
TMACRO(set_materials)
// 0 - white floor
// 1 - red wall
// 2 - green wall
// 3 - white ceiling
// 4 - window back wall
// 5 - blue tetrahedron
// 6
// 7
// 8 - glass sphere
// 9 - metall cylinder
// 10 - metall cylinder cap
// 11 - light
// set colors
    IMIN(tmp0, pid, 5),
//                      0    1    2    3    4      5
    TSCT6(clr0, tmp0, 1.0,   1, 0.2, 1.0, 1.0,   0.0),
    TSCT6(clr1, tmp0, 1.0, 0.4,   1, 1.0, 1.0, 0.212),
    TSCT6(clr2, tmp0, 1.0, 0.2, 0.4, 1.0, 1.0, 0.553),
    IEQ(tmp0, pid, 8),          // glass?
    TCMOV(clr0, tmp0, 1, clr0),
    TCMOV(clr1, tmp0, 1, clr1),
    TCMOV(clr2, tmp0, 1, clr2),
    IEQ(tmp0, pid, 9),          // cylinder?
    IOREQ(tmp0, tmp0, pid, 10), // or cap?
    TCMOV(clr0, tmp0, 1, clr0),
    TCMOV(clr1, tmp0, 0.786, clr1),
    TCMOV(clr2, tmp0, 0.427, clr2),
// set normals and tangents
    TMOV(t0, 0),              // default tangent (0, 1, 0)
    TMOV(t1, 1),
    TMOV(t2, 0),
    IEQ(tmp0, pid, 10),
    IOREQ(tmp0, tmp0, pid, 11), // tmp0 <- cap or light?
    IEQ(tmp1, pid, 8),
    IOREQ(tmp1, tmp1, pid, 9),  // tmp1 <- sphere or cylinder
    IMOV(rp, 0),                // assume polygons
    ICMOV(rp, tmp0, 1, rp),
    ISUB(tmp2, pid, 6),         // speculatively
    ICMOV(rp, tmp1, tmp2, rp),
    IPJMP4(rp, set_polygon_normal, set_disk_normal, compute_sphere_normal, compute_cylinder_normal),
    LABEL(set_polygon_normal),  // just set precomputed normals. not fair, but who cares
    TSCTN_CS(CS(n0), CS(pid), CS(0), CS(1), CS(-1), CS(0), CS(0), scene::TH0_Nx, scene::TH1_Nx, scene::TH2_Nx),
    TSCTN_CS(CS(n1), CS(pid), CS(1), CS(0), CS(0), CS(-1), CS(0), scene::TH0_Ny, scene::TH1_Ny, scene::TH2_Ny),
    TSCTN_CS(CS(n2), CS(pid), CS(0), CS(0), CS(0), CS(0), CS(1), scene::TH0_Nz, scene::TH1_Nz, scene::TH2_Nz),
    JMP(set_materials_end),
    LABEL(set_disk_normal),
    TMOV(n0, 0),
    TMOV(n2, 0),
    IEQ(tmp0, pid, 10),         // tmp0 <- cap?
    INSERT_TMACRO(vec_vec3_sub, CS(vr2), CS(o), scene::CL_CP), // compute tangent speculatively
    INSERT_TMACRO(vec3_norm, CS(vr2), CS(tmp1)),               // normalize tangent
    TCMOV(n1, tmp0, 1, -1),
    TCMOV(t0, tmp0, vr20, t0),
    TCMOV(t1, tmp0, vr21, t1),
    TCMOV(t2, tmp0, vr22, t2),
    JMP(set_materials_end),
    LABEL(compute_sphere_normal),
    INSERT_TMACRO(vec_vec3_sub, CS(n), CS(o), scene::SP_P),
    INSERT_TMACRO(vec3_norm, CS(n), CS(tmp0)),
    JMP(set_materials_end),
    LABEL(compute_cylinder_normal),
    TMOV(n1, 0),
    TSUB_CS(CS(n0), CS(o0), scene::CL_Px),
    TSUB_CS(CS(n2), CS(o2), scene::CL_Pz),
    INSERT_TMACRO(vec3_norm, CS(n), CS(tmp0)),
    LABEL(set_materials_end)
MACRO_END;

template <std::floating_point T>
TMACRO1(bias_ray, vtmp0)
    INSERT_TMACRO(vec3_scalar_mul, vtmp0, CS(d), CS(0.001)),
    INSERT_TMACRO(vec_vec3_add, CS(o), CS(o), vtmp0)
MACRO_END;

template <std::floating_point T>
TMACRO2(luminance, dst, rgb)
    INSERT_TMACRO(vec3_dot, dst, rgb, scene::LUM)
MACRO_END;

// rpt - return pointer
//
// roll:
// 0: sample_pixel
// 1: traverse_scene
// 2: direct_lighting
// 3: lambert_indirect
//
// traverse_scene:
// 0: post_traversal
// 1: direct_lighting_pdf
// 2: lambert_indirect

// save up on variables by encoding pixel position and resolution into single uint32 value:
// d2 <- pixel position 16/16 x/y
// tmp0 <- resolution 16/16 w/h
#if 0

template <std::same_as<float> T>
KERNEL(pt, IVAR(d2), IVAR(tmp0), IVAR(ns))
    IRSHFT(d0, d2, 16),
    IBAND(d1, d2, 0xffff),
    IRSHFT(tmp1, tmp0, 16),     // tmp1 <- w
    IBAND(tmp2, tmp0, 0xffff),  // tmp2 <- h
    IMOV(c0, d0),
    IMUL(c0, c0, tmp1),
    IADD(c0, c0, d1),
    IMOV(c1, 0),
    MOV(TVAR(tmp1), IVAR(tmp1)),
    MOV(TVAR(tmp2), IVAR(tmp2)),
    MOV(TVAR(pp0), IVAR(d0)),
    MOV(TVAR(pp1), IVAR(d1)),
    TDIV(tmp0, tmp1, tmp2),     // tmp0 <- ratio
    TDIV(pw, tmp0, tmp1),       // pw <- pixel width
    INSERT_TMACRO(to_sample_space, CS(pp0), CS(pp1), CS(tmp1), CS(tmp2), CS(tmp0)),
    TMOV(L0, 0),                // pixel contribution
    TMOV(L1, 0),
    TMOV(L2, 0),
    IMOV(rc, 0),                // ray count
    IMOV(rpt, 0),
    IMOV(sc, 0),                // sample count
// roll
    LABEL(roll),
    IMOV(u0, c0),
    IMOV(u1, c1),
    INSERT_MACRO(threefry2x32r32, CS(u0), CS(u1), scene::K0, scene::K1, scene::K2, CS(tmp0), CS(tmp1)),
    INSERT_MACRO(bump_ctr, CS(c0), CS(c1), CS(1), CS(tmp0), CS(tmp1), CS(tmp2)),
    INSERT_TMACRO(sample_u, CS(u0), CS(u1)),
    IPJMP4(rpt, sample_pixel, traverse_scene, direct_lighting, lambert_indirect),
// sample pixel
    LABEL(sample_pixel),
    TMOV_CS(CS(o0), scene::CAM_Px), // reset ray origin
    TMOV_CS(CS(o1), scene::CAM_Py),
    TMOV_CS(CS(o2), scene::CAM_Pz),
    TMOV(PL0, 1),               // path product
    TMOV(PL1, 1),
    TMOV(PL2, 1),
    TMOV(d0, pp0),
    TMOV(d1, pp1),
    TMOV(clr0, 0),
    TMOV(clr1, 0),
    TMOV(clr2, 0),
    IMOV(depth, 0),
    IADD(sc, sc, 1),
    INSERT_TMACRO(jitter_sample_pos, CS(pw), CS(d), CS(u), CS(tmp0)),
    INSERT_TMACRO(gen_ray_dir, CS(d0), CS(d1)),
    TMOV_CS(CS(d2), scene::CAM_Dz),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp0)),
    IGE(tmp0, sc, ns),
    IPJMP2(tmp0, traverse_scene, fin_pixel),
// traverse
    LABEL(traverse_scene),
    INSERT_TMACRO(bias_ray, CS(vr0)),
    INSERT_TMACRO(traverse_scene),
    IADD(rc, rc, 1),
    IPJMP3(rpt, post_traversal, direct_lighting_pdf, lambert_indirect),
// post traverse
    LABEL(post_traversal),
    IEQ(tmp0, pid, 11),                // hit light?
    TCMOV_CS(CS(tmp1), CS(tmp0), scene::L_E, CS(0)),
    INSERT_TMACRO(vec3_scalar_mul, CS(PL), CS(PL), CS(tmp1)), // final path factor
    IOREQ(tmp0, tmp0, pid, 12),                // tmp0 <- miss?
    IORGE_CS(CS(tmp0), CS(tmp0), CS(depth), scene::MAXDEPTH),
    IMOV(rpt, 0),
    IPJMP2(tmp0, set_materials, roll), // miss ? new sample
// set materials
    LABEL(set_materials),
    INSERT_TMACRO(vec3_scalar_mul, CS(vr0), CS(d), CS(t)),
    INSERT_TMACRO(vec_vec3_add, CS(o), CS(o), CS(vr0)), // compute new position
    INSERT_TMACRO(set_materials),
    IMOV(rpt, 2),
    JMP(roll),
// direct lighting
    LABEL(direct_lighting),
    TMOV(pdf_l, 0),
    INSERT_TMACRO(sample_light_disk, CS(d), CS(u0), CS(u1), CS(tmp0), CS(vr2)),
    INSERT_TMACRO(vec_vec3_sub, CS(d), CS(d), CS(o)),
    INSERT_TMACRO(vec3_norm, CS(d), CS(tmp0)),
    INSERT_TMACRO(vec3_dot, CS(tmp0), CS(d), CS(n)),
    TGT(tmp0, tmp0, 0),  // tmp0 <- cos(theta)>0
    ICMOV(rpt, tmp0, 1, 3),               // return to compute pdf after traverse, or skip to lambert indirect after roll
    IPJMP2(tmp0, roll, traverse_scene),
// direct lighting pdf
    LABEL(direct_lighting_pdf),
    INSERT_TMACRO(vec_vec3_sub, CS(vr2), scene::L_P, CS(o)),
    INSERT_TMACRO(vec3_length2, CS(pdf_l), CS(vr2)), // pdf_l <- l
    INSERT_TMACRO(vec3_dot, CS(tmp0), scene::L_N, CS(d)),
    TABS(tmp0, tmp0),
    TMUL_CS(CS(tmp0), CS(tmp0), scene::L_A),
    TDIV(pdf_l, pdf_l, tmp0),
    IEQ(tmp2, pid, 11),
// finish computing pdf
    INSERT_TMACRO(vec3_dot, CS(tmp0), CS(d), CS(n)),
    TMAX(tmp0, tmp0, 0),
    TMUL_CS(CS(tmp0), CS(tmp0), scene::L_E),
    TDIV(tmp0, tmp0, pdf_l),
    TCMOV(tmp0, tmp2, tmp0, 0),
    // INSERT_TMACRO(vec3_scalar_mul, CS(vr0), CS(clr), CS(tmp0)),
    INSERT_TMACRO(vec3_scalar_add, CS(PL), CS(PL), CS(tmp0)), // direct light contribution color
    IMOV(rpt, 3),                                       // return to lambert_indirect
    JMP(roll),
// indirect diffuse
    LABEL(lambert_indirect),
    INSERT_TMACRO(onb, CS(vr00), CS(vr01), CS(vr02), CS(t0), CS(t1), CS(t2), CS(n0), CS(n1), CS(n2)),
    INSERT_TMACRO(vec3_norm, CS(vr0), CS(tmp0)),
    INSERT_TMACRO(vec3_norm, CS(t), CS(tmp0)),
    INSERT_TMACRO(sample_hemisphere, CS(d), CS(n), CS(vr0), CS(t), CS(u0), CS(u1), CS(tmp0), CS(tmp1)),
    INSERT_TMACRO(vec3_norm, CS(d), CS(tmp0)),
    INSERT_TMACRO(vec_vec3_mul, CS(PL), CS(PL), CS(clr)),
    IMOV(rpt, 0),
    IADD(depth, depth, 1),
    JMP(traverse_scene),
    LABEL(fin_pixel),
    MOV(TVAR(sc), IVAR(sc)),
    INSERT_TMACRO(vec_vec3_add, CS(L), CS(L), CS(PL)),
    INSERT_TMACRO(vec3_scalar_div, CS(L), CS(L), CS(sc))
KERNEL_END(TVAR(L0), TVAR(L1), TVAR(L2), IVAR(rc));

#else

template <std::same_as<float> T>
KERNEL(pt, IVAR(ppp), IVAR(tmp0), TVAR(threshold), IVAR(maxdepth))
    IRSHFT(d0, ppp, 16),
    IBAND(d1, ppp, 0xffff),
    IRSHFT(tmp1, tmp0, 16),     // tmp1 <- w
    IBAND(tmp2, tmp0, 0xffff),  // tmp2 <- h
    IMOV(c0, d0),
    IMUL(c0, c0, tmp1),
    IADD(c0, c0, d1),
    IMOV(c1, 0),
    MOV(TVAR(tmp1), IVAR(tmp1)),
    MOV(TVAR(tmp2), IVAR(tmp2)),
    MOV(TVAR(pp0), IVAR(d0)),
    MOV(TVAR(pp1), IVAR(d1)),
    TDIV(tmp0, tmp1, tmp2),     // tmp0 <- ratio
    TDIV(pw, tmp0, tmp1),       // pw <- pixel width
    TMOV(mean, 0),
    TMOV(m2, 0),
    INSERT_TMACRO(to_sample_space, CS(pp0), CS(pp1), CS(tmp1), CS(tmp2), CS(tmp0)),
    TMOV(L0, 0),                // pixel contribution
    TMOV(L1, 0),
    TMOV(L2, 0),
    IMOV(rc, 0),                // ray count
    IMOV(rpt, 0),
    IMOV(ns, 0),                // sample count
// roll
    LABEL(roll),
    IMOV(u0, c0),
    IMOV(u1, c1),
    INSERT_MACRO(threefry2x32r32, CS(u0), CS(u1), scene::K0, scene::K1, scene::K2, CS(tmp0), CS(tmp1)),
    INSERT_MACRO(bump_ctr, CS(c0), CS(c1), CS(39916801), CS(tmp0), CS(tmp1), CS(tmp2)),
    INSERT_TMACRO(sample_u, CS(u0), CS(u1)),
    IPJMP3(rpt, sample_pixel, traverse_scene, lambert_indirect),
// sample pixel
    LABEL(sample_pixel),
    TMOV_CS(CS(o0), scene::CAM_Px),
    TMOV_CS(CS(o1), scene::CAM_Py),
    TMOV_CS(CS(o2), scene::CAM_Pz),
    TMOV(PL0, 1),               // path product
    TMOV(PL1, 1),
    TMOV(PL2, 1),
    TMOV(d0, pp0),
    TMOV(d1, pp1),
    TMOV(clr0, 0),
    TMOV(clr1, 0),
    TMOV(clr2, 0),
    IMOV(depth, 0),
    IADD(ns, ns, 1),
    INSERT_TMACRO(jitter_sample_pos, CS(pw), CS(d), CS(u), CS(tmp0)),
    INSERT_TMACRO(gen_ray_dir, CS(d0), CS(d1)),
    TMOV_CS(CS(d2), scene::CAM_Dz),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp0)),
// traverse
    LABEL(traverse_scene),
    INSERT_TMACRO(bias_ray, CS(vr0)),
    INSERT_TMACRO(traverse_scene),
    IADD(rc, rc, 1),
// post traverse
    LABEL(post_traversal),
    IEQ(tmp0, pid, 11),                // hit light?
    TCMOV_CS(CS(tmp1), CS(tmp0), scene::L_E, CS(1)),
    IEQ(tmp2, pid, 12),                                       // miss?
    IORGE_CS(CS(tmp2), CS(tmp2), CS(depth), CS(maxdepth)),    // ||depth>=maxdepth
    TCMOV(tmp1, tmp2, 0, tmp1),                               // miss||depth>=maxdepth?0:l|1
    INSERT_TMACRO(vec3_scalar_mul, CS(PL), CS(PL), CS(tmp1)), // contribute to path
    ILOR(tmp0, tmp0, tmp2),                                   // hit light||miss||depth>=maxdepth
    IPJMP2(tmp0, set_materials, fin_sample), // contribute sample otherwise proceed
// set materials
    LABEL(set_materials),
    INSERT_TMACRO(vec3_scalar_mul, CS(vr0), CS(d), CS(t)),
    INSERT_TMACRO(vec_vec3_add, CS(o), CS(o), CS(vr0)), // compute new position
    INSERT_TMACRO(set_materials),
    IMOV(rpt, 2),
    JMP(roll),
// indirect diffuse
    LABEL(lambert_indirect),
    INSERT_TMACRO(onb, CS(vr00), CS(vr01), CS(vr02), CS(t0), CS(t1), CS(t2), CS(n0), CS(n1), CS(n2)),
    INSERT_TMACRO(vec3_norm, CS(vr0), CS(tmp0)),
    INSERT_TMACRO(vec3_norm, CS(t), CS(tmp0)),
    INSERT_TMACRO(sample_hemisphere, CS(d), CS(n), CS(vr0), CS(t), CS(u0), CS(u1), CS(tmp0), CS(tmp1)),
    INSERT_TMACRO(vec3_norm, CS(d), CS(tmp0)),
    INSERT_TMACRO(vec_vec3_mul, CS(PL), CS(PL), CS(clr)),
    IMOV(rpt, 0),
    IADD(depth, depth, 1),
    JMP(traverse_scene),
// fin sample
    LABEL(fin_sample),
    INSERT_TMACRO(vec_vec3_add, CS(L), CS(L), CS(PL)), // contribute sample
    MOV(TVAR(tmp2), IVAR(ns)),                         // tmp2 <- ns
    INSERT_TMACRO(luminance, CS(tmp0), CS(PL)),        // computing variance
    TMIN(tmp0, tmp0, 1),                               // tmp0 <- x
    TSUB(tmp1, tmp0, mean),                            // tmp1 <- delta
    TDIV(vr00, tmp1, tmp2),                            // vr00 <- delta/ns
    TADD(mean, mean, vr00),                            // mean += delta/ns
    TSUB(vr01, tmp0, mean),                            // vr01 <- delta2
    TFMA(m2, tmp1, vr01, m2),                          // m2 += delta*delta2
    ISUB(tmp1, ns, 1),          // computing threshold condition
    MOV(TVAR(tmp1), IVAR(tmp1)),
    TDIV(tmp1, m2, tmp1),
    TSQRT(tmp1, tmp1),
    TDIV(tmp1, tmp1, tmp2),     // tmp1 <- std::sqrt(m2 / (ns - 1)) / ns
    TLT(tmp1, tmp1, threshold), // tmp1 > treshold
    IANDGE_CS(CS(tmp0), CS(tmp1), CS(ns), scene::MINSAMPLES),
    IMOV(rpt, 0),
    IPJMP2(tmp0, roll, fin_pixel),
// fin pixel
    LABEL(fin_pixel),
    MOV(TVAR(ns), IVAR(ns)),
    INSERT_TMACRO(vec3_scalar_div, CS(L), CS(L), CS(ns))
KERNEL_END(TVAR(L0), TVAR(L1), TVAR(L2), IVAR(rc), IVAR(ppp));

#endif

template <std::floating_point T>
TMACRO(set_colors)
// 0 - grey floor
// 1 - red wall
// 2 - green wall
// 3 - grey ceiling
// 4 - dark grey back wall
// 5 - blue tetrahedron
// 6
// 7
// 8 - pink sphere
// 9 - orange cylinder
// 10 - orange cylinder cap
// 11 - light
// set colors
    IMIN(tmp0, pid, 7),
//                      0    1    2    3    4      5      6      7
    TSCT8(clr0, tmp0, 0.8, 1.0, 0.2, 0.8, 0.3,   0.0,   0.0,   0.0),
    TSCT8(clr1, tmp0, 0.8, 0.4, 1.0, 0.8, 0.4, 0.212, 0.212, 0.212),
    TSCT8(clr2, tmp0, 0.8, 0.2, 0.4, 0.8, 0.4, 0.553, 0.553, 0.553),
    IMAX(tmp0, pid, 8),
    IMIN(tmp0, tmp0, 11),
    ISUB(tmp0, tmp0, 8),
//                      8      9     10    11
    TSCT4(vr00, tmp0, 0.8,   1.0,   1.0, 1.0),
    TSCT4(vr01, tmp0, 0.2, 0.786, 0.786, 1.0),
    TSCT4(vr02, tmp0, 0.8, 0.427, 0.427, 1.0),
    ILE(tmp0, pid, 7),
    TCMOV(clr0, tmp0, clr0, vr00),
    TCMOV(clr1, tmp0, clr1, vr01),
    TCMOV(clr2, tmp0, clr2, vr02)
MACRO_END;

template <std::same_as<float> T>
KERNEL(primary, IVAR(ppp), IVAR(tmp0), TVAR(u0))
    IRSHFT(d0, ppp, 16),
    IBAND(d1, ppp, 0xffff),
    IRSHFT(tmp1, tmp0, 16),     // tmp1 <- w
    IBAND(tmp2, tmp0, 0xffff),  // tmp2 <- h
    IMOV(c0, d0),
    IMUL(c0, c0, tmp1),
    IADD(c0, c0, d1),
    IMOV(c1, 0),
    MOV(TVAR(tmp1), IVAR(tmp1)),
    MOV(TVAR(tmp2), IVAR(tmp2)),
    MOV(TVAR(pp0), IVAR(d0)),
    MOV(TVAR(pp1), IVAR(d1)),
    TDIV(tmp0, tmp1, tmp2),     // tmp0 <- ratio
    TDIV(pw, tmp0, tmp1),       // pw <- pixel width
    TMUL(pw, pw, u0),
    INSERT_TMACRO(to_sample_space, CS(pp0), CS(pp1), CS(tmp1), CS(tmp2), CS(tmp0)),
    TMOV(L0, 0),                // pixel contribution
    TMOV(L1, 0),
    TMOV(L2, 0),
    IMOV(rc, 0),                // ray count
    IMOV(rpt, 0),
    IMOV(sc, 0),                // sample count
// pixel sample
    LABEL(roll),
    IMOV(u0, c0),
    IMOV(u1, c1),
    INSERT_MACRO(threefry2x32r32, CS(u0), CS(u1), scene::K0, scene::K1, scene::K2, CS(tmp0), CS(tmp1)),
    INSERT_MACRO(bump_ctr, CS(c0), CS(c1), CS(1357), CS(tmp0), CS(tmp1), CS(tmp2)),
    INSERT_TMACRO(sample_u, CS(u0), CS(u1)),
    TMOV_CS(CS(o0), scene::CAM_Px),
    TMOV_CS(CS(o1), scene::CAM_Py),
    TMOV_CS(CS(o2), scene::CAM_Pz),
    TMOV(d0, pp0),
    TMOV(d1, pp1),
    TMOV(clr0, 0),
    TMOV(clr1, 0),
    TMOV(clr2, 0),
    IMOV(depth, 0),
    IADD(sc, sc, 1),
    INSERT_TMACRO(jitter_sample_pos, CS(pw), CS(d), CS(u), CS(tmp0)),
    INSERT_TMACRO(gen_ray_dir, CS(d0), CS(d1)),
    TMOV_CS(CS(d2), scene::CAM_Dz),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp0)),
    INSERT_TMACRO(bias_ray, CS(vr0)),
    INSERT_TMACRO(traverse_scene),
    IADD(rc, rc, 1),
    INSERT_TMACRO(set_colors),
    INSERT_TMACRO(vec_vec3_add, CS(L), CS(L), CS(clr)),
    IGE_CS(CS(tmp0), CS(sc), scene::MINSAMPLES),
    IPJMP2(tmp0, roll, fin_pixel),
// finalize
    LABEL(fin_pixel),
    INSERT_TMACRO(vec3_scalar_div, CS(L), CS(L), scene::MINSAMPLES)
KERNEL_END(TVAR(L0), TVAR(L1), TVAR(L2), IVAR(rc), IVAR(ppp));

template <std::same_as<float> T, typename C>
KERNEL(pt_const, IVAR(ppp))
    IRSHFT(d0, ppp, 16),
    IBAND(d1, ppp, 0xffff),
    IMOV(c0, d0),
    IMUL_CS(CS(c0), CS(c0), typename C::WIDTH),
    IADD(c0, c0, d1),
    IMOV(c1, 0),
    TMOV_CS(CS(tmp1), typename C::WIDTH),
    TMOV_CS(CS(tmp2), typename C::HEIGHT),
    MOV(TVAR(pp0), IVAR(d0)),
    MOV(TVAR(pp1), IVAR(d1)),
    TDIV(tmp0, tmp1, tmp2),     // tmp0 <- ratio
    TDIV(pw, tmp0, tmp1),       // pw <- pixel width
    TMOV(mean, 0),
    TMOV(m2, 0),
    INSERT_TMACRO(to_sample_space, CS(pp0), CS(pp1), CS(tmp1), CS(tmp2), CS(tmp0)),
    TMOV(L0, 0),                // pixel contribution
    TMOV(L1, 0),
    TMOV(L2, 0),
    IMOV(rc, 0),                // ray count
    IMOV(rpt, 0),
    IMOV(ns, 0),                // sample count
// roll
    LABEL(roll),
    IMOV(u0, c0),
    IMOV(u1, c1),
    INSERT_MACRO(threefry2x32r32, CS(u0), CS(u1), scene::K0, scene::K1, scene::K2, CS(tmp0), CS(tmp1)),
    INSERT_MACRO(bump_ctr, CS(c0), CS(c1), CS(39916801), CS(tmp0), CS(tmp1), CS(tmp2)),
    INSERT_TMACRO(sample_u, CS(u0), CS(u1)),
    IPJMP3(rpt, sample_pixel, traverse_scene, lambert_indirect),
// sample pixel
    LABEL(sample_pixel),
    TMOV_CS(CS(o0), scene::CAM_Px),
    TMOV_CS(CS(o1), scene::CAM_Py),
    TMOV_CS(CS(o2), scene::CAM_Pz),
    TMOV(PL0, 1),               // path product
    TMOV(PL1, 1),
    TMOV(PL2, 1),
    TMOV(d0, pp0),
    TMOV(d1, pp1),
    TMOV(clr0, 0),
    TMOV(clr1, 0),
    TMOV(clr2, 0),
    IMOV(depth, 0),
    IADD(ns, ns, 1),
    INSERT_TMACRO(jitter_sample_pos, CS(pw), CS(d), CS(u), CS(tmp0)),
    INSERT_TMACRO(gen_ray_dir, CS(d0), CS(d1)),
    TMOV_CS(CS(d2), scene::CAM_Dz),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp0)),
// traverse
    LABEL(traverse_scene),
    INSERT_TMACRO(bias_ray, CS(vr0)),
    INSERT_TMACRO(traverse_scene),
    IADD(rc, rc, 1),
// post traverse
    LABEL(post_traversal),
    IEQ(tmp0, pid, 11),                // hit light?
    TCMOV_CS(CS(tmp1), CS(tmp0), scene::L_E, CS(1)),
    IEQ(tmp2, pid, 12),                                       // miss?
    IORGE_CS(CS(tmp2), CS(tmp2), CS(depth), typename C::MAXDEPTH), // ||depth>=maxdepth
    TCMOV(tmp1, tmp2, 0, tmp1),                               // miss||depth>=maxdepth?0:l|1
    INSERT_TMACRO(vec3_scalar_mul, CS(PL), CS(PL), CS(tmp1)), // contribute to path
    ILOR(tmp0, tmp0, tmp2),                                   // hit light||miss||depth>=maxdepth
    IPJMP2(tmp0, set_materials, fin_sample), // contribute sample otherwise proceed
// set materials
    LABEL(set_materials),
    INSERT_TMACRO(vec3_scalar_mul, CS(vr0), CS(d), CS(t)),
    INSERT_TMACRO(vec_vec3_add, CS(o), CS(o), CS(vr0)), // compute new position
    INSERT_TMACRO(set_materials),
    IMOV(rpt, 2),
    JMP(roll),
// indirect diffuse
    LABEL(lambert_indirect),
    INSERT_TMACRO(onb, CS(vr00), CS(vr01), CS(vr02), CS(t0), CS(t1), CS(t2), CS(n0), CS(n1), CS(n2)),
    INSERT_TMACRO(vec3_norm, CS(vr0), CS(tmp0)),
    INSERT_TMACRO(vec3_norm, CS(t), CS(tmp0)),
    INSERT_TMACRO(sample_hemisphere, CS(d), CS(n), CS(vr0), CS(t), CS(u0), CS(u1), CS(tmp0), CS(tmp1)),
    INSERT_TMACRO(vec3_norm, CS(d), CS(tmp0)),
    INSERT_TMACRO(vec_vec3_mul, CS(PL), CS(PL), CS(clr)),
    IMOV(rpt, 0),
    IADD(depth, depth, 1),
    JMP(traverse_scene),
// fin sample
    LABEL(fin_sample),
    INSERT_TMACRO(vec_vec3_add, CS(L), CS(L), CS(PL)), // contribute sample
    MOV(TVAR(tmp2), IVAR(ns)),                         // tmp2 <- ns
    INSERT_TMACRO(luminance, CS(tmp0), CS(PL)),        // computing variance
    TMIN(tmp0, tmp0, 1),                               // tmp0 <- x
    TSUB(tmp1, tmp0, mean),                            // tmp1 <- delta
    TDIV(vr00, tmp1, tmp2),                            // vr00 <- delta/ns
    TADD(mean, mean, vr00),                            // mean += delta/ns
    TSUB(vr01, tmp0, mean),                            // vr01 <- delta2
    TFMA(m2, tmp1, vr01, m2),                          // m2 += delta*delta2
    ISUB(tmp1, ns, 1),          // computing threshold condition
    MOV(TVAR(tmp1), IVAR(tmp1)),
    TDIV(tmp1, m2, tmp1),
    TSQRT(tmp1, tmp1),
    TDIV(tmp1, tmp1, tmp2),     // tmp1 <- std::sqrt(m2 / (ns - 1)) / ns
    TLT_CS(CS(tmp1), CS(tmp1), typename C::THRESHOLD), // tmp1 > treshold
    IANDGE_CS(CS(tmp0), CS(tmp1), CS(ns), scene::MINSAMPLES),
    IMOV(rpt, 0),
    IPJMP2(tmp0, roll, fin_pixel),
// fin pixel
    LABEL(fin_pixel),
    MOV(TVAR(ns), IVAR(ns)),
    INSERT_TMACRO(vec3_scalar_div, CS(L), CS(L), CS(ns))
KERNEL_END(TVAR(L0), TVAR(L1), TVAR(L2), IVAR(rc), IVAR(ppp));

template <std::same_as<float> T, typename C>
KERNEL(primary_const, IVAR(ppp))
    IRSHFT(d0, ppp, 16),
    IBAND(d1, ppp, 0xffff),
    IMOV(c0, d0),
    IMUL_CS(CS(c0), CS(c0), typename C::WIDTH),
    IADD(c0, c0, d1),
    IMOV(c1, 0),
    TMOV_CS(CS(tmp1), typename C::WIDTH),
    TMOV_CS(CS(tmp2), typename C::HEIGHT),
    MOV(TVAR(pp0), IVAR(d0)),
    MOV(TVAR(pp1), IVAR(d1)),
    TDIV(tmp0, tmp1, tmp2),     // tmp0 <- ratio
    TDIV(pw, tmp0, tmp1),       // pw <- pixel width
    TMUL_CS(CS(pw), CS(pw), typename C::BLUR),
    INSERT_TMACRO(to_sample_space, CS(pp0), CS(pp1), CS(tmp1), CS(tmp2), CS(tmp0)),
    TMOV(L0, 0),                // pixel contribution
    TMOV(L1, 0),
    TMOV(L2, 0),
    IMOV(rc, 0),                // ray count
    IMOV(rpt, 0),
    IMOV(sc, 0),                // sample count
// pixel sample
    LABEL(roll),
    IMOV(u0, c0),
    IMOV(u1, c1),
    INSERT_MACRO(threefry2x32r32, CS(u0), CS(u1), scene::K0, scene::K1, scene::K2, CS(tmp0), CS(tmp1)),
    INSERT_MACRO(bump_ctr, CS(c0), CS(c1), CS(1357), CS(tmp0), CS(tmp1), CS(tmp2)),
    INSERT_TMACRO(sample_u, CS(u0), CS(u1)),
    TMOV_CS(CS(o0), scene::CAM_Px),
    TMOV_CS(CS(o1), scene::CAM_Py),
    TMOV_CS(CS(o2), scene::CAM_Pz),
    TMOV(d0, pp0),
    TMOV(d1, pp1),
    TMOV(clr0, 0),
    TMOV(clr1, 0),
    TMOV(clr2, 0),
    IMOV(depth, 0),
    IADD(sc, sc, 1),
    INSERT_TMACRO(jitter_sample_pos, CS(pw), CS(d), CS(u), CS(tmp0)),
    INSERT_TMACRO(gen_ray_dir, CS(d0), CS(d1)),
    TMOV_CS(CS(d2), scene::CAM_Dz),
    INSERT_TMACRO(vm::vec3_norm, CS(d), CS(tmp0)),
    INSERT_TMACRO(bias_ray, CS(vr0)),
    INSERT_TMACRO(traverse_scene),
    IADD(rc, rc, 1),
    INSERT_TMACRO(set_colors),
    INSERT_TMACRO(vec_vec3_add, CS(L), CS(L), CS(clr)),
    IGE_CS(CS(tmp0), CS(sc), typename C::NSAMPLES),
    IPJMP2(tmp0, roll, fin_pixel),
// finalize
    LABEL(fin_pixel),
    INSERT_TMACRO(vec3_scalar_div, CS(L), CS(L), typename C::NSAMPLES)
KERNEL_END(TVAR(L0), TVAR(L1), TVAR(L2), IVAR(rc), IVAR(ppp));

template <typename R, typename T, typename N, typename B = CS(1), typename D = CS(10)>
struct constants
{
    using WIDTH = R;
    using HEIGHT = R;
    using THRESHOLD = T;
    using NSAMPLES = N;
    using BLUR = B;
    using MAXDEPTH = D;
};
} // namespace loom::vm
