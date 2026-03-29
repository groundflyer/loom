// -*- C++ -*-
/// @file macros.hpp
/// @brief C macros for programming VM asm.

#pragma once

#ifndef MAJOR_TYPE
#define MAJOR_TYPE T
#endif

#define INDEX_TYPE         loom::uint_of_size_t<sizeof(MAJOR_TYPE)>

// convert to char_sequence
#define CS(S)              loom::sl2cs<#S>

#define CS_JOIN(A, B)      loom::intlist::join<A, B>

// NAME - char_sequence
#define VAR_CS(TYPE, NAME) loom::vm::Variable<TYPE, NAME>
// NAME - literal without quotes
#define VAR(TYPE, NAME)    VAR_CS(TYPE, CS(NAME))
#define TVAR(NAME)         VAR(MAJOR_TYPE, NAME)
#define IVAR(NAME)         VAR(INDEX_TYPE, NAME)
#define TVAR_CS(NAME)      VAR_CS(MAJOR_TYPE, NAME)
#define IVAR_CS(NAME)      VAR_CS(INDEX_TYPE, NAME)

#define KERNEL(NAME, ...)                           \
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
#define TKERNEL7(NAME, p1, p2, p3, p4, p5, p6, p7) \
    KERNEL(NAME,                                   \
           TVAR(p1),                               \
           TVAR(p2),                               \
           TVAR(p3),                               \
           TVAR(p4),                               \
           TVAR(p5),                               \
           TVAR(p6),                               \
           TVAR(p7))
#define TKERNEL8(NAME, p1, p2, p3, p4, p5, p6, p7, p8) \
    KERNEL(NAME,                                       \
           TVAR(p1),                                   \
           TVAR(p2),                                   \
           TVAR(p3),                                   \
           TVAR(p4),                                   \
           TVAR(p5),                                   \
           TVAR(p6),                                   \
           TVAR(p7),                                   \
           TVAR(p8))
#define TKERNEL9(NAME, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
    KERNEL(NAME,                                           \
           TVAR(p1),                                       \
           TVAR(p2),                                       \
           TVAR(p3),                                       \
           TVAR(p4),                                       \
           TVAR(p5),                                       \
           TVAR(p6),                                       \
           TVAR(p7),                                       \
           TVAR(p8),                                       \
           TVAR(p9))
#define TKERNEL10(NAME, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) \
    KERNEL(NAME,                                                 \
           TVAR(p1),                                             \
           TVAR(p2),                                             \
           TVAR(p3),                                             \
           TVAR(p4),                                             \
           TVAR(p5),                                             \
           TVAR(p6),                                             \
           TVAR(p7),                                             \
           TVAR(p8),                                             \
           TVAR(p9),                                             \
           TVAR(p10))
#define TKERNEL11(NAME, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) \
    KERNEL(NAME,                                                      \
           TVAR(p1),                                                  \
           TVAR(p2),                                                  \
           TVAR(p3),                                                  \
           TVAR(p4),                                                  \
           TVAR(p5),                                                  \
           TVAR(p6),                                                  \
           TVAR(p7),                                                  \
           TVAR(p8),                                                  \
           TVAR(p9),                                                  \
           TVAR(p10),                                                 \
           TVAR(p11))
#define TKERNEL12(NAME, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12) \
    KERNEL(NAME,                                                           \
           TVAR(p1),                                                       \
           TVAR(p2),                                                       \
           TVAR(p3),                                                       \
           TVAR(p4),                                                       \
           TVAR(p5),                                                       \
           TVAR(p6),                                                       \
           TVAR(p7),                                                       \
           TVAR(p8),                                                       \
           TVAR(p9),                                                       \
           TVAR(p10),                                                      \
           TVAR(p11),                                                      \
           TVAR(p12))
#define TKERNEL13(NAME, \
                  p1,   \
                  p2,   \
                  p3,   \
                  p4,   \
                  p5,   \
                  p6,   \
                  p7,   \
                  p8,   \
                  p9,   \
                  p10,  \
                  p11,  \
                  p12,  \
                  p13)  \
    KERNEL(NAME,        \
           TVAR(p1),    \
           TVAR(p2),    \
           TVAR(p3),    \
           TVAR(p4),    \
           TVAR(p5),    \
           TVAR(p6),    \
           TVAR(p7),    \
           TVAR(p8),    \
           TVAR(p9),    \
           TVAR(p10),   \
           TVAR(p11),   \
           TVAR(p12),   \
           TVAR(p13))
#define TKERNEL14(NAME, \
                  p1,   \
                  p2,   \
                  p3,   \
                  p4,   \
                  p5,   \
                  p6,   \
                  p7,   \
                  p8,   \
                  p9,   \
                  p10,  \
                  p11,  \
                  p12,  \
                  p13,  \
                  p14)  \
    KERNEL(NAME,        \
           TVAR(p1),    \
           TVAR(p2),    \
           TVAR(p3),    \
           TVAR(p4),    \
           TVAR(p5),    \
           TVAR(p6),    \
           TVAR(p7),    \
           TVAR(p8),    \
           TVAR(p9),    \
           TVAR(p10),   \
           TVAR(p11),   \
           TVAR(p12),   \
           TVAR(p13),   \
           TVAR(p14))
#define TKERNEL15(NAME, \
                  p1,   \
                  p2,   \
                  p3,   \
                  p4,   \
                  p5,   \
                  p6,   \
                  p7,   \
                  p8,   \
                  p9,   \
                  p10,  \
                  p11,  \
                  p12,  \
                  p13,  \
                  p14,  \
                  p15)  \
    KERNEL(NAME,        \
           TVAR(p1),    \
           TVAR(p2),    \
           TVAR(p3),    \
           TVAR(p4),    \
           TVAR(p5),    \
           TVAR(p6),    \
           TVAR(p7),    \
           TVAR(p8),    \
           TVAR(p9),    \
           TVAR(p10),   \
           TVAR(p11),   \
           TVAR(p12),   \
           TVAR(p13),   \
           TVAR(p14),   \
           TVAR(p15))
#define TKERNEL16(NAME, \
                  p1,   \
                  p2,   \
                  p3,   \
                  p4,   \
                  p5,   \
                  p6,   \
                  p7,   \
                  p8,   \
                  p9,   \
                  p10,  \
                  p11,  \
                  p12,  \
                  p13,  \
                  p14,  \
                  p15,  \
                  p16)  \
    KERNEL(NAME,        \
           TVAR(p1),    \
           TVAR(p2),    \
           TVAR(p3),    \
           TVAR(p4),    \
           TVAR(p5),    \
           TVAR(p6),    \
           TVAR(p7),    \
           TVAR(p8),    \
           TVAR(p9),    \
           TVAR(p10),   \
           TVAR(p11),   \
           TVAR(p12),   \
           TVAR(p13),   \
           TVAR(p14),   \
           TVAR(p15),   \
           TVAR(p16))
