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
} // namespace loom::vm
