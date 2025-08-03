// -*- C++ -*-
// macros.hpp
//

#pragma once

#ifndef MAJOR_TYPE
#define MAJOR_TYPE T
#endif

#define INDEX_TYPE       lucid::uint_of_size_t<sizeof(MAJOR_TYPE)>

// convert to char_sequence
#define CS(S)            lucid::sl2cs<#S>

#define CS_JOIN(A, B)    lucid::intlist::join<A, B>

// NAME - char_sequence
#define NVAR(TYPE, NAME) lucid::Variable<TYPE, NAME>
// NAME - literal without quotes
#define VAR(TYPE, NAME)  NVAR(TYPE, CS(NAME))
#define TVAR(NAME)       VAR(MAJOR_TYPE, NAME)
#define IVAR(NAME)       VAR(INDEX_TYPE, NAME)
#define NTVAR(NAME)      NVAR(MAJOR_TYPE, NAME)
#define NIVAR(NAME)      NVAR(INDEX_TYPE, NAME)

#define BEGIN(NAME, ...)                                             \
    using NAME = lucid::Program < lucid::type_sequence<__VA_ARGS__>, \
          lucid::type_sequence <
#define END(...)               >, lucid::type_sequence < __VA_ARGS__ >>
#define TBEGIN1(NAME, v)       BEGIN(NAME, TVAR(v))
#define TBEGIN2(NAME, a, b)    BEGIN(NAME, TVAR(a), TVAR(b))
#define TBEGIN3(NAME, a, b, c) BEGIN(NAME, TVAR(a), TVAR(b), TVAR(c))
#define TBEGIN4(NAME, a, b, c, d) \
    BEGIN(NAME, TVAR(a), TVAR(b), TVAR(c), TVAR(d))
#define TBEGIN5(NAME, a, b, c, d, e) \
    BEGIN(NAME, TVAR(a), TVAR(b), TVAR(c), TVAR(d), TVAR(e))
#define TBEGIN6(NAME, a, b, c, d, e, f) \
    BEGIN(NAME, TVAR(a), TVAR(b), TVAR(c), TVAR(d), TVAR(e), TVAR(f))
#define TEND1(v) \
    END          \
    (TVAR(v))
#define TEND2(a, b) \
    END             \
    (TVAR(a), TVAR(b))
#define IBEGIN1(NAME, v)    BEGIN(NAME, IVAR(v))
#define IBEGIN2(NAME, a, b) BEGIN(NAME, IVAR(a), IVAR(b))
#define IEND1(v) \
    END          \
    (IVAR(v))
#define IEND2(a, b) \
    END             \
    (IVAR(a), IVAR(b))

#define MACRO_BEGIN(NAME) using NAME = lucid::type_sequence <

// parametric macro
#define PMACRO_BEGIN(NAME, ...)                               \
    template <typename MAJOR_TYPE __VA_OPT__(, ) __VA_ARGS__> \
    MACRO_BEGIN(NAME)
#define PMACRO_BEGIN1(NAME, CLASS, T1) PMACRO_BEGIN(NAME, CLASS T1)
#define PMACRO_BEGIN2(NAME, CLASS, T1, T2) \
    PMACRO_BEGIN(NAME, CLASS T1, CLASS T2)
#define PMACRO_BEGIN3(NAME, CLASS, T1, T2, T3) \
    PMACRO_BEGIN(NAME, CLASS T1, CLASS T2, CLASS T3)
#define PMACRO_BEGIN4(NAME, CLASS, T1, T2, T3, T4) \
    PMACRO_BEGIN(NAME, CLASS T1, CLASS T2, CLASS T3, CLASS T4)
#define PMACRO_BEGIN5(NAME, CLASS, T1, T2, T3, T4, T5) \
    PMACRO_BEGIN(NAME, CLASS T1, CLASS T2, CLASS T3, CLASS T4, CLASS T5)
#define PMACRO_BEGIN6(NAME, CLASS, T1, T2, T3, T4, T5, T6) \
    PMACRO_BEGIN(NAME,                                     \
                 CLASS T1,                                 \
                 CLASS T2,                                 \
                 CLASS T3,                                 \
                 CLASS T4,                                 \
                 CLASS T5,                                 \
                 CLASS T6)
#define PMACRO_BEGIN7(NAME, CLASS, T1, T2, T3, T4, T5, T6, T7) \
    PMACRO_BEGIN(NAME,                                         \
                 CLASS T1,                                     \
                 CLASS T2,                                     \
                 CLASS T3,                                     \
                 CLASS T4,                                     \
                 CLASS T5,                                     \
                 CLASS T6,                                     \
                 CLASS T7)
