// -*- C++ -*-
// pt_macros.hpp
//

#pragma once

#include <smallpt/vm/pt.hpp>
#include <vm/dbg.hpp>

namespace loom::vm
{
using namespace dbg;

using pt_macros = type_sequence<PRETTIFY_MACRO(rotl32),
                                PRETTIFY_MACRO(threefry_round),
                                PRETTIFY_MACRO(threefry_key_injection),
                                PRETTIFY_MACRO(threefry_8rounds),
                                PRETTIFY_MACRO(threefry2x32r32),
                                PRETTIFY_MACRO(bump_ctr),
                                PRETTIFY_TMACRO(float, gen_canonical),
                                PRETTIFY_TMACRO(float, vec2_scalar_sub),
                                PRETTIFY_TMACRO(float, vec2_scalar_mul),
                                PRETTIFY_TMACRO(float, vec_vec2_add),
                                PRETTIFY_TMACRO(float, vec_vec3_add),
                                PRETTIFY_TMACRO(float, vec_vec3_sub),
                                PRETTIFY_TMACRO(float, vec_vec3_mul),
                                PRETTIFY_TMACRO(float, vec_vec3_div),
                                PRETTIFY_TMACRO(float, vec3_scalar_add),
                                PRETTIFY_TMACRO(float, vec3_scalar_sub),
                                PRETTIFY_TMACRO(float, vec3_scalar_mul),
                                PRETTIFY_TMACRO(float, vec3_scalar_div),
                                PRETTIFY_TMACRO(float, vec3_dot),
                                PRETTIFY_TMACRO(float, vec3_cross),
                                PRETTIFY_TMACRO(float, vec3_length2),
                                PRETTIFY_TMACRO(float, vec3_length),
                                PRETTIFY_TMACRO(float, vec3_norm),
                                PRETTIFY_TMACRO(float, vec3_dist),
                                PRETTIFY_TMACRO(float, isect_disk),
                                PRETTIFY_TMACRO(float, solve_quadratic),
                                PRETTIFY_TMACRO(float, prep_isect_sphere),
                                PRETTIFY_TMACRO(float, cyl_prep_abc),
                                PRETTIFY_TMACRO(float, cyl_fin),
                                PRETTIFY_TMACRO(float, isect_triangle),
                                PRETTIFY_TMACRO(float, isect_quad),
                                PRETTIFY_TMACRO(float, sample_u),
                                PRETTIFY_TMACRO(float, jitter_sample_pos),
                                PRETTIFY_TMACRO(float, onb),
                                PRETTIFY_TMACRO(float, sample_hemisphere),
                                PRETTIFY_TMACRO(float, to_sample_space),
                                PRETTIFY_TMACRO(float, sample_light_disk),
                                PRETTIFY_TMACRO(float, gen_ray_dir),
                                PRETTIFY_TMACRO(float, traverse_scene),
                                PRETTIFY_TMACRO(float, set_materials),
                                PRETTIFY_TMACRO(float, bias_ray),
                                PRETTIFY_TMACRO(float, luminance),
                                PRETTIFY_TMACRO(float, schlick),
                                PRETTIFY_TMACRO(float, reflect),
                                PRETTIFY_TMACRO(float, refract),
                                PRETTIFY_TMACRO(float, compute_eta),
                                PRETTIFY_TMACRO(float, reflectance),
                                PRETTIFY_TMACRO(float, comp_f),
                                PRETTIFY_TMACRO(float, gtr),
                                PRETTIFY_TMACRO(float, gaf),
                                PRETTIFY_TMACRO(float, get_slope),
                                PRETTIFY_TMACRO(float, microfacet),
                                PRETTIFY_TMACRO(float, anisorough_i),
                                PRETTIFY_TMACRO(float, anisorough_n)>;
} // namespace loom::vm
