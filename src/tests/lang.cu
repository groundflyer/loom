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

#include <lang/cuda.hpp>
#include <utils/test.hpp>

#include <bit>

using namespace lucid;
using namespace lucid::test;

int
main(int argc, char* argv[])
{
    Track          track;
    const unsigned vector_size = std::bit_floor(parse_numruns(argc, argv));
    ParmTestMaker  tm{vector_size};
    RandomState    rs;

    auto g_int   = rs.make_gen<int>(2, 10);
    auto g_ui    = rs.make_gen<unsigned>(2, 10);
    auto g_float = rs.make_gen<float>();
    auto g_u3    = rs.make_gen<unsigned>(0, 3);
    auto g_j4    = [&]() noexcept { return std::pair{g_u3(), g_float()}; };
    auto g_shift = [&]() noexcept { return std::pair(g_ui(), g_ui()); };

    cuda::function<incr_int>     cuda_ii(vector_size);
    cuda::function<fac_unsigned> cuda_fu(vector_size);
    cuda::function<fac_int>      cuda_fi(vector_size);
    cuda::function<sut>          cuda_su(vector_size);
    cuda::function<cut>          cuda_cu(vector_size);
    cuda::function<jmp4_float>   cuda_j4(vector_size);
    cuda::function<choice_float> cuda_ch(vector_size);
    cuda::function<lshift_u>     cuda_ls(vector_size);
    cuda::function<rshift_u>     cuda_rs(vector_size);

    track.check(NamedTest(tm.rng_with_scalar_ref(g_int, cuda_ii, val_inc),
                          "CUDA increment int"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_ui, cuda_fu, val_fac),
                          "CUDA factorial unsigned"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_float, cuda_su, val_sut),
                          "CUDA simple union"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_float, cuda_cu, val_cut),
                          "CUDA complex union"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_j4, cuda_j4, val_jmp4),
                          "CUDA jump4"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_j4, cuda_ch, val_choice),
                          "CUDA choice"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_shift, cuda_ls, val_lshift),
                          "CUDA left shift"));
    track.check(NamedTest(tm.rng_with_scalar_ref(g_shift, cuda_rs, val_rshift),
                          "CUDA right shift"));

    return track.ret_code();
}
