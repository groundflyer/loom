// -*- C++ -*-
// niebloids.hpp
//
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


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