#define TKERNEL17(NAME, \
                  p1,   \
                  p2,   \
                  p3,   \
                  p4,   \
                  p5,   \
                  p6,   \
                  p7,   \
                  p8,   \
                  p9,   \
                  p10,  \
                  p11,  \
                  p12,  \
                  p13,  \
                  p14,  \
                  p15,  \
                  p16,  \
                  p17)  \
    KERNEL(NAME,        \
           TVAR(p1),    \
           TVAR(p2),    \
           TVAR(p3),    \
           TVAR(p4),    \
           TVAR(p5),    \
           TVAR(p6),    \
           TVAR(p7),    \
           TVAR(p8),    \
           TVAR(p9),    \
           TVAR(p10),   \
           TVAR(p11),   \
           TVAR(p12),   \
           TVAR(p13),   \
           TVAR(p14),   \
           TVAR(p15),   \
           TVAR(p16),   \
           TVAR(p17))
#define TKERNEL18(NAME, \
                  p1,   \
                  p2,   \
                  p3,   \
                  p4,   \
                  p5,   \
                  p6,   \
                  p7,   \
                  p8,   \
                  p9,   \
                  p10,  \
                  p11,  \
                  p12,  \
                  p13,  \
                  p14,  \
                  p15,  \
                  p16,  \
                  p17,  \
                  p18)  \
    KERNEL(NAME,        \
           TVAR(p1),    \
           TVAR(p2),    \
           TVAR(p3),    \
           TVAR(p4),    \
           TVAR(p5),    \
           TVAR(p6),    \
           TVAR(p7),    \
           TVAR(p8),    \
           TVAR(p9),    \
           TVAR(p10),   \
           TVAR(p11),   \
           TVAR(p12),   \
           TVAR(p13),   \
           TVAR(p14),   \
           TVAR(p15),   \
           TVAR(p16),   \
           TVAR(p17),   \
           TVAR(p18))

#define TKERNEL_END1(v) \
    KERNEL_END          \
    (TVAR(v))
#define TKERNEL_END2(a, b) \
    KERNEL_END             \
    (TVAR(a), TVAR(b))
#define TKERNEL_END3(a, b, c) \
    KERNEL_END                \
    (TVAR(a), TVAR(b), TVAR(c))
#define TKERNEL_END4(o1, o2, o3, o4) \
    KERNEL_END                       \
    (TVAR(o1), TVAR(o2), TVAR(o3), TVAR(o4))
#define TKERNEL_END5(o1, o2, o3, o4, o5) \
    KERNEL_END                           \
    (TVAR(o1), TVAR(o2), TVAR(o3), TVAR(o4), TVAR(o5))
#define TKERNEL_END6(o1, o2, o3, o4, o5, o6) \
    KERNEL_END                               \
    (TVAR(o1), TVAR(o2), TVAR(o3), TVAR(o4), TVAR(o5), TVAR(o6))
#define IKERNEL1(NAME, v)    KERNEL(NAME, IVAR(v))
#define IKERNEL2(NAME, a, b) KERNEL(NAME, IVAR(a), IVAR(b))
#define IKERNEL_END1(v) \
    KERNEL_END          \
    (IVAR(v))
#define IKERNEL_END2(a, b) \
    KERNEL_END             \
    (IVAR(a), IVAR(b))

#define MACRO_DECL_PREFIX __macro_decl_

#define __MACRO_DECLARE_PREFIX(PREFIX, MACRO_NAME, ...) \
    using PREFIX##MACRO_NAME =                          \
        loom::vm::macro_decl<CS(MACRO_NAME) __VA_OPT__(, ) __VA_ARGS__>
#define __MACRO_DECLARE(NAME, ...) \
    __MACRO_DECLARE_PREFIX(MACRO_DECL_PREFIX, NAME, __VA_ARGS__)
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
#define MACRO8(NAME, P1, P2, P3, P4, P5, P6, P7, P8) \
    __MACRO_DECLARE(NAME,                            \
                    CS(P1),                          \
                    CS(P2),                          \
                    CS(P3),                          \
                    CS(P4),                          \
                    CS(P5),                          \
                    CS(P6),                          \
                    CS(P7),                          \
                    CS(P8));                         \
    template <typename P1,                           \
              typename P2,                           \
              typename P3,                           \
              typename P4,                           \
              typename P5,                           \
              typename P6,                           \
              typename P7,                           \
              typename P8>                           \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO9(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9) \
    __MACRO_DECLARE(NAME,                                \
                    CS(P1),                              \
                    CS(P2),                              \
                    CS(P3),                              \
                    CS(P4),                              \
                    CS(P5),                              \
                    CS(P6),                              \
                    CS(P7),                              \
                    CS(P8),                              \
                    CS(P9));                             \
    template <typename P1,                               \
              typename P2,                               \
              typename P3,                               \
              typename P4,                               \
              typename P5,                               \
              typename P6,                               \
              typename P7,                               \
              typename P8,                               \
              typename P9>                               \
    __MACRO_DEF_BEGIN(NAME)
#define MACRO10(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10) \
    __MACRO_DECLARE(NAME,                                      \
                    CS(P1),                                    \
                    CS(P2),                                    \
                    CS(P3),                                    \
                    CS(P4),                                    \
                    CS(P5),                                    \
                    CS(P6),                                    \
                    CS(P7),                                    \
                    CS(P8),                                    \
                    CS(P9),                                    \
                    CS(P10));                                  \
    template <typename P1,                                     \
              typename P2,                                     \
              typename P3,                                     \
              typename P4,                                     \
              typename P5,                                     \
              typename P6,                                     \
              typename P7,                                     \
              typename P8,                                     \
              typename P9,                                     \
              typename P10>                                    \
    __MACRO_DEF_BEGIN(NAME)

