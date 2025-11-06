// -*- C++ -*-
// macros.hpp
//

#pragma once

#ifndef MAJOR_TYPE
#define MAJOR_TYPE T
#endif

#define INDEX_TYPE       loom::uint_of_size_t<sizeof(MAJOR_TYPE)>

// convert to char_sequence
#define CS(S)            loom::sl2cs<#S>

#define CS_JOIN(A, B)    loom::intlist::join<A, B>

// NAME - char_sequence
#define NVAR(TYPE, NAME) loom::vm::Variable<TYPE, NAME>
// NAME - literal without quotes
#define VAR(TYPE, NAME)  NVAR(TYPE, CS(NAME))
#define TVAR(NAME)       VAR(MAJOR_TYPE, NAME)
#define IVAR(NAME)       VAR(INDEX_TYPE, NAME)
#define NTVAR(NAME)      NVAR(MAJOR_TYPE, NAME)
#define NIVAR(NAME)      NVAR(INDEX_TYPE, NAME)

#define KERNEL(NAME, ...)                            \
    using NAME = loom::vm::kernel_blank < CS(NAME), \
          loom::type_sequence<__VA_ARGS__>, loom::type_sequence <
#define KERNEL_END(...)         >, loom::type_sequence < __VA_ARGS__ >>
#define TKERNEL1(NAME, v)       KERNEL(NAME, TVAR(v))
#define TKERNEL2(NAME, a, b)    KERNEL(NAME, TVAR(a), TVAR(b))
#define TKERNEL3(NAME, a, b, c) KERNEL(NAME, TVAR(a), TVAR(b), TVAR(c))
#define TKERNEL4(NAME, a, b, c, d) \
    KERNEL(NAME, TVAR(a), TVAR(b), TVAR(c), TVAR(d))
#define TKERNEL5(NAME, a, b, c, d, e) \
    KERNEL(NAME, TVAR(a), TVAR(b), TVAR(c), TVAR(d), TVAR(e))
#define TKERNEL6(NAME, a, b, c, d, e, f) \
    KERNEL(NAME, TVAR(a), TVAR(b), TVAR(c), TVAR(d), TVAR(e), TVAR(f))
#define TKERNEL_END1(v) \
    KERNEL_END          \
    (TVAR(v))
#define TKERNEL_END2(a, b) \
    KERNEL_END             \
    (TVAR(a), TVAR(b))
#define IKERNEL1(NAME, v)    KERNEL(NAME, IVAR(v))
#define IKERNEL2(NAME, a, b) KERNEL(NAME, IVAR(a), IVAR(b))
#define IKERNEL_END1(v) \
    KERNEL_END          \
    (IVAR(v))
#define IKERNEL_END2(a, b) \
    KERNEL_END             \
    (IVAR(a), IVAR(b))

#define __MACRO_PREFIX __macro_decl_

#define __MACRO_DECLARE(MACRO_NAME, ...) \
    using __MACRO_PREFIX##MACRO_NAME =   \
        loom::vm::macro_decl<CS(MACRO_NAME) __VA_OPT__(, ) __VA_ARGS__>
// #define __MACRO_DECLARE(NAME, ...) \
//     __MACRO_DECLARE_PREFIX(__MACRO_PREFIX, NAME, __VA_ARGS__)
#define __MACRO_DEF_BEGIN(NAME) \
    struct NAME                 \
    {                           \
        using type = loom::type_sequence <

#define MACRO(NAME)        \
    __MACRO_DECLARE(NAME); \
    __MACRO_DEF_BEGIN(NAME)

#define MACRO1(NAME, P1)           \
    __MACRO_DECLARE(NAME, CS(P1)); \
    template <typename P1>         \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO2(NAME, P1, P2)               \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2)); \
    template <typename P1, typename P2>    \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO3(NAME, P1, P2, P3)                     \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3));   \
    template <typename P1, typename P2, typename P3> \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO4(NAME, P1, P2, P3, P4)                              \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3), CS(P4));        \
    template <typename P1, typename P2, typename P3, typename P4> \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO5(NAME, P1, P2, P3, P4, P5)                                       \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3), CS(P4), CS(P5));             \
    template <typename P1, typename P2, typename P3, typename P4, typename P5> \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO6(NAME, P1, P2, P3, P4, P5, P6)                               \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3), CS(P4), CS(P5), CS(P6)); \
    template <typename P1,                                                 \
              typename P2,                                                 \
              typename P3,                                                 \
              typename P4,                                                 \
              typename P5,                                                 \
              typename P6>                                                 \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO7(NAME, P1, P2, P3, P4, P5, P6, P7) \
    __MACRO_DECLARE(NAME,                        \
                    CS(P1),                      \
                    CS(P2),                      \
                    CS(P3),                      \
                    CS(P4),                      \
                    CS(P5),                      \
                    CS(P6),                      \
                    CS(P7));                     \
    template <typename P1,                       \
              typename P2,                       \
              typename P3,                       \
              typename P4,                       \
              typename P5,                       \
              typename P6,                       \
              typename P7>                       \
    __MACRO_DEF_BEGIN(NAME)

