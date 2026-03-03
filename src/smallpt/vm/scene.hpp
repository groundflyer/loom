// -*- C++ -*-
// scene.hpp
//

#pragma once

#include "linal.hpp"
#include "primitives.hpp"

namespace loom::vm
{
// clang-format off

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
TMACRO4(gen_ray_dir, d0, d1, ndc0, ndc1)
    TMUL_CS(ndc0, ndc0, default_cam::TAN_FOV_HALF),
    TMUL_CS(ndc1, ndc1, default_cam::TAN_FOV_HALF),
    TNEG_CS(d0, ndc0),
    TNEG_CS(d1, ndc1),
    INSERT_TMACRO(vec3_norm, CS_UNIDX(d0), ndc0)
MACRO_END;

// pid[out] primitive id
// o[in] v[3] ray origin
// d[in] v[3] ray direction
template <std::floating_point T>
TMACRO3(traverse, pid, o, d)
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
    TMOV(pp, 0),                // next: set_left_wall
    JMP(isect_quad),
    LABEL(set_left_wall),
    // prim 1 - left wall
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
    TMOV(pp, 1),                // next: set_right_wall
    JMP(isect_quad),
    LABEL(set_right_wall),
    // prim 2 - right wall
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
    TMOV(pp, 2),                // next: set_ceiling
    JMP(isect_quad),
    LABEL(set_ceiling),
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
    TMOV(pp, 3),                // next: set_back_wall
    JMP(isect_quad),
    LABEL(set_back_wall),
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
    TMOV(pp, 4),                // next: traverse_room_end
    JMP(isect_quad),
    LABEL(isect_quad),
    INSERT_TMACRO(isect_quad, CS(hit), CS(ct), o, d,
                  CS(vr0), CS(vr1), CS(vr2), CS(vr3),
                  CS(vr4), CS(vr5), CS(vr6),
                  CS(tmp0), CS(tmp1), CS(tmp2)),
    TMIN(ct, t, ct),
    TCMOV(t, hit, ct, t),       // t <- hit ? min(ct, t) : t
    ICMOV_CS(pid, CS(hit), CS(pp), pid), // pid <- hit ? pp : pid
    IPJMP5(pp, set_left_wall, set_right_wall, set_ceiling, set_back_wall, traverse_room_end),
    LABEL(traverse_room_end)
MACRO_END;
}