#define TMACRO(NAME)               \
    __MACRO_DECLARE(NAME);         \
    template <typename MAJOR_TYPE> \
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
#define TMACRO8(NAME, P1, P2, P3, P4, P5, P6, P7, P8) \
    __MACRO_DECLARE(NAME,                             \
                    CS(P1),                           \
                    CS(P2),                           \
                    CS(P3),                           \
                    CS(P4),                           \
                    CS(P5),                           \
                    CS(P6),                           \
                    CS(P7),                           \
                    CS(P8));                          \
    template <typename MAJOR_TYPE,                    \
              typename P1,                            \
              typename P2,                            \
              typename P3,                            \
              typename P4,                            \
              typename P5,                            \
              typename P6,                            \
              typename P7,                            \
              typename P8>                            \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO9(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9) \
    __MACRO_DECLARE(NAME,                                 \
                    CS(P1),                               \
                    CS(P2),                               \
                    CS(P3),                               \
                    CS(P4),                               \
                    CS(P5),                               \
                    CS(P6),                               \
                    CS(P7),                               \
                    CS(P8),                               \
                    CS(P9));                              \
    template <typename MAJOR_TYPE,                        \
              typename P1,                                \
              typename P2,                                \
              typename P3,                                \
              typename P4,                                \
              typename P5,                                \
              typename P6,                                \
              typename P7,                                \
              typename P8,                                \
              typename P9>                                \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO10(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10) \
    __MACRO_DECLARE(NAME,                                       \
                    CS(P1),                                     \
                    CS(P2),                                     \
                    CS(P3),                                     \
                    CS(P4),                                     \
                    CS(P5),                                     \
                    CS(P6),                                     \
                    CS(P7),                                     \
                    CS(P8),                                     \
                    CS(P9),                                     \
                    CS(P10));                                   \
    template <typename MAJOR_TYPE,                              \
              typename P1,                                      \
              typename P2,                                      \
              typename P3,                                      \
              typename P4,                                      \
              typename P5,                                      \
              typename P6,                                      \
              typename P7,                                      \
              typename P8,                                      \
              typename P9,                                      \
              typename P10>                                     \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO11(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11) \
    __MACRO_DECLARE(NAME,                                            \
                    CS(P1),                                          \
                    CS(P2),                                          \
                    CS(P3),                                          \
                    CS(P4),                                          \
                    CS(P5),                                          \
                    CS(P6),                                          \
                    CS(P7),                                          \
                    CS(P8),                                          \
                    CS(P9),                                          \
                    CS(P10),                                         \
                    CS(P11));                                        \
    template <typename MAJOR_TYPE,                                   \
              typename P1,                                           \
              typename P2,                                           \
              typename P3,                                           \
              typename P4,                                           \
              typename P5,                                           \
              typename P6,                                           \
              typename P7,                                           \
              typename P8,                                           \
              typename P9,                                           \
              typename P10,                                          \
              typename P11>                                          \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO12(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12) \
    __MACRO_DECLARE(NAME,                                                 \
                    CS(P1),                                               \
                    CS(P2),                                               \
                    CS(P3),                                               \
                    CS(P4),                                               \
                    CS(P5),                                               \
                    CS(P6),                                               \
                    CS(P7),                                               \
                    CS(P8),                                               \
                    CS(P9),                                               \
                    CS(P10),                                              \
                    CS(P11),                                              \
                    CS(P12));                                             \
    template <typename MAJOR_TYPE,                                        \
              typename P1,                                                \
              typename P2,                                                \
              typename P3,                                                \
              typename P4,                                                \
              typename P5,                                                \
              typename P6,                                                \
              typename P7,                                                \
              typename P8,                                                \
              typename P9,                                                \
              typename P10,                                               \
              typename P11,                                               \
              typename P12>                                               \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO13(NAME, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13) \
    __MACRO_DECLARE(NAME,                                                      \
                    CS(P1),                                                    \
                    CS(P2),                                                    \
                    CS(P3),                                                    \
                    CS(P4),                                                    \
                    CS(P5),                                                    \
                    CS(P6),                                                    \
                    CS(P7),                                                    \
                    CS(P8),                                                    \
                    CS(P9),                                                    \
                    CS(P10),                                                   \
                    CS(P11),                                                   \
                    CS(P12),                                                   \
                    CS(P13));                                                  \
    template <typename MAJOR_TYPE,                                             \
              typename P1,                                                     \
              typename P2,                                                     \
              typename P3,                                                     \
              typename P4,                                                     \
              typename P5,                                                     \
              typename P6,                                                     \
              typename P7,                                                     \
              typename P8,                                                     \
              typename P9,                                                     \
              typename P10,                                                    \
              typename P11,                                                    \
              typename P12,                                                    \
              typename P13>                                                    \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO14(NAME,             \
                 P1,               \
                 P2,               \
                 P3,               \
                 P4,               \
                 P5,               \
                 P6,               \
                 P7,               \
                 P8,               \
                 P9,               \
                 P10,              \
                 P11,              \
                 P12,              \
                 P13,              \
                 P14)              \
    __MACRO_DECLARE(NAME,          \
                    CS(P1),        \
                    CS(P2),        \
                    CS(P3),        \
                    CS(P4),        \
                    CS(P5),        \
                    CS(P6),        \
                    CS(P7),        \
                    CS(P8),        \
                    CS(P9),        \
                    CS(P10),       \
                    CS(P11),       \
                    CS(P12),       \
                    CS(P13),       \
                    CS(P14));      \
    template <typename MAJOR_TYPE, \
              typename P1,         \
              typename P2,         \
              typename P3,         \
              typename P4,         \
              typename P5,         \
              typename P6,         \
              typename P7,         \
              typename P8,         \
              typename P9,         \
              typename P10,        \
              typename P11,        \
              typename P12,        \
              typename P13,        \
              typename P14>        \
    __MACRO_DEF_BEGIN(NAME)
#define TMACRO15(NAME,             \
                 P1,               \
                 P2,               \
                 P3,               \
                 P4,               \
                 P5,               \
                 P6,               \
                 P7,               \
                 P8,               \
                 P9,               \
                 P10,              \
                 P11,              \
                 P12,              \
                 P13,              \
                 P14,              \
                 P15)              \
    __MACRO_DECLARE(NAME,          \
                    CS(P1),        \
                    CS(P2),        \
                    CS(P3),        \
                    CS(P4),        \
                    CS(P5),        \
                    CS(P6),        \
                    CS(P7),        \
                    CS(P8),        \
                    CS(P9),        \
                    CS(P10),       \
                    CS(P11),       \
                    CS(P12),       \
                    CS(P13),       \
                    CS(P14),       \
                    CS(P15));      \
    template <typename MAJOR_TYPE, \
              typename P1,         \
              typename P2,         \
              typename P3,         \
              typename P4,         \
              typename P5,         \
              typename P6,         \
              typename P7,         \
              typename P8,         \
              typename P9,         \
              typename P10,        \
              typename P11,        \
              typename P12,        \
              typename P13,        \
              typename P14,        \
              typename P15>        \
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
#define TMOV_CS(a, b)           MOV(TVAR_CS(a), TVAR_CS(b))
#define IMOV_CS(a, b)           MOV(IVAR_CS(a), IVAR_CS(b))

