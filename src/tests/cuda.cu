#include "programs.hpp"

#include <lang/cuda.hpp>

template <typename T>
std::vector<T>
cuda_inc(unsigned n, const std::vector<T>& in)
{
    lucid::cuda::evaluator<increment<T>> eval;
    return eval(n, in);
}

template <typename T>
std::vector<T>
cuda_fac(unsigned n, const std::vector<T>& in)
{
    lucid::cuda::evaluator<factorial_int<T>> eval;
    return eval(n, in);
}

template std::vector<int>
cuda_inc(unsigned, const std::vector<int>&);

template std::vector<unsigned>
cuda_fac(unsigned, const std::vector<unsigned>&);