#define TMACRO_BEGIN1(NAME, T1)     PMACRO_BEGIN1(NAME, typename, T1)
#define TMACRO_BEGIN2(NAME, T1, T2) PMACRO_BEGIN2(NAME, typename, T1, T2)
#define TMACRO_BEGIN3(NAME, T1, T2, T3) \
    PMACRO_BEGIN3(NAME, typename, T1, T2, T3)
#define TMACRO_BEGIN4(NAME, T1, T2, T3, T4) \
    PMACRO_BEGIN4(NAME, typename, T1, T2, T3, T4)
#define TMACRO_BEGIN5(NAME, T1, T2, T3, T4, T5) \
    PMACRO_BEGIN5(NAME, typename, T1, T2, T3, T4, T5)
#define TMACRO_BEGIN6(NAME, T1, T2, T3, T4, T5, T6) \
    PMACRO_BEGIN6(NAME, typename, T1, T2, T3, T4, T5, T6)
#define TMACRO_BEGIN7(NAME, T1, T2, T3, T4, T5, T6, T7) \
    PMACRO_BEGIN7(NAME, typename, T1, T2, T3, T4, T5, T6, T7)
#define MACRO_END                >

#define INSERT_MACRO(NAME, ...)  NAME __VA_OPT__(<__VA_ARGS__>)
#define INSERT_TMACRO(NAME, ...) INSERT_MACRO(NAME, MAJOR_TYPE, __VA_ARGS__)

#define MOV(a, b)                lucid::Compute<std::identity, a, b>
#define TMOV(a, b)               MOV(TVAR(a), TVAR(b))
#define IMOV(a, b)               MOV(IVAR(a), IVAR(b))
#define TMOV_CS(a, b)            MOV(NTVAR(a), NTVAR(b))
#define IMOV_CS(a, b)            MOV(NIVAR(a), NIVAR(b))

#define LABEL_CS(NAME)           lucid::Label<NAME>
#define LABEL(NAME)              LABEL_CS(CS(NAME))
#define PREFIX_LABEL_CS(_P, _L)  LABEL_CS(CS_JOIN(_P, _L))

#define JMP(TARGET)             \
    lucid::Jump < LABEL(TARGET) \
    >

#define PJMPN(VN, ...) \
    lucid::ParmJump<VAR(INDEX_TYPE, VN), lucid::type_sequence<__VA_ARGS__>>
#define PJMP2(VN, L1, L2)                                               \
    lucid::ParmJump < VAR(INDEX_TYPE, VN), lucid::type_pair < LABEL(L1) \
    , LABEL(L2)                                                         \
    >>
#define PJMP3(VN, L1, L2, L3)                                               \
    lucid::ParmJump < VAR(INDEX_TYPE, VN), lucid::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                             \
    , LABEL(L3)                                                             \
    >>
#define PJMP4(VN, L1, L2, L3, L4)                                           \
    lucid::ParmJump < VAR(INDEX_TYPE, VN), lucid::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                             \
    , LABEL(L3)                                                             \
    , LABEL(L4)                                                             \
    >>
#define PJMP2_CS(VN, L1, L2)                                             \
    lucid::ParmJump < NVAR(INDEX_TYPE, VN), lucid::type_pair < LABEL(L1) \
    , LABEL(L2)                                                          \
    >>
#define PJMP3_CS(VN, L1, L2, L3)                                             \
    lucid::ParmJump < NVAR(INDEX_TYPE, VN), lucid::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                              \
    , LABEL(L3)                                                              \
    >>
#define PJMP4_CS(VN, L1, L2, L3, L4)                                         \
    lucid::ParmJump < NVAR(INDEX_TYPE, VN), lucid::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                              \
    , LABEL(L3)                                                              \
    , LABEL(L4)                                                              \
    >>

#define BINOP_CS(OP, TYPE, DST, A, B) \
    lucid::Compute<OP<TYPE>, NVAR(TYPE, DST), NVAR(TYPE, A), NVAR(TYPE, B)>
#define BINOP(OP, TYPE, DST, A, B) BINOP_CS(OP, TYPE, CS(DST), CS(A), CS(B))

#define UNOP_CS(OP, TYPE, DST, V) \
    lucid::Compute<OP<TYPE>, NVAR(TYPE, DST), NVAR(TYPE, V)>