#define LABEL_CS(NAME)          loom::vm::Label<NAME>
#define LABEL(NAME)             LABEL_CS(CS(NAME))
#define PREFIX_LABEL_CS(_P, _L) LABEL_CS(CS_JOIN(_P, _L))

#define JMP(TARGET)                \
    loom::vm::Jump < LABEL(TARGET) \
    >

#define PJMPN(JVAR, ...) \
    loom::vm::ParmJump<JVAR, loom::type_sequence<__VA_ARGS__>>

#define PJMP2(JVAR, L1, L2)                                    \
    loom::vm::ParmJump < JVAR, loom::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                \
    >>
#define PJMP3(JVAR, L1, L2, L3)                                \
    loom::vm::ParmJump < JVAR, loom::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                \
    , LABEL(L3)                                                \
    >>
#define PJMP4(JVAR, L1, L2, L3, L4)                            \
    loom::vm::ParmJump < JVAR, loom::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                \
    , LABEL(L3)                                                \
    , LABEL(L4)                                                \
    >>
#define PJMP5(JVAR, L1, L2, L3, L4, L5)                        \
    loom::vm::ParmJump < JVAR, loom::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                \
    , LABEL(L3)                                                \
    , LABEL(L4)                                                \
    , LABEL(L5)                                                \
    >>
#define PJMP6(JVAR, L1, L2, L3, L4, L5, L6)                    \
    loom::vm::ParmJump < JVAR, loom::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                \
    , LABEL(L3)                                                \
    , LABEL(L4)                                                \
    , LABEL(L5)                                                \
    , LABEL(L6)                                                \
    >>

#define PJMP2_CS(JTYPE, JNAME, L1, L2) \
    PJMP2(VAR_CS(JTYPE, JNAME), LABEL(L1), LABEL(L2))
#define PJMP3_CS(JTYPE, JNAME, L1, L2, L3) \
    PJMP3(VAR_CS(JTYPE, JNAME), LABEL(L1), LABEL(L2), LABEL(L3))
#define PJMP4_CS(JTYPE, JNAME, L1, L2, L3, L4) \
    PJMP4(VAR_CS(JTYPE, JNAME), LABEL(L1), LABEL(L2), LABEL(L3), LABEL(L4))
#define PJMP5_CS(JTYPE, JNAME, L1, L2, L3, L4, L5) \
    PJMP4(VAR_CS(JTYPE, JNAME),                    \
          LABEL(L1),                               \
          LABEL(L2),                               \
          LABEL(L3),                               \
          LABEL(L4),                               \
          LABEL(L5))
#define PJMP6_CS(JTYPE, JNAME, L1, L2, L3, L4, L5, L6) \
    PJMP4(VAR_CS(JTYPE, JNAME),                        \
          LABEL(L1),                                   \
          LABEL(L2),                                   \
          LABEL(L3),                                   \
          LABEL(L4),                                   \
          LABEL(L5),                                   \
          LABEL(L6))

#define IPJMPN(JNAME, ...)    PJMPN(IVAR(JNAME), __VA_ARGS__)
#define IPJMP2(JNAME, ...)    PJMP2(IVAR(JNAME), __VA_ARGS__)
#define IPJMP3(JNAME, ...)    PJMP3(IVAR(JNAME), __VA_ARGS__)
#define IPJMP4(JNAME, ...)    PJMP4(IVAR(JNAME), __VA_ARGS__)
#define IPJMP5(JNAME, ...)    PJMP5(IVAR(JNAME), __VA_ARGS__)
#define IPJMP6(JNAME, ...)    PJMP6(IVAR(JNAME), __VA_ARGS__)

#define IPJMPN_CS(JNAME, ...) PJMP(IVAR_CS(JNAME), __VA_ARGS__)
#define IPJMP2_CS(JNAME, ...) PJMP2(IVAR_CS(JNAME), __VA_ARGS__)
#define IPJMP3_CS(JNAME, ...) PJMP3(IVAR_CS(JNAME), __VA_ARGS__)
#define IPJMP4_CS(JNAME, ...) PJMP4(IVAR_CS(JNAME), __VA_ARGS__)
#define IPJMP5_CS(JNAME, ...) PJMP5(IVAR_CS(JNAME), __VA_ARGS__)
#define IPJMP6_CS(JNAME, ...) PJMP6(IVAR_CS(JNAME), __VA_ARGS__)

#define BINOP_CS(OP, TYPE, DST, A, B) \
    loom::vm::                        \
        Compute<OP<TYPE>, VAR_CS(TYPE, DST), VAR_CS(TYPE, A), VAR_CS(TYPE, B)>
#define BINOP(OP, TYPE, DST, A, B) BINOP_CS(OP, TYPE, CS(DST), CS(A), CS(B))

#define UNOP_CS(OP, TYPE, DST, V) \
    loom::vm::Compute<OP<TYPE>, VAR_CS(TYPE, DST), VAR_CS(TYPE, V)>
#define UNOP(OP, TYPE, DST, V) UNOP_CS(OP, TYPE, CS(DST), CS(V))

#define CMP_CS(OP, DST_TYPE, OP_TYPE, DST, A, B) \
    loom::vm::Compute<OP<OP_TYPE>,               \
                      VAR_CS(DST_TYPE, DST),     \
                      VAR_CS(OP_TYPE, A),        \
                      VAR_CS(OP_TYPE, B)>
#define CMP(OP, DST_TYPE, OP_TYPE, DST, A, B) \
    CMP_CS(OP, DST_TYPE, OP_TYPE, CS(DST), CS(A), CS(B))

#define CMOV_CS(TYPE, CTYPE, DST, C, A, B) \
    loom::vm::Compute<loom::vm::ops::cmov, \
                      VAR_CS(TYPE, DST),   \
                      VAR_CS(CTYPE, C),    \
                      VAR_CS(TYPE, A),     \
                      VAR_CS(TYPE, B)>
#define CMOV(TYPE, CTYPE, DST, C, A, B) \
    CMOV_CS(TYPE, CTYPE, CS(DST), CS(C), CS(A), CS(B))