#define TMACRO1(NAME, P1)                       \
    __MACRO_DECLARE(NAME, CS(P1));              \
    template <typename MAJOR_TYPE, typename P1> \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO2(NAME, P1, P2)                                \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2));                   \
    template <typename MAJOR_TYPE, typename P1, typename P2> \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO3(NAME, P1, P2, P3)                                         \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3));                        \
    template <typename MAJOR_TYPE, typename P1, typename P2, typename P3> \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO4(NAME, P1, P2, P3, P4)                      \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3), CS(P4)); \
    template <typename MAJOR_TYPE,                         \
              typename P1,                                 \
              typename P2,                                 \
              typename P3,                                 \
              typename P4>                                 \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO5(NAME, P1, P2, P3, P4, P5)                          \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3), CS(P4), CS(P5)); \
    template <typename MAJOR_TYPE,                                 \
              typename P1,                                         \
              typename P2,                                         \
              typename P3,                                         \
              typename P4,                                         \
              typename P5>                                         \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO6(NAME, P1, P2, P3, P4, P5, P6)                              \
    __MACRO_DECLARE(NAME, CS(P1), CS(P2), CS(P3), CS(P4), CS(P5), CS(P6)); \
    template <typename MAJOR_TYPE,                                         \
              typename P1,                                                 \
              typename P2,                                                 \
              typename P3,                                                 \
              typename P4,                                                 \
              typename P5,                                                 \
              typename P6>                                                 \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO7(NAME, P1, P2, P3, P4, P5, P6, P7) \
    __MACRO_DECLARE(NAME,                         \
                    CS(P1),                       \
                    CS(P2),                       \
                    CS(P3),                       \
                    CS(P4),                       \
                    CS(P5),                       \
                    CS(P6),                       \
                    CS(P7));                      \
    template <typename MAJOR_TYPE,                \
              typename P1,                        \
              typename P2,                        \
              typename P3,                        \
              typename P4,                        \
              typename P5,                        \
              typename P6,                        \
              typename P7>                        \
    __MACRO_DEF_BEGIN(NAME)

#define MACRO_END \
    > ;           \
    }

#define __INSERT_MACRO_INFO(NAME, ...) \
    loom::vm::macro_inv<CS(NAME) __VA_OPT__(, ) __VA_ARGS__>
#define INSERT_MACRO(NAME, ...)             \
    __INSERT_MACRO_INFO(NAME, __VA_ARGS__), \
        typename NAME __VA_OPT__(<__VA_ARGS__>)::type
#define INSERT_TMACRO(NAME, ...)            \
    __INSERT_MACRO_INFO(NAME, __VA_ARGS__), \
        typename NAME<MAJOR_TYPE __VA_OPT__(, ) __VA_ARGS__>::type

#define MOV(a, b)               loom::vm::Compute<std::identity, a, b>
#define TMOV(a, b)              MOV(TVAR(a), TVAR(b))
#define IMOV(a, b)              MOV(IVAR(a), IVAR(b))
#define TMOV_CS(a, b)           MOV(NTVAR(a), NTVAR(b))
#define IMOV_CS(a, b)           MOV(NIVAR(a), NIVAR(b))