#define UNOP(OP, TYPE, DST, V) UNOP_CS(OP, TYPE, CS(DST), CS(V))

#define CMP_CS(OP, TYPE, DST, A, B) \
    lucid::Compute<OP<TYPE>, NIVAR(DST), NVAR(TYPE, A), NVAR(TYPE, B)>
#define CMP(OP, TYPE, DST, A, B) CMP_CS(OP, TYPE, CS(DST), CS(A), CS(B))

#define CHOICE_CS(TYPE, DST, C, A, B)  \
    lucid::Compute<lucid::ops::choice, \
                   NVAR(TYPE, DST),    \
                   NIVAR(C),           \
                   NVAR(TYPE, A),      \
                   NVAR(TYPE, B)>
#define CHOICE(TYPE, DST, C, A, B) CHOICE_CS(TYPE, CS(DST), CS(C), CS(A), CS(B))

#define ADD(...)                   BINOP(std::plus, __VA_ARGS__)
#define SUB(...)                   BINOP(std::minus, __VA_ARGS__)
#define MUL(...)                   BINOP(std::multiplies, __VA_ARGS__)
#define DIV(...)                   BINOP(std::divides, __VA_ARGS__)
#define MOD(...)                   BINOP(std::modulus, __VA_ARGS__)
#define NEG(...)                   UNOP(std::negate, __VA_ARGS__)

#define TADD(...)                  ADD(MAJOR_TYPE, __VA_ARGS__)
#define TSUB(...)                  SUB(MAJOR_TYPE, __VA_ARGS__)
#define TMUL(...)                  MUL(MAJOR_TYPE, __VA_ARGS__)
#define TDIV(...)                  DIV(MAJOR_TYPE, __VA_ARGS__)
#define TMOD(...)                  MOD(MAJOR_TYPE, __VA_ARGS__)
#define TNEG(...)                  NEG(MAJOR_TYPE, __VA_ARGS__)

#define IADD(...)                  ADD(INDEX_TYPE, __VA_ARGS__)
#define ISUB(...)                  SUB(INDEX_TYPE, __VA_ARGS__)
#define IMUL(...)                  MUL(INDEX_TYPE, __VA_ARGS__)
#define IDIV(...)                  DIV(INDEX_TYPE, __VA_ARGS__)
#define IMOD(...)                  MOD(INDEX_TYPE, __VA_ARGS__)
#define INEG(...)                  NEG(INDEX_TYPE, __VA_ARGS__)

#define ADD_CS(...)                BINOP_CS(std::plus, __VA_ARGS__)
#define SUB_CS(...)                BINOP_CS(std::minus, __VA_ARGS__)
#define MUL_CS(...)                BINOP_CS(std::multiplies, __VA_ARGS__)
#define DIV_CS(...)                BINOP_CS(std::divides, __VA_ARGS__)
#define MOD_CS(...)                BINOP_CS(std::modulus, __VA_ARGS__)
#define NEG_CS(...)                UNOP_CS(std::negate, __VA_ARGS__)

#define TADD_CS(...)               ADD_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSUB_CS(...)               SUB_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMUL_CS(...)               MUL_CS(MAJOR_TYPE, __VA_ARGS__)
#define TDIV_CS(...)               DIV_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMOD_CS(...)               MOD_CS(MAJOR_TYPE, __VA_ARGS__)
#define TNEG_CS(...)               NEG_CS(MAJOR_TYPE, __VA_ARGS__)

#define IADD_CS(...)               ADD_CS(INDEX_TYPE, __VA_ARGS__)
#define ISUB_CS(...)               SUB_CS(INDEX_TYPE, __VA_ARGS__)
#define IMUL_CS(...)               MUL_CS(INDEX_TYPE, __VA_ARGS__)
#define IDIV_CS(...)               DIV_CS(INDEX_TYPE, __VA_ARGS__)
#define IMOD_CS(...)               MOD_CS(INDEX_TYPE, __VA_ARGS__)
#define INEG_CS(...)               NEG_CS(INDEX_TYPE, __VA_ARGS__)

#define EQ(...)                    CMP(std::equal_to, __VA_ARGS__)
#define NEQ(...)                   CMP(std::not_equal_to, __VA_ARGS__)
#define GT(...)                    CMP(std::greater, __VA_ARGS__)
#define LT(...)                    CMP(std::less, __VA_ARGS__)
#define GE(...)                    CMP(std::greater_equal, __VA_ARGS__)
#define LE(...)                    CMP(std::less_equal, __VA_ARGS__)

