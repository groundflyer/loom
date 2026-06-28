#include "utils.hpp"

namespace loom
{
std::tuple<vft, vft, vft, vut, vut>
simd_compact_pt(vut vpp, vut vres, vft vt, vut vd)
{
    vm::simd::compact_function<pt_float, false, true> f;
    return f(vpp, vres, vt, vd);
}
} // namespace loom
