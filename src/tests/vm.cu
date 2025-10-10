// this is to shut clang up
namespace std
{
__device__ __inline__ bool
isfinite(long double)
{
    return true;
}

__device__ __inline__ bool
isnan(long double)
{
    return false;
}
} // namespace std

#include "programs.hpp"
#include "stream_wrapper.hpp"

#include <vm/cuda.hpp>
#include <utils/test.hpp>

#include <bit>

using namespace lucid;
using namespace vm;
using namespace test;

using incr_int     = increment<int>;
using fac_int      = factorial_int<int>;
using fac_unsigned = factorial_int<unsigned>;
using sut          = simple_union_test<float>;
using cut          = complex_union_test<float>;
using jmp4_float   = jmp4_test<float>;
using cmov_float   = cmov_test<float>;
using bit_u        = bit_test<unsigned>;
using lshift_u     = lshift_test<unsigned>;
using rshift_u     = rshift_test<unsigned>;
using abs_float    = abs_test<float>;
using trunc_float  = trunc_test<float>;
using ceilfloor_f  = ceilfloor<float>;
using minmax_float = minmax_test<float>;

int
main(int argc, char* argv[])
{
    Track          track;
    const unsigned vector_size =
        std::bit_floor(parse_numruns<102400>(argc, argv));
    const unsigned stream_size = vector_size >> 1;
    ParmTestMaker  tm{vector_size};
    RandomState    rs;

    auto g_int   = rs.make_gen<int>(2, 10);
    auto g_ui    = rs.make_gen<unsigned>(2, 10);
    auto g_float = rs.make_gen<float>();
    auto g_u3    = rs.make_gen<unsigned>(0, 3);
    auto g_j4    = [&]() noexcept { return std::pair{g_u3(), g_float()}; };
    auto g_shift = [&]() noexcept { return std::pair(g_ui(), g_ui()); };

    // wrap tests to reuse memory
    auto test_function =
        [&]<Kernel K>(K, auto& g, auto val, std::string_view name) noexcept
    {
        cuda::function<K> func(vector_size);
        return track.check(
            NamedTest(tm.rng_with_scalar_ref(g, func, val), name));
    };

    auto test_stream =
        [&]<Kernel K>(K, auto& g, auto val, std::string_view name) noexcept
    {
        cuda::stream<K> stream(stream_size);
        return track.check(
            NamedTest(tm.rng_with_scalar_ref(g, StreamWrapper(&stream), val),
                      name));
    };

    test_function(incr_int{}, g_int, val_inc, "CUDA increment int");
    test_function(fac_unsigned{}, g_ui, val_fac, "CUDA factorial unsigned");
    test_function(fac_int{}, g_int, val_fac, "CUDA factorial int");
    test_function(sut{}, g_float, val_sut, "CUDA simple union");
    test_function(cut{}, g_float, val_cut, "CUDA complex union");
    test_function(jmp4_float{}, g_j4, val_jmp4, "CUDA jump4");
    test_function(cmov_float{}, g_j4, val_cmov, "CUDA cmov");
    test_function(lshift_u{}, g_shift, val_lshift, "CUDA left shift");
    test_function(rshift_u{}, g_shift, val_rshift, "CUDA right shift");
    test_stream(fac_unsigned{},
                g_ui,
                val_fac,
                "CUDA stream factorial unsigned");

    return track.ret_code();
}
