#include "pt.hpp"
#include <vm/simd.hpp>

namespace loom
{
using vft = vm::simd::native_vector<float>;
using vut = vm::simd::native_vector<std::uint32_t>;

std::tuple<vft, vft, vft, vut, vut>
simd_compact_pt(vut vpp, vut vres, vft vt, vut vd)
{
    vm::simd::compact_function<vm::pt<float>, false, true> f;
    return f(vpp, vres, vt, vd);
}
} // namespace loom
