#include "programs.hpp"

#include <lang/cuda.hpp>

#define DEF_TEST_FUNC(NAME, PROGRAM)                          \
    template <typename T>                                     \
    std::vector<T> NAME(unsigned n, const std::vector<T>& in) \
    {                                                         \
        lucid::cuda::evaluator<PROGRAM> eval;                 \
        return eval(n, in);                                   \
    }

#define DEF_TEST_FUNC_UNION(NAME, PROGRAM)                    \
    template <typename R, typename T>                         \
    std::vector<R> NAME(unsigned n, const std::vector<T>& in) \
    {                                                         \
        lucid::cuda::evaluator<PROGRAM> eval;                 \
        return eval(n, in);                                   \
    }

#define DEF_TEST_FUNC2(NAME, PROGRAM)                \
    template <typename T1, typename T2>              \
    std::vector<T2> NAME(unsigned               n,   \
                         const std::vector<T1>& in1, \
                         const std::vector<T2>& in2) \
    {                                                \
        lucid::cuda::evaluator<PROGRAM> eval;        \
        return eval(n, in1, in2);                    \
    }

#define INST_TEST_FUNC(NAME, T) \
    template std::vector<T> NAME(unsigned, const std::vector<T>&);

#define INST_TEST_FUNC_UNION(NAME, R, T) \
    template std::vector<R> NAME<R>(unsigned, const std::vector<T>&);

#define INST_TEST_FUNC2(NAME, T1, T2)                     \
    template std::vector<T2> NAME(unsigned,               \
                                  const std::vector<T1>&, \
                                  const std::vector<T2>&);

DEF_TEST_FUNC(cuda_inc, incr_int)
DEF_TEST_FUNC(cuda_fac, fac_unsigned)
DEF_TEST_FUNC_UNION(cuda_sut, sut)
DEF_TEST_FUNC_UNION(cuda_cut, cut)
DEF_TEST_FUNC2(cuda_jmp4, jmp4_float)
DEF_TEST_FUNC2(cuda_choice, choice_float)

INST_TEST_FUNC(cuda_inc, int)
INST_TEST_FUNC(cuda_fac, unsigned)
INST_TEST_FUNC_UNION(cuda_sut, unsigned, float)
INST_TEST_FUNC_UNION(cuda_cut, unsigned, float)
INST_TEST_FUNC2(cuda_jmp4, unsigned, float)
INST_TEST_FUNC2(cuda_choice, unsigned, float)
