// -*- C++ -*-
// vm_smallpt.cpp
#include <common/test.hpp>
#include <smallpt/ref/rng.hpp>
#include <smallpt/vm/rng.hpp>
#include <vm/bse.hpp>

using namespace loom;
using namespace test;

// clang-format off
KERNEL(rotl32_test, VAR(std::uint32_t, x), VAR(std::uint32_t, n))
    INSERT_MACRO(vm::rotl32, CS(x), CS(n), CS(dst), CS(tmp))
KERNEL_END(VAR(std::uint32_t, dst));
// clang-format on

int
main(int argc, char* argv[])
{
    Track         track;
    ParmTestMaker tm{parse_numruns<100000>(argc, argv)};
    RandomState   rs;

    vm::bse::function<rotl32_test> bse_rotl32;

    track.check(NamedTest(
        tm.with_ref([gx = rs.make_gen<std::uint32_t>(0, -1u),
                     gn = rs.make_gen<std::uint32_t>(0, 31)]() mutable noexcept
                    { return std::tuple{gx(), gn()}; },
                    bse_rotl32,
                    rotl32),
        "rotl32"));

    return track.ret_code();
}