#define LABEL_CS(NAME)          loom::vm::Label<NAME>
#define LABEL(NAME)             LABEL_CS(CS(NAME))
#define PREFIX_LABEL_CS(_P, _L) LABEL_CS(CS_JOIN(_P, _L))

#define JMP(TARGET)                 \
    loom::vm::Jump < LABEL(TARGET) \
    >

#define PJMPN(VN, ...) \
    loom::vm::ParmJump<VAR(INDEX_TYPE, VN), loom::type_sequence<__VA_ARGS__>>
#define PJMP2(VN, L1, L2)                                                   \
    loom::vm::ParmJump < VAR(INDEX_TYPE, VN), loom::type_pair < LABEL(L1) \
    , LABEL(L2)                                                             \
    >>
#define PJMP3(VN, L1, L2, L3)                  \
    loom::vm::ParmJump < VAR(INDEX_TYPE, VN), \
        loom::type_sequence < LABEL(L1)       \
    , LABEL(L2)                                \
    , LABEL(L3)                                \
    >>
#define PJMP4(VN, L1, L2, L3, L4)              \
    loom::vm::ParmJump < VAR(INDEX_TYPE, VN), \
        loom::type_sequence < LABEL(L1)       \
    , LABEL(L2)                                \
    , LABEL(L3)                                \
    , LABEL(L4)                                \
    >>
#define PJMP2_CS(VN, L1, L2)                                                 \
    loom::vm::ParmJump < NVAR(INDEX_TYPE, VN), loom::type_pair < LABEL(L1) \
    , LABEL(L2)                                                              \
    >>
#define PJMP3_CS(VN, L1, L2, L3)                \
    loom::vm::ParmJump < NVAR(INDEX_TYPE, VN), \
        loom::type_sequence < LABEL(L1)        \
    , LABEL(L2)                                 \
    , LABEL(L3)                                 \
    >>
#define PJMP4_CS(VN, L1, L2, L3, L4)            \
    loom::vm::ParmJump < NVAR(INDEX_TYPE, VN), \
        loom::type_sequence < LABEL(L1)        \
    , LABEL(L2)                                 \
    , LABEL(L3)                                 \
    , LABEL(L4)                                 \
    >>

#define BINOP_CS(OP, TYPE, DST, A, B) \
    loom::vm::Compute<OP<TYPE>, NVAR(TYPE, DST), NVAR(TYPE, A), NVAR(TYPE, B)>
#define BINOP(OP, TYPE, DST, A, B) BINOP_CS(OP, TYPE, CS(DST), CS(A), CS(B))

#define UNOP_CS(OP, TYPE, DST, V) \
    loom::vm::Compute<OP<TYPE>, NVAR(TYPE, DST), NVAR(TYPE, V)>
#define UNOP(OP, TYPE, DST, V) UNOP_CS(OP, TYPE, CS(DST), CS(V))

#define CMP_CS(OP, TYPE, DST, A, B) \
    loom::vm::Compute<OP<TYPE>, NIVAR(DST), NVAR(TYPE, A), NVAR(TYPE, B)>
#define CMP(OP, TYPE, DST, A, B) CMP_CS(OP, TYPE, CS(DST), CS(A), CS(B))

#define CMOV_CS(TYPE, DST, C, A, B)          \
    loom::vm::Compute<loom::vm::ops::cmov, \
                       NVAR(TYPE, DST),      \
                       NIVAR(C),             \
                       NVAR(TYPE, A),        \
                       NVAR(TYPE, B)>
#define CMOV(TYPE, DST, C, A, B) CMOV_CS(TYPE, CS(DST), CS(C), CS(A), CS(B))

#define FMA_CS(TYPE, DST, X, Y, Z)          \
    loom::vm::Compute<loom::vm::ops::fma, \
                       NVAR(TYPE, DST),     \
                       NVAR(TYPE, X),       \
                       NVAR(TYPE, Y),       \
                       NVAR(TYPE, Z)>
#define TFMA_CS(...)       FMA_CS(MAJOR_TYPE, __VA_ARGS__)
#define TFMA(DST, X, Y, Z) TFMA_CS(CS(DST), CS(X), CS(Y), CS(Z))