#define EQ_CS(...)                 CMP_CS(std::equal_to, __VA_ARGS__)
#define NEQ_CS(...)                CMP_CS(std::not_equal_to, __VA_ARGS__)
#define GT_CS(...)                 CMP_CS(std::greater, __VA_ARGS__)
#define LT_CS(...)                 CMP_CS(std::less, __VA_ARGS__)
#define GE_CS(...)                 CMP_CS(std::greater_equal, __VA_ARGS__)
#define LE_CS(...)                 CMP_CS(std::less_equal, __VA_ARGS__)

#define TEQ(...)                   EQ(MAJOR_TYPE, __VA_ARGS__)
#define TNEQ(...)                  NEQ(MAJOR_TYPE, __VA_ARGS__)
#define TGT(...)                   GT(MAJOR_TYPE, __VA_ARGS__)
#define TLT(...)                   LT(MAJOR_TYPE, __VA_ARGS__)
#define TGE(...)                   GE(MAJOR_TYPE, __VA_ARGS__)
#define TLE(...)                   LE(MAJOR_TYPE, __VA_ARGS__)

#define IEQ(...)                   EQ(INDEX_TYPE, __VA_ARGS__)
#define INEQ(...)                  NEQ(INDEX_TYPE, __VA_ARGS__)
#define IGT(...)                   GT(INDEX_TYPE, __VA_ARGS__)
#define ILT(...)                   LT(INDEX_TYPE, __VA_ARGS__)
#define IGE(...)                   GE(INDEX_TYPE, __VA_ARGS__)
#define ILE(...)                   LE(INDEX_TYPE, __VA_ARGS__)

#define TEQ_CS(...)                EQ_CS(MAJOR_TYPE, __VA_ARGS__)
#define TNEQ_CS(...)               NEQ_CS(MAJOR_TYPE, __VA_ARGS__)
#define TGT_CS(...)                GT_CS(MAJOR_TYPE, __VA_ARGS__)
#define TLT_CS(...)                LT_CS(MAJOR_TYPE, __VA_ARGS__)
#define TGE_CS(...)                GE_CS(MAJOR_TYPE, __VA_ARGS__)
#define TLE_CS(...)                LE_CS(MAJOR_TYPE, __VA_ARGS__)

#define IEQ_CS(...)                EQ_CS(INDEX_TYPE, __VA_ARGS__)
#define INEQ_CS(...)               NEQ_CS(INDEX_TYPE, __VA_ARGS__)
#define IGT_CS(...)                GT_CS(INDEX_TYPE, __VA_ARGS__)
#define ILT_CS(...)                LT_CS(INDEX_TYPE, __VA_ARGS__)
#define IGE_CS(...)                GE_CS(INDEX_TYPE, __VA_ARGS__)
#define ILE_CS(...)                LE_CS(INDEX_TYPE, __VA_ARGS__)

#define LAND(...)                  BINOP(std::logical_and, INDEX_TYPE, __VA_ARGS__)
#define LOR(...)                   BINOP(std::logical_or, INDEX_TYPE, __VA_ARGS__)
#define LNOT(...)                  UNOP(std::logical_not, INDEX_TYPE, __VA_ARGS__)

#define LAND_CS(...)               BINOP_CS(std::logical_and, INDEX_TYPE, __VA_ARGS__)
#define LOR_CS(...)                BINOP_CS(std::logical_or, INDEX_TYPE, __VA_ARGS__)
#define LNOT_CS(...)               UNOP_CS(std::logical_not, INDEX_TYPE, __VA_ARGS__)

#define BAND(...)                  BINOP(std::bit_and, INDEX_TYPE, __VA_ARGS__)
#define BOR(...)                   BINOP(std::bit_or, INDEX_TYPE, __VA_ARGS__)
#define XOR(...)                   BINOP(std::bit_xor, INDEX_TYPE, __VA_ARGS__)
#define BNOT(...)                  UNOP(std::bit_not, INDEX_TYPE, __VA_ARGS__)

#define BAND_CS(...)               BINOP_CS(std::bit_and, INDEX_TYPE, __VA_ARGS__)
#define BOR_CS(...)                BINOP_CS(std::bit_or, INDEX_TYPE, __VA_ARGS__)
#define XOR_CS(...)                BINOP_CS(std::bit_xor, INDEX_TYPE, __VA_ARGS__)
#define BNOT_CS(...)               UNOP_CS(std::bit_not, INDEX_TYPE, __VA_ARGS__)

