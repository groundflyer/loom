// -*- C++ -*-

#include "generators.hpp"
#include "util.hpp"
#include "vm_test_suite_utils.hpp"

#include <benchmarks/fp_mix.hpp>

#include <array>

#include <fmt/core.h>

using namespace loom;
using namespace vm;
using namespace test;

int
main(int argc, char* argv[])
{
    auto [track, tm, rs] = test_init(argc, argv);

    test_suite_types(all_floats{},
                     RefBSESuite<fp_mix_k, GM_F3, fp_mix_ref, true>{},
                     track,
                     tm,
                     rs);

    return track.ret_code();
}