#define ADD(...)           BINOP(std::plus, __VA_ARGS__)
#define SUB(...)           BINOP(std::minus, __VA_ARGS__)
#define MUL(...)           BINOP(std::multiplies, __VA_ARGS__)
#define DIV(...)           BINOP(std::divides, __VA_ARGS__)
#define MOD(...)           BINOP(std::modulus, __VA_ARGS__)
#define NEG(...)           UNOP(std::negate, __VA_ARGS__)

#define TADD(...)          ADD(MAJOR_TYPE, __VA_ARGS__)
#define TSUB(...)          SUB(MAJOR_TYPE, __VA_ARGS__)
#define TMUL(...)          MUL(MAJOR_TYPE, __VA_ARGS__)
#define TDIV(...)          DIV(MAJOR_TYPE, __VA_ARGS__)
#define TMOD(...)          MOD(MAJOR_TYPE, __VA_ARGS__)
#define TNEG(...)          NEG(MAJOR_TYPE, __VA_ARGS__)

#define IADD(...)          ADD(INDEX_TYPE, __VA_ARGS__)
#define ISUB(...)          SUB(INDEX_TYPE, __VA_ARGS__)
#define IMUL(...)          MUL(INDEX_TYPE, __VA_ARGS__)
#define IDIV(...)          DIV(INDEX_TYPE, __VA_ARGS__)
#define IMOD(...)          MOD(INDEX_TYPE, __VA_ARGS__)
#define INEG(...)          NEG(INDEX_TYPE, __VA_ARGS__)

#define ADD_CS(...)        BINOP_CS(std::plus, __VA_ARGS__)
#define SUB_CS(...)        BINOP_CS(std::minus, __VA_ARGS__)
#define MUL_CS(...)        BINOP_CS(std::multiplies, __VA_ARGS__)
#define DIV_CS(...)        BINOP_CS(std::divides, __VA_ARGS__)
#define MOD_CS(...)        BINOP_CS(std::modulus, __VA_ARGS__)
#define NEG_CS(...)        UNOP_CS(std::negate, __VA_ARGS__)

#define TADD_CS(...)       ADD_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSUB_CS(...)       SUB_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMUL_CS(...)       MUL_CS(MAJOR_TYPE, __VA_ARGS__)
#define TDIV_CS(...)       DIV_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMOD_CS(...)       MOD_CS(MAJOR_TYPE, __VA_ARGS__)
#define TNEG_CS(...)       NEG_CS(MAJOR_TYPE, __VA_ARGS__)

#define IADD_CS(...)       ADD_CS(INDEX_TYPE, __VA_ARGS__)
#define ISUB_CS(...)       SUB_CS(INDEX_TYPE, __VA_ARGS__)
#define IMUL_CS(...)       MUL_CS(INDEX_TYPE, __VA_ARGS__)
#define IDIV_CS(...)       DIV_CS(INDEX_TYPE, __VA_ARGS__)
#define IMOD_CS(...)       MOD_CS(INDEX_TYPE, __VA_ARGS__)
#define INEG_CS(...)       NEG_CS(INDEX_TYPE, __VA_ARGS__)

#define EQ(...)            CMP(std::equal_to, __VA_ARGS__)
#define NEQ(...)           CMP(std::not_equal_to, __VA_ARGS__)
#define GT(...)            CMP(std::greater, __VA_ARGS__)
#define LT(...)            CMP(std::less, __VA_ARGS__)
#define GE(...)            CMP(std::greater_equal, __VA_ARGS__)
#define LE(...)            CMP(std::less_equal, __VA_ARGS__)

#define EQ_CS(...)         CMP_CS(std::equal_to, __VA_ARGS__)
#define NEQ_CS(...)        CMP_CS(std::not_equal_to, __VA_ARGS__)
#define GT_CS(...)         CMP_CS(std::greater, __VA_ARGS__)
#define LT_CS(...)         CMP_CS(std::less, __VA_ARGS__)
#define GE_CS(...)         CMP_CS(std::greater_equal, __VA_ARGS__)
#define LE_CS(...)         CMP_CS(std::less_equal, __VA_ARGS__)

