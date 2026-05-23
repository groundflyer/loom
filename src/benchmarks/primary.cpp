// -*- C++ -*-
// smallpt.cpp
#if !(defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__))
#error "This program requires at least AVX"
#endif

#include "primary.hpp"
#include "bm.hpp"
#include "render.hpp"

#include <common/argparse.hpp>

#include <string_view>

using namespace loom;
using namespace vm;

using C1 = constants<CS(256), CS(0.1), CS(64), CS(1)>;

BM_PRIMARY(C1)
