// -*- C++ -*-
// cuda.hpp
//

#pragma once

#include <vector>

template <typename T>
std::vector<T>
cuda_inc(unsigned, const std::vector<T>&);

template <typename T>
std::vector<T>
cuda_fac(unsigned, const std::vector<T>&);