#define TEQ(...)           EQ(MAJOR_TYPE, __VA_ARGS__)
#define TNEQ(...)          NEQ(MAJOR_TYPE, __VA_ARGS__)
#define TGT(...)           GT(MAJOR_TYPE, __VA_ARGS__)
#define TLT(...)           LT(MAJOR_TYPE, __VA_ARGS__)
#define TGE(...)           GE(MAJOR_TYPE, __VA_ARGS__)
#define TLE(...)           LE(MAJOR_TYPE, __VA_ARGS__)

#define IEQ(...)           EQ(INDEX_TYPE, __VA_ARGS__)
#define INEQ(...)          NEQ(INDEX_TYPE, __VA_ARGS__)
#define IGT(...)           GT(INDEX_TYPE, __VA_ARGS__)
#define ILT(...)           LT(INDEX_TYPE, __VA_ARGS__)
#define IGE(...)           GE(INDEX_TYPE, __VA_ARGS__)
#define ILE(...)           LE(INDEX_TYPE, __VA_ARGS__)

#define TEQ_CS(...)        EQ_CS(MAJOR_TYPE, __VA_ARGS__)
#define TNEQ_CS(...)       NEQ_CS(MAJOR_TYPE, __VA_ARGS__)
#define TGT_CS(...)        GT_CS(MAJOR_TYPE, __VA_ARGS__)
#define TLT_CS(...)        LT_CS(MAJOR_TYPE, __VA_ARGS__)
#define TGE_CS(...)        GE_CS(MAJOR_TYPE, __VA_ARGS__)
#define TLE_CS(...)        LE_CS(MAJOR_TYPE, __VA_ARGS__)

#define IEQ_CS(...)        EQ_CS(INDEX_TYPE, __VA_ARGS__)
#define INEQ_CS(...)       NEQ_CS(INDEX_TYPE, __VA_ARGS__)
#define IGT_CS(...)        GT_CS(INDEX_TYPE, __VA_ARGS__)
#define ILT_CS(...)        LT_CS(INDEX_TYPE, __VA_ARGS__)
#define IGE_CS(...)        GE_CS(INDEX_TYPE, __VA_ARGS__)
#define ILE_CS(...)        LE_CS(INDEX_TYPE, __VA_ARGS__)

#define LAND(...)          BINOP(std::logical_and, INDEX_TYPE, __VA_ARGS__)
#define LOR(...)           BINOP(std::logical_or, INDEX_TYPE, __VA_ARGS__)
#define LNOT(...)          UNOP(std::logical_not, INDEX_TYPE, __VA_ARGS__)

#define LAND_CS(...)       BINOP_CS(std::logical_and, INDEX_TYPE, __VA_ARGS__)
#define LOR_CS(...)        BINOP_CS(std::logical_or, INDEX_TYPE, __VA_ARGS__)
#define LNOT_CS(...)       UNOP_CS(std::logical_not, INDEX_TYPE, __VA_ARGS__)

#define BAND(...)          BINOP(std::bit_and, INDEX_TYPE, __VA_ARGS__)
#define BOR(...)           BINOP(std::bit_or, INDEX_TYPE, __VA_ARGS__)
#define XOR(...)           BINOP(std::bit_xor, INDEX_TYPE, __VA_ARGS__)
#define BNOT(...)          UNOP(std::bit_not, INDEX_TYPE, __VA_ARGS__)

#define BAND_CS(...)       BINOP_CS(std::bit_and, INDEX_TYPE, __VA_ARGS__)
#define BOR_CS(...)        BINOP_CS(std::bit_or, INDEX_TYPE, __VA_ARGS__)
#define XOR_CS(...)        BINOP_CS(std::bit_xor, INDEX_TYPE, __VA_ARGS__)
#define BNOT_CS(...)       UNOP_CS(std::bit_not, INDEX_TYPE, __VA_ARGS__)

#define TCMOV(...)         CMOV(MAJOR_TYPE, __VA_ARGS__)
#define ICMOV(...)         CMOV(INDEX_TYPE, __VA_ARGS__)

#define TCMOV_CS(...)      CMOV_CS(MAJOR_TYPE, __VA_ARGS__)
#define ICMOV_CS(...)      CMOV_CS(INDEX_TYPE, __VA_ARGS__)