#define FMOP_CS(OP, TYPE, DST, X, Y, Z)  \
    loom::vm::Compute<OP,                \
                      VAR_CS(TYPE, DST), \
                      VAR_CS(TYPE, X),   \
                      VAR_CS(TYPE, Y),   \
                      VAR_CS(TYPE, Z)>
#define FMA_CS(...)        FMOP_CS(loom::vm::ops::fma, __VA_ARGS__)
#define FMS_CS(...)        FMOP_CS(loom::vm::ops::fms, __VA_ARGS__)
#define TFMA_CS(...)       FMA_CS(MAJOR_TYPE, __VA_ARGS__)
#define TFMA(DST, X, Y, Z) TFMA_CS(CS(DST), CS(X), CS(Y), CS(Z))
#define TFMS_CS(...)       FMS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TFMS(DST, X, Y, Z) TFMS_CS(CS(DST), CS(X), CS(Y), CS(Z))

#define SCTN(DST_TYPE, IDX_TYPE, DST, IDX, ...)                             \
    loom::vm::Compute<loom::vm::ops::sct<DST_TYPE,                          \
                                         IDX_TYPE,                          \
                                         loom::type_sequence<__VA_ARGS__>>, \
                      VAR(DST_TYPE, DST),                                   \
                      VAR(IDX_TYPE, IDX)>

#define TSCTN(...)                  SCTN(MAJOR_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ISCTN(...)                  SCTN(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define TSCT3(DST, IDX, C0, C1, C2) TSCTN(DST, IDX, CS(C0), CS(C1), CS(C2))
#define TSCT4(DST, IDX, C0, C1, C2, C3) \
    TSCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3))
#define TSCT5(DST, IDX, C0, C1, C2, C3, C4) \
    TSCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3), CS(C4))
#define TSCT6(DST, IDX, C0, C1, C2, C3, C4, C5) \
    TSCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3), CS(C4), CS(C5))
#define TSCT7(DST, IDX, C0, C1, C2, C3, C4, C5, C6) \
    TSCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3), CS(C4), CS(C5), CS(C6))
#define TSCT8(DST, IDX, C0, C1, C2, C3, C4, C5, C6, C7) \
    TSCTN(DST,                                          \
          IDX,                                          \
          CS(C0),                                       \
          CS(C1),                                       \
          CS(C2),                                       \
          CS(C3),                                       \
          CS(C4),                                       \
          CS(C5),                                       \
          CS(C6),                                       \
          CS(C7))
#define TSCT9(DST, IDX, C0, C1, C2, C3, C4, C5, C6, C7, C8) \
    TSCTN(DST,                                              \
          IDX,                                              \
          CS(C0),                                           \
          CS(C1),                                           \
          CS(C2),                                           \
          CS(C3),                                           \
          CS(C4),                                           \
          CS(C5),                                           \
          CS(C6),                                           \
          CS(C7),                                           \
          CS(C8))
#define TSCT10(DST, IDX, C0, C1, C2, C3, C4, C5, C6, C7, C8, C9) \
    TSCTN(DST,                                                   \
          IDX,                                                   \
          CS(C0),                                                \
          CS(C1),                                                \
          CS(C2),                                                \
          CS(C3),                                                \
          CS(C4),                                                \
          CS(C5),                                                \
          CS(C6),                                                \
          CS(C7),                                                \
          CS(C8),                                                \
          CS(C9))
#define TSCT11(DST, IDX, C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10) \
    TSCTN(DST,                                                        \
          IDX,                                                        \
          CS(C0),                                                     \
          CS(C1),                                                     \
          CS(C2),                                                     \
          CS(C3),                                                     \
          CS(C4),                                                     \
          CS(C5),                                                     \
          CS(C6),                                                     \
          CS(C7),                                                     \
          CS(C8),                                                     \
          CS(C9),                                                     \
          CS(C10))
#define TSCT12(DST, IDX, C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11) \
    TSCTN(DST,                                                             \
          IDX,                                                             \
          CS(C0),                                                          \
          CS(C1),                                                          \
          CS(C2),                                                          \
          CS(C3),                                                          \
          CS(C4),                                                          \
          CS(C5),                                                          \
          CS(C6),                                                          \
          CS(C7),                                                          \
          CS(C8),                                                          \
          CS(C9),                                                          \
          CS(C10),                                                         \
          CS(C11))
#define TSCT13(DST, \
               IDX, \
               C0,  \
               C1,  \
               C2,  \
               C3,  \
               C4,  \
               C5,  \
               C6,  \
               C7,  \
               C8,  \
               C9,  \
               C10, \
               C11, \
               C12) \
    TSCTN(DST,      \
          IDX,      \
          CS(C0),   \
          CS(C1),   \
          CS(C2),   \
          CS(C3),   \
          CS(C4),   \
          CS(C5),   \
          CS(C6),   \
          CS(C7),   \
          CS(C8),   \
          CS(C9),   \
          CS(C10),  \
          CS(C11),  \
          CS(C12))
#define ISCT3(DST, IDX, C0, C1, C2) ISCTN(DST, IDX, CS(C0), CS(C1), CS(C2))
#define ISCT4(DST, IDX, C0, C1, C2, C3) \
    ISCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3))
#define ISCT5(DST, IDX, C0, C1, C2, C3, C4) \
    ISCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3), CS(C4))
#define ISCT6(DST, IDX, C0, C1, C2, C3, C4, C5) \
    ISCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3), CS(C4), CS(C5))
#define ISCT7(DST, IDX, C0, C1, C2, C3, C4, C5, C6) \
    ISCTN(DST, IDX, CS(C0), CS(C1), CS(C2), CS(C3), CS(C4), CS(C5), CS(C6))
#define ISCT8(DST, IDX, C0, C1, C2, C3, C4, C5, C6, C7) \
    ISCTN(DST,                                          \
          IDX,                                          \
          CS(C0),                                       \
          CS(C1),                                       \
          CS(C2),                                       \
          CS(C3),                                       \
          CS(C4),                                       \
          CS(C5),                                       \
          CS(C6),                                       \
          CS(C7))

#define SCTN_CS(DST_TYPE, IDX_TYPE, DST, IDX, ...)                          \
    loom::vm::Compute<loom::vm::ops::sct<DST_TYPE,                          \
                                         IDX_TYPE,                          \
                                         loom::type_sequence<__VA_ARGS__>>, \
                      VAR_CS(DST_TYPE, DST),                                \
                      VAR_CS(IDX_TYPE, IDX)>
