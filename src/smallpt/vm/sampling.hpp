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
}