#define TCHOICE(...)               CHOICE(MAJOR_TYPE, __VA_ARGS__)
#define ICHOICE(...)               CHOICE(INDEX_TYPE, __VA_ARGS__)

#define TCHOICE_CS(...)            CHOICE_CS(MAJOR_TYPE, __VA_ARGS__)
#define ICHOICE_CS(...)            CHOICE_CS(INDEX_TYPE, __VA_ARGS__)

#define LSHFT(...)                 BINOP(lucid::ops::lshift, INDEX_TYPE, __VA_ARGS__)
#define RSHFT(...)                 BINOP(lucid::ops::rshift, INDEX_TYPE, __VA_ARGS__)

#define LSHFT_CS(...)              BINOP_CS(lucid::ops::lshift, INDEX_TYPE, __VA_ARGS__)
#define RSHFT_CS(...)              BINOP_CS(lucid::ops::rshift, INDEX_TYPE, __VA_ARGS__)

#define ABS(...)                   UNOP(lucid::ops::abs, __VA_ARGS__)
#define ROUND(...)                 UNOP(lucid::ops::round, __VA_ARGS__)
#define CEIL(...)                  UNOP(lucid::ops::ceil, __VA_ARGS__)
#define FLOOR(...)                 UNOP(lucid::ops::floor, __VA_ARGS__)
#define TRUNC(...)                 UNOP(lucid::ops::trunc, __VA_ARGS__)
#define COPYSIGN(...)              UNOP(lucid::ops::copysign, __VA_ARGS__)
#define SIN(...)                   UNOP(lucid::ops::sin, __VA_ARGS__)
#define COS(...)                   UNOP(lucid::ops::cos, __VA_ARGS__)
#define MIN(...)                   BINOP(lucid::ops::min, __VA_ARGS__)
#define MAX(...)                   BINOP(lucid::ops::max, __VA_ARGS__)

#define TABS(...)                  ABS(MAJOR_TYPE, __VA_ARGS__)
#define TROUND(...)                ROUND(MAJOR_TYPE, __VA_ARGS__)
#define TCEIL(...)                 CEIL(MAJOR_TYPE, __VA_ARGS__)
#define TFLOOR(...)                FLOOR(MAJOR_TYPE, __VA_ARGS__)
#define TTRUNC(...)                TRUNC(MAJOR_TYPE, __VA_ARGS__)
#define TCOPYSIGN(...)             COPYSIGN(MAJOR_TYPE, __VA_ARGS__)
#define TSIN(...)                  SIN(MAJOR_TYPE, __VA_ARGS__)
#define TCOS(...)                  COS(MAJOR_TYPE, __VA_ARGS__)
#define TMIN(...)                  MIN(MAJOR_TYPE, __VA_ARGS__)
#define TMAX(...)                  MAX(MAJOR_TYPE, __VA_ARGS__)

#define ABS_CS(...)                UNOP_CS(lucid::ops::abs, __VA_ARGS__)
#define ROUND_CS(...)              UNOP_CS(lucid::ops::round, __VA_ARGS__)
#define CEIL_CS(...)               UNOP_CS(lucid::ops::ceil, __VA_ARGS__)
#define FLOOR_CS(...)              UNOP_CS(lucid::ops::floor, __VA_ARGS__)
#define TRUNC_CS(...)              UNOP_CS(lucid::ops::trunc, __VA_ARGS__)
#define COPYSIGN_CS(...)           UNOP_CS(lucid::ops::copysign, __VA_ARGS__)
#define SIN_CS(...)                UNOP_CS(lucid::ops::sin, __VA_ARGS__)
#define COS_CS(...)                UNOP_CS(lucid::ops::cos, __VA_ARGS__)
#define MIN_CS(...)                BINOP_CS(lucid::ops::min, __VA_ARGS__)
#define MAX_CS(...)                BINOP_CS(lucid::ops::max, __VA_ARGS__)

#define TABS_CS(...)               ABS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TROUND_CS(...)             ROUND_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCEIL_CS(...)              CEIL_CS(MAJOR_TYPE, __VA_ARGS__)
#define TFLOOR_CS(...)             FLOOR_CS(MAJOR_TYPE, __VA_ARGS__)
#define TTRUNC_CS(...)             TRUNC_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCOPYSIGN_CS(...)          COPYSIGN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSIN_CS(...)               SIN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCOS_CS(...)               COS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMIN_CS(...)               MIN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMAX_CS(...)               MAX_CS(MAJOR_TYPE, __VA_ARGS__)