#define TSCTN_CS(...) SCTN_CS(MAJOR_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ISCTN_CS(...) SCTN_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define PACC(ACC_OP, CMP_OP, PTYPE, CTYPE, DST, P, X, Y)                 \
    loom::vm::Compute<loom::vm::ops::pacc<CTYPE, PTYPE, ACC_OP, CMP_OP>, \
                      VAR(PTYPE, DST),                                   \
                      VAR(PTYPE, P),                                     \
                      VAR(CTYPE, X),                                     \
                      VAR(CTYPE, Y)>
#define PACC_CS(ACC_OP, CMP_OP, PTYPE, CTYPE, DST, P, X, Y)              \
    loom::vm::Compute<loom::vm::ops::pacc<CTYPE, PTYPE, ACC_OP, CMP_OP>, \
                      VAR_CS(PTYPE, DST),                                \
                      VAR_CS(PTYPE, P),                                  \
                      VAR_CS(CTYPE, X),                                  \
                      VAR_CS(CTYPE, Y)>

#define ADD(...)          BINOP(std::plus, __VA_ARGS__)
#define SUB(...)          BINOP(std::minus, __VA_ARGS__)
#define MUL(...)          BINOP(std::multiplies, __VA_ARGS__)
#define DIV(...)          BINOP(std::divides, __VA_ARGS__)
#define MOD(...)          BINOP(std::modulus, __VA_ARGS__)
#define NEG(...)          UNOP(std::negate, __VA_ARGS__)

#define TADD(...)         ADD(MAJOR_TYPE, __VA_ARGS__)
#define TSUB(...)         SUB(MAJOR_TYPE, __VA_ARGS__)
#define TMUL(...)         MUL(MAJOR_TYPE, __VA_ARGS__)
#define TDIV(...)         DIV(MAJOR_TYPE, __VA_ARGS__)
#define TMOD(...)         MOD(MAJOR_TYPE, __VA_ARGS__)
#define TNEG(...)         NEG(MAJOR_TYPE, __VA_ARGS__)

#define IADD(...)         ADD(INDEX_TYPE, __VA_ARGS__)
#define ISUB(...)         SUB(INDEX_TYPE, __VA_ARGS__)
#define IMUL(...)         MUL(INDEX_TYPE, __VA_ARGS__)
#define IDIV(...)         DIV(INDEX_TYPE, __VA_ARGS__)
#define IMOD(...)         MOD(INDEX_TYPE, __VA_ARGS__)
#define INEG(...)         NEG(INDEX_TYPE, __VA_ARGS__)

#define ADD_CS(...)       BINOP_CS(std::plus, __VA_ARGS__)
#define SUB_CS(...)       BINOP_CS(std::minus, __VA_ARGS__)
#define MUL_CS(...)       BINOP_CS(std::multiplies, __VA_ARGS__)
#define DIV_CS(...)       BINOP_CS(std::divides, __VA_ARGS__)
#define MOD_CS(...)       BINOP_CS(std::modulus, __VA_ARGS__)
#define NEG_CS(...)       UNOP_CS(std::negate, __VA_ARGS__)

#define TADD_CS(...)      ADD_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSUB_CS(...)      SUB_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMUL_CS(...)      MUL_CS(MAJOR_TYPE, __VA_ARGS__)
#define TDIV_CS(...)      DIV_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMOD_CS(...)      MOD_CS(MAJOR_TYPE, __VA_ARGS__)
#define TNEG_CS(...)      NEG_CS(MAJOR_TYPE, __VA_ARGS__)

#define IADD_CS(...)      ADD_CS(INDEX_TYPE, __VA_ARGS__)
#define ISUB_CS(...)      SUB_CS(INDEX_TYPE, __VA_ARGS__)
#define IMUL_CS(...)      MUL_CS(INDEX_TYPE, __VA_ARGS__)
#define IDIV_CS(...)      DIV_CS(INDEX_TYPE, __VA_ARGS__)
#define IMOD_CS(...)      MOD_CS(INDEX_TYPE, __VA_ARGS__)
#define INEG_CS(...)      NEG_CS(INDEX_TYPE, __VA_ARGS__)

#define EQ(...)           CMP(std::equal_to, __VA_ARGS__)
#define NEQ(...)          CMP(std::not_equal_to, __VA_ARGS__)
#define GT(...)           CMP(std::greater, __VA_ARGS__)
#define LT(...)           CMP(std::less, __VA_ARGS__)
#define GE(...)           CMP(std::greater_equal, __VA_ARGS__)
#define LE(...)           CMP(std::less_equal, __VA_ARGS__)

#define EQ_CS(...)        CMP_CS(std::equal_to, __VA_ARGS__)
#define NEQ_CS(...)       CMP_CS(std::not_equal_to, __VA_ARGS__)
#define GT_CS(...)        CMP_CS(std::greater, __VA_ARGS__)
#define LT_CS(...)        CMP_CS(std::less, __VA_ARGS__)
#define GE_CS(...)        CMP_CS(std::greater_equal, __VA_ARGS__)
#define LE_CS(...)        CMP_CS(std::less_equal, __VA_ARGS__)

#define TEQ(...)          EQ(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TNEQ(...)         NEQ(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TGT(...)          GT(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TLT(...)          LT(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TGE(...)          GE(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TLE(...)          LE(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)

#define IEQ(...)          EQ(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define INEQ(...)         NEQ(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IGT(...)          GT(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ILT(...)          LT(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IGE(...)          GE(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ILE(...)          LE(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define TEQ_CS(...)       EQ_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TNEQ_CS(...)      NEQ_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TGT_CS(...)       GT_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TLT_CS(...)       LT_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TGE_CS(...)       GE_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TLE_CS(...)       LE_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)

#define IEQ_CS(...)       EQ_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define INEQ_CS(...)      NEQ_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IGT_CS(...)       GT_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ILT_CS(...)       LT_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IGE_CS(...)       GE_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ILE_CS(...)       LE_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define LAND(...)         BINOP(std::logical_and, __VA_ARGS__)
#define LOR(...)          BINOP(std::logical_or, __VA_ARGS__)
#define LNOT(...)         UNOP(std::logical_not, __VA_ARGS__)

#define LAND_CS(...)      BINOP_CS(std::logical_and, __VA_ARGS__)
#define LOR_CS(...)       BINOP_CS(std::logical_or, __VA_ARGS__)
#define LNOT_CS(...)      UNOP_CS(std::logical_not, __VA_ARGS__)

