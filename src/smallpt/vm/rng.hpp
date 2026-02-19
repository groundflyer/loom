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

MACRO5(threefry_round, c0, c1, r, tmp1, tmp2)
    ADD_CS(std::uint32_t, c0, c0, c1),
    INSERT_MACRO(rotl32, c1, r, tmp1, tmp2),
    XOR_CS(std::uint32_t, c1, tmp1, c0)
MACRO_END;

MACRO5(threefry_key_injection, kc, ks1, ks2, c0, c1)
    ADD_CS(std::uint32_t, c0, c0, ks1),
    ADD_CS(std::uint32_t, c1, c1, ks2),
    ADD_CS(std::uint32_t, c1, c1, kc)
MACRO_END;

MACRO10(threefry_8rounds, c0, c1, kc1, kc2, ks1, ks2, ks3, ks4, tmp1, tmp2)
    INSERT_MACRO(threefry_round, c0, c1, CS(13), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(15), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(26), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(6), tmp1, tmp2),
    INSERT_MACRO(threefry_key_injection, kc1, ks1, ks2, c0, c1),
    INSERT_MACRO(threefry_round, c0, c1, CS(17), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(29), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(16), tmp1, tmp2),
    INSERT_MACRO(threefry_round, c0, c1, CS(24), tmp1, tmp2),
    INSERT_MACRO(threefry_key_injection, kc2, ks3, ks4, c0, c1)
MACRO_END;

MACRO7(threefry2x32r32, c0, c1, k0, k1, k2, tmp1, tmp2) // k2 should be constant too: k2 = 0x1bd11bda ^ k0 ^ k1
    ADD_CS(std::uint32_t, c0, c0, k0),
    ADD_CS(std::uint32_t, c1, c1, k1),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(1), CS(2), k1, k2, k2, k0, tmp1, tmp2),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(3), CS(4), k0, k1, k1, k2, tmp1, tmp2),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(5), CS(6), k2, k0, k0, k1, tmp1, tmp2),
    INSERT_MACRO(threefry_8rounds, c0, c1, CS(7), CS(8), k1, k2, k2, k0, tmp1, tmp2)
MACRO_END;

TMACRO1(gen_canonical, x)
    RSHFT_CS(std::uint32_t, x, x, CS(9)),
    BOR_CS(std::uint32_t, x, x, CS(0x3f800000)),
    TSUB_CS(x, x, CS(1))
MACRO_END;

MACRO5(bump_ctr, c0, c1, i, tmp1, tmp2)
    ADD_CS(std::uint32_t, tmp1, c0, i),
    LT_CS(std::uint32_t, std::uint32_t, tmp2, tmp1, c0),
    PJMP2(VAR_CS(std::uint32_t, tmp2), bump_ctr_end, incr1),
    LABEL(incr1),
    ADD_CS(std::uint32_t, c1, c1, tmp1),
    LABEL(bump_ctr_end),
    CMOV_CS(std::uint32_t, std::uint32_t, c0, tmp2, CS(0), tmp1)
MACRO_END;
// clang-format on
}
