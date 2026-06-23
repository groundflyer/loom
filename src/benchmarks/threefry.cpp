// -*- C++ -*-
// threefry.cpp
#include "bm.hpp"
#include "utils.hpp"

#include <common/random.hpp>
#include <common/timer.hpp>
#include <vm/lib/random.hpp>
#include <vm/simd.hpp>

#include <algorithm>

using namespace loom;

constexpr std::uint32_t k0 = 0xdead;
constexpr std::uint32_t k1 = 0xbeef;
// constexpr std::uint32_t k2 = 0x1bd11bda ^ k0 ^ k1; // 0x1bd17b98
constexpr std::uint32_t ctr_inc = 39916801;

// clang-format off

KERNEL(kernel, VAR(std::uint32_t, c0), VAR(std::uint32_t, c1))
    MOV(VAR(std::uint32_t, u0), VAR(std::uint32_t, c0)),
    MOV(VAR(std::uint32_t, u1), VAR(std::uint32_t, c1)),
    INSERT_MACRO(vm::threefry2x32r32, CS(u0), CS(u1), CS(0xdead), CS(0xbeef), CS(0x1bd17b98), CS(tmp0), CS(tmp1)),
    INSERT_MACRO(vm::bump_ctr, CS(c0), CS(c1), CS(39916801), CS(tmp0), CS(tmp1), CS(tmp2))
KERNEL_END(VAR(std::uint32_t, u0), VAR(std::uint32_t, u1), VAR(std::uint32_t, c0), VAR(std::uint32_t, c1));

// clang-format on

struct Ref
{
    std::size_t    size;
    std::uint32_t& dest0;
    std::uint32_t& dest1;

    std::string
    info() const noexcept
    {
        return fmt::format("reference|{}", size);
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        std::array<std::uint32_t, 2> c{1, 11};

        ElapsedTimer<> timer;

        for(std::size_t i = 0; i < size; i += 2)
        {
            std::tie(dest0, dest1) = threefry2x32r32<k0, k1>(c);

            c = bump_ctr(c, ctr_inc);
        }

        return timer.elapsed();
    }
};

struct SIMD
{
    static constexpr std::size_t N =
        vm::simd::native_vector_extent_v<std::uint32_t>;
    using vector_type = vm::simd::native_vector<std::uint32_t>;

    std::size_t  size;
    vector_type& dest0;
    vector_type& dest1;

    std::string
    info() const noexcept
    {
        return fmt::format("simd|{}", size);
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        vm::simd::compact_function<kernel> f;

        vector_type vc0;
        vector_type vc1;

        std::array<std::uint32_t, 2> c{1, 11};

        for(std::size_t i = 0; i < N; ++i)
        {
            vc0[i] = c[0];
            vc1[i] = c[1];

            c = bump_ctr(c);
        }

        ElapsedTimer<> timer;

        for(std::size_t i = 0; i < size; i += N * 2)
            std::tie(dest0, dest1, vc0, vc1) = f(vc0, vc1);

        return timer.elapsed();
    }
};

int
main(int argc, char* argv[])
{
    const std::size_t num_runs = align_up(500000000, 64);
    std::uint32_t     d0, d1;
    SIMD::vector_type vd0, vd1;

    run({argc, argv}, Ref{num_runs, d0, d1}, SIMD{num_runs, vd0, vd1});
    return 0;
}
