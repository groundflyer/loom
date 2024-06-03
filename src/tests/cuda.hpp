// -*- C++ -*-
// cuda.hpp
//

#pragma once

#include <vector>

#define DECL_TEST_FUNC(NAME) \
    template <typename T>    \
    std::vector<T> NAME(unsigned, const std::vector<T>&);

#define DECL_TEST_FUNC_UNION(NAME)    \
    template <typename R, typename T> \
    std::vector<R> NAME(unsigned, const std::vector<T>&);

#define DECL_TEST_FUNC2(NAME)                    \
    template <typename T1, typename T2>          \
    std::vector<T2> NAME(unsigned,               \
                         const std::vector<T1>&, \
                         const std::vector<T2>&);

DECL_TEST_FUNC(cuda_inc)
DECL_TEST_FUNC(cuda_fac)
DECL_TEST_FUNC_UNION(cuda_sut)
DECL_TEST_FUNC_UNION(cuda_cut)
DECL_TEST_FUNC2(cuda_jmp4)
DECL_TEST_FUNC2(cuda_choice)

#undef DECL_TEST_FUNC
#undef DECL_TEST_FUNC2
#undef DECL_TEST_FUNC_UNION