#define ILAND(...)        LAND(INDEX_TYPE, __VA_ARGS__)
#define ILOR(...)         LOR(INDEX_TYPE, __VA_ARGS__)
#define ILNOT(...)        LNOT(INDEX_TYPE, __VA_ARGS__)

#define ILAND_CS(...)     LAND_CS(INDEX_TYPE, __VA_ARGS__)
#define ILOR_CS(...)      LOR_CS(INDEX_TYPE, __VA_ARGS__)
#define ILNOT_CS(...)     LNOT_CS(INDEX_TYPE, __VA_ARGS__)

#define BAND(...)         BINOP(std::bit_and, __VA_ARGS__)
#define BOR(...)          BINOP(std::bit_or, __VA_ARGS__)
#define XOR(...)          BINOP(std::bit_xor, __VA_ARGS__)
#define BNOT(...)         UNOP(std::bit_not, __VA_ARGS__)

#define BAND_CS(...)      BINOP_CS(std::bit_and, __VA_ARGS__)
#define BOR_CS(...)       BINOP_CS(std::bit_or, __VA_ARGS__)
#define XOR_CS(...)       BINOP_CS(std::bit_xor, __VA_ARGS__)
#define BNOT_CS(...)      UNOP_CS(std::bit_not, __VA_ARGS__)

#define IBAND(...)        BAND(INDEX_TYPE, __VA_ARGS__)
#define IBOR(...)         BOR(INDEX_TYPE, __VA_ARGS__)
#define IXOR(...)         XOR(INDEX_TYPE, __VA_ARGS__)
#define IBNOT(...)        BNOT(INDEX_TYPE, __VA_ARGS__)

#define IBAND_CS(...)     BAND_CS(INDEX_TYPE, __VA_ARGS__)
#define IBOR_CS(...)      BOR_CS(INDEX_TYPE, __VA_ARGS__)
#define IXOR_CS(...)      XOR_CS(INDEX_TYPE, __VA_ARGS__)
#define IBNOT_CS(...)     BNOT_CS(INDEX_TYPE, __VA_ARGS__)

#define TCMOV(...)        CMOV(MAJOR_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ICMOV(...)        CMOV(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define TCMOV_CS(...)     CMOV_CS(MAJOR_TYPE, INDEX_TYPE, __VA_ARGS__)
#define ICMOV_CS(...)     CMOV_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define LSHFT(...)        BINOP(loom::vm::ops::lshift, __VA_ARGS__)
#define RSHFT(...)        BINOP(loom::vm::ops::rshift, __VA_ARGS__)

#define LSHFT_CS(...)     BINOP_CS(loom::vm::ops::lshift, __VA_ARGS__)
#define RSHFT_CS(...)     BINOP_CS(loom::vm::ops::rshift, __VA_ARGS__)

#define ILSHFT(...)       LSHFT(INDEX_TYPE, __VA_ARGS__)
#define IRSHFT(...)       RSHFT(INDEX_TYPE, __VA_ARGS__)

#define ILSHFT_CS(...)    LSHFT_CS(INDEX_TYPE, __VA_ARGS__)
#define IRSHFT_CS(...)    RSHFT_CS(INDEX_TYPE, __VA_ARGS__)

#define ABS(...)          UNOP(loom::vm::ops::abs, __VA_ARGS__)
#define ROUND(...)        UNOP(loom::vm::ops::round, __VA_ARGS__)
#define CEIL(...)         UNOP(loom::vm::ops::ceil, __VA_ARGS__)
#define FLOOR(...)        UNOP(loom::vm::ops::floor, __VA_ARGS__)
#define TRUNC(...)        UNOP(loom::vm::ops::trunc, __VA_ARGS__)
#define SIN(...)          UNOP(loom::vm::ops::sin, __VA_ARGS__)
#define COS(...)          UNOP(loom::vm::ops::cos, __VA_ARGS__)
#define SQRT(...)         UNOP(loom::vm::ops::sqrt, __VA_ARGS__)
#define RSQRT(...)        UNOP(loom::vm::ops::rsqrt, __VA_ARGS__)
#define MIN(...)          BINOP(loom::vm::ops::min, __VA_ARGS__)
#define MAX(...)          BINOP(loom::vm::ops::max, __VA_ARGS__)
#define COPYSIGN(...)     BINOP(loom::vm::ops::copysign, __VA_ARGS__)

#define TABS(...)         ABS(MAJOR_TYPE, __VA_ARGS__)
#define TROUND(...)       ROUND(MAJOR_TYPE, __VA_ARGS__)
#define TCEIL(...)        CEIL(MAJOR_TYPE, __VA_ARGS__)
#define TFLOOR(...)       FLOOR(MAJOR_TYPE, __VA_ARGS__)
#define TTRUNC(...)       TRUNC(MAJOR_TYPE, __VA_ARGS__)
#define TSIN(...)         SIN(MAJOR_TYPE, __VA_ARGS__)
#define TCOS(...)         COS(MAJOR_TYPE, __VA_ARGS__)
#define TSQRT(...)        SQRT(MAJOR_TYPE, __VA_ARGS__)
#define TRSQRT(...)       RSQRT(MAJOR_TYPE, __VA_ARGS__)
#define TMIN(...)         MIN(MAJOR_TYPE, __VA_ARGS__)
#define TMAX(...)         MAX(MAJOR_TYPE, __VA_ARGS__)
#define TCOPYSIGN(...)    COPYSIGN(MAJOR_TYPE, __VA_ARGS__)

#define IMIN(...)         MIN(INDEX_TYPE, __VA_ARGS__)
#define IMAX(...)         MAX(INDEX_TYPE, __VA_ARGS__)

#define ABS_CS(...)       UNOP_CS(loom::vm::ops::abs, __VA_ARGS__)
#define ROUND_CS(...)     UNOP_CS(loom::vm::ops::round, __VA_ARGS__)
#define CEIL_CS(...)      UNOP_CS(loom::vm::ops::ceil, __VA_ARGS__)
#define FLOOR_CS(...)     UNOP_CS(loom::vm::ops::floor, __VA_ARGS__)
#define TRUNC_CS(...)     UNOP_CS(loom::vm::ops::trunc, __VA_ARGS__)
#define SIN_CS(...)       UNOP_CS(loom::vm::ops::sin, __VA_ARGS__)
#define COS_CS(...)       UNOP_CS(loom::vm::ops::cos, __VA_ARGS__)
#define SQRT_CS(...)      UNOP_CS(loom::vm::ops::sqrt, __VA_ARGS__)
#define RSQRT_CS(...)     UNOP_CS(loom::vm::ops::rsqrt, __VA_ARGS__)
#define MIN_CS(...)       BINOP_CS(loom::vm::ops::min, __VA_ARGS__)
#define MAX_CS(...)       BINOP_CS(loom::vm::ops::max, __VA_ARGS__)
#define COPYSIGN_CS(...)  BINOP_CS(loom::vm::ops::copysign, __VA_ARGS__)

