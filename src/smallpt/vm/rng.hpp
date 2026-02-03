// -*- C++ -*-
/// @file rng.hpp
/// @brief PRNG for SPMD,

#pragma once

#include <vm/core.hpp>
#include <vm/macros.hpp>
#include <vm/operators.hpp>

namespace loom::vm
{
// clang-format off
MACRO4(rotl32, x, n, dst, tmp)
    BAND_CS(std::uint32_t, dst, n, CS(31)),
    LSHFT_CS(std::uint32_t, dst, x, dst),
    SUB_CS(std::uint32_t, tmp, CS(32), n),
    BAND_CS(std::uint32_t, tmp, tmp, CS(31)),
    RSHFT_CS(std::uint32_t, tmp, x, tmp),
    BOR_CS(std::uint32_t, dst, dst, tmp)
MACRO_END;
}
