// -*- C++ -*-
// transforms.hpp
//

#pragma once

#include "primitives/aabb.hpp"
#include "primitives/disk.hpp"
#include "primitives/quad.hpp"
#include "primitives/sphere.hpp"
#include "primitives/triangle.hpp"
#include "ray.hpp"
#include "transforms.hpp"

namespace loom
{
MK_NIEBLOID(make_normal)
MK_NIEBLOID(apply_transform)
MK_NIEBLOID(apply_transform_p)
MK_NIEBLOID(apply_transform_n)
MK_NIEBLOID(translate)
MK_NIEBLOID(scale)
MK_NIEBLOID(rotate)
MK_NIEBLOID(look_at)
MK_NIEBLOID(basis)
MK_NIEBLOID(basis_matrix)
} // namespace loom