#define TABS_CS(...)      ABS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TROUND_CS(...)    ROUND_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCEIL_CS(...)     CEIL_CS(MAJOR_TYPE, __VA_ARGS__)
#define TFLOOR_CS(...)    FLOOR_CS(MAJOR_TYPE, __VA_ARGS__)
#define TTRUNC_CS(...)    TRUNC_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSIN_CS(...)      SIN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCOS_CS(...)      COS_CS(MAJOR_TYPE, __VA_ARGS__)
#define TSQRT_CS(...)     SQRT_CS(MAJOR_TYPE, __VA_ARGS__)
#define TRSQRT_CS(...)    RSQRT_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMIN_CS(...)      MIN_CS(MAJOR_TYPE, __VA_ARGS__)
#define TMAX_CS(...)      MAX_CS(MAJOR_TYPE, __VA_ARGS__)
#define TCOPYSIGN_CS(...) COPYSIGN_CS(MAJOR_TYPE, __VA_ARGS__)

#define IMIN_CS(...)      MIN_CS(INDEX_TYPE, __VA_ARGS__)
#define IMAX_CS(...)      MAX_CS(INDEX_TYPE, __VA_ARGS__)

#define PACCAND(...)      PACC(std::logical_and, __VA_ARGS__)
#define PACCOR(...)       PACC(std::logical_or, __VA_ARGS__)

#define PACCAND_CS(...)   PACC_CS(std::logical_and, __VA_ARGS__)
#define PACCOR_CS(...)    PACC_CS(std::logical_or, __VA_ARGS__)

#define ANDEQ(...)        PACCAND(std::equal_to, __VA_ARGS__)
#define ANDNEQ(...)       PACCAND(std::not_equal_to, __VA_ARGS__)
#define ANDGT(...)        PACCAND(std::greater, __VA_ARGS__)
#define ANDLT(...)        PACCAND(std::less, __VA_ARGS__)
#define ANDGE(...)        PACCAND(std::greater_equal, __VA_ARGS__)
#define ANDLE(...)        PACCAND(std::less_equal, __VA_ARGS__)

#define OREQ(...)         PACCOR(std::equal_to, __VA_ARGS__)
#define ORNEQ(...)        PACCOR(std::not_equal_to, __VA_ARGS__)
#define ORGT(...)         PACCOR(std::greater, __VA_ARGS__)
#define ORLT(...)         PACCOR(std::less, __VA_ARGS__)
#define ORGE(...)         PACCOR(std::greater_equal, __VA_ARGS__)
#define ORLE(...)         PACCOR(std::less_equal, __VA_ARGS__)

#define ANDEQ_CS(...)     PACCAND_CS(std::equal_to, __VA_ARGS__)
#define ANDNEQ_CS(...)    PACCAND_CS(std::not_equal_to, __VA_ARGS__)
#define ANDGT_CS(...)     PACCAND_CS(std::greater, __VA_ARGS__)
#define ANDLT_CS(...)     PACCAND_CS(std::less, __VA_ARGS__)
#define ANDGE_CS(...)     PACCAND_CS(std::greater_equal, __VA_ARGS__)
#define ANDLE_CS(...)     PACCAND_CS(std::less_equal, __VA_ARGS__)

#define OREQ_CS(...)      PACCOR_CS(std::equal_to, __VA_ARGS__)
#define ORNEQ_CS(...)     PACCOR_CS(std::not_equal_to, __VA_ARGS__)
#define ORGT_CS(...)      PACCOR_CS(std::greater, __VA_ARGS__)
#define ORLT_CS(...)      PACCOR_CS(std::less, __VA_ARGS__)
#define ORGE_CS(...)      PACCOR_CS(std::greater_equal, __VA_ARGS__)
#define ORLE_CS(...)      PACCOR_CS(std::less_equal, __VA_ARGS__)

#define TANDEQ(...)       ANDEQ(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDNEQ(...)      ANDNEQ(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDGT(...)       ANDGT(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDLT(...)       ANDLT(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDGE(...)       ANDGE(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDLE(...)       ANDLE(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)

#define TOREQ(...)        OREQ(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORNEQ(...)       ORNEQ(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORGT(...)        ORGT(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORLT(...)        ORLT(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORGE(...)        ORGE(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORLE(...)        ORLE(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)

#define IANDEQ(...)       ANDEQ(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDNEQ(...)      ANDNEQ(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDGT(...)       ANDGT(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDLT(...)       ANDLT(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDGE(...)       ANDGE(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDLE(...)       ANDLE(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define IOREQ(...)        OREQ(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORNEQ(...)       ORNEQ(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORGT(...)        ORGT(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORLT(...)        ORLT(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORGE(...)        ORGE(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORLE(...)        ORLE(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define TANDEQ_CS(...)    ANDEQ_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDNEQ_CS(...)   ANDNEQ_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDGT_CS(...)    ANDGT_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDLT_CS(...)    ANDLT_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDGE_CS(...)    ANDGE_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TANDLE_CS(...)    ANDLE_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)

#define TOREQ_CS(...)     OREQ_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORNEQ_CS(...)    ORNEQ_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORGT_CS(...)     ORGT_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORLT_CS(...)     ORLT_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORGE_CS(...)     ORGE_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)
#define TORLE_CS(...)     ORLE_CS(INDEX_TYPE, MAJOR_TYPE, __VA_ARGS__)

#define IANDEQ_CS(...)    ANDEQ_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDNEQ_CS(...)   ANDNEQ_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDGT_CS(...)    ANDGT_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDLT_CS(...)    ANDLT_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDGE_CS(...)    ANDGE_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IANDLE_CS(...)    ANDLE_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)

#define IOREQ_CS(...)     OREQ_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORNEQ_CS(...)    ORNEQ_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORGT_CS(...)     ORGT_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORLT_CS(...)     ORLT_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORGE_CS(...)     ORGE_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
#define IORLE_CS(...)     ORLE_CS(INDEX_TYPE, INDEX_TYPE, __VA_ARGS__)
