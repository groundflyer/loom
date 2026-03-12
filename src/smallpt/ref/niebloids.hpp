// -*- C++ -*-
// niebloids.hpp
//

#pragma once

#include "primitives/aabb.hpp"
#include "primitives/cylinder.hpp"
#include "primitives/disk.hpp"
#include "primitives/quad.hpp"
#include "primitives/sphere.hpp"
#include "primitives/triangle.hpp"
#include "ray.hpp"
#include "transforms.hpp"

namespace loom
{
// transforms
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

// primitives interaction
MK_NIEBLOID(intersect)
MK_NIEBLOID(normal)
MK_NIEBLOID(sample)
MK_NIEBLOID(centroid)
MK_NIEBLOID(bound)
} // namespace loom
