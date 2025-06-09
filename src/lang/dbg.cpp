// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <tests/root_finding.hpp>

#include <cctype>

using namespace std;
using namespace lucid;
using namespace dbg;

template <floating_point T>
using cubic_fz_p = cubic_fz_program<T, typename constants<T>::errt>;

int main(/*int argc, char* argv[]*/)
{
    using p_t = cubic_fz_p<float>;
    constexpr p_t p{};
    fmt::print("Program:\n");
    dbg::print_program(p);
    auto inputs     = get_inputs(p_t{});
    auto mask       = get_mask(p_t::variables{});
    using Inspector = debug_compiler<p_t>::inspector;
    Inspector insp{5, true, mask};
    maybe_apply(insp, inputs);

    return 0;
}
