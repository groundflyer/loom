// -*- C++ -*-
// sampling.hpp
//

#pragma once

#include "linal.hpp"
#include "rng.hpp"

namespace loom::vm
{
// clang-format off
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
}