#define LSHFT(...)         BINOP(loom::vm::ops::lshift, INDEX_TYPE, __VA_ARGS__)
#define RSHFT(...)         BINOP(loom::vm::ops::rshift, INDEX_TYPE, __VA_ARGS__)

#define LSHFT_CS(...)      BINOP_CS(loom::vm::ops::lshift, INDEX_TYPE, __VA_ARGS__)
#define RSHFT_CS(...)      BINOP_CS(loom::vm::ops::rshift, INDEX_TYPE, __VA_ARGS__)

#define ABS(...)           UNOP(loom::vm::ops::abs, __VA_ARGS__)
#define ROUND(...)         UNOP(loom::vm::ops::round, __VA_ARGS__)
#define CEIL(...)          UNOP(loom::vm::ops::ceil, __VA_ARGS__)
#define FLOOR(...)         UNOP(loom::vm::ops::floor, __VA_ARGS__)
#define TRUNC(...)         UNOP(loom::vm::ops::trunc, __VA_ARGS__)
#define COPYSIGN(...)      UNOP(loom::vm::ops::copysign, __VA_ARGS__)
#define SIN(...)           UNOP(loom::vm::ops::sin, __VA_ARGS__)
#define COS(...)           UNOP(loom::vm::ops::cos, __VA_ARGS__)
#define MIN(...)           BINOP(loom::vm::ops::min, __VA_ARGS__)
#define MAX(...)           BINOP(loom::vm::ops::max, __VA_ARGS__)

#define TABS(...)          ABS(MAJOR_TYPE, __VA_ARGS__)
#define TROUND(...)        ROUND(MAJOR_TYPE, __VA_ARGS__)
#define TCEIL(...)         CEIL(MAJOR_TYPE, __VA_ARGS__)
#define TFLOOR(...)        FLOOR(MAJOR_TYPE, __VA_ARGS__)
#define TTRUNC(...)        TRUNC(MAJOR_TYPE, __VA_ARGS__)
#define TCOPYSIGN(...)     COPYSIGN(MAJOR_TYPE, __VA_ARGS__)
#define TSIN(...)          SIN(MAJOR_TYPE, __VA_ARGS__)
#define TCOS(...)          COS(MAJOR_TYPE, __VA_ARGS__)
#define TMIN(...)          MIN(MAJOR_TYPE, __VA_ARGS__)
#define TMAX(...)          MAX(MAJOR_TYPE, __VA_ARGS__)

#define ABS_CS(...)        UNOP_CS(loom::vm::ops::abs, __VA_ARGS__)
#define ROUND_CS(...)      UNOP_CS(loom::vm::ops::round, __VA_ARGS__)
#define CEIL_CS(...)       UNOP_CS(loom::vm::ops::ceil, __VA_ARGS__)
#define FLOOR_CS(...)      UNOP_CS(loom::vm::ops::floor, __VA_ARGS__)
#define TRUNC_CS(...)      UNOP_CS(loom::vm::ops::trunc, __VA_ARGS__)
#define COPYSIGN_CS(...)   UNOP_CS(loom::vm::ops::copysign, __VA_ARGS__)
#define SIN_CS(...)        UNOP_CS(loom::vm::ops::sin, __VA_ARGS__)
#define COS_CS(...)        UNOP_CS(loom::vm::ops::cos, __VA_ARGS__)
#define MIN_CS(...)        BINOP_CS(loom::vm::ops::min, __VA_ARGS__)
#define MAX_CS(...)        BINOP_CS(loom::vm::ops::max, __VA_ARGS__)

#define TABS_CS(...)       ABS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TROUND_CS(...)     ROUND_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCEIL_CS(...)      CEIL_CS(MAJOR_TYPE, __VA_ARGS__)
#define TFLOOR_CS(...)     FLOOR_CS(MAJOR_TYPE, __VA_ARGS__)
#define TTRUNC_CS(...)     TRUNC_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCOPYSIGN_CS(...)  COPYSIGN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSIN_CS(...)       SIN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCOS_CS(...)       COS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMIN_CS(...)       MIN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMAX_CS(...)       MAX_CS(MAJOR_TYPE, __VA_ARGS__)
