// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <tests/root_finding.hpp>
#include <utils/argparse.hpp>

#include <fast_float/fast_float.h>

#include <cctype>
#include <charconv>

using namespace std;
using namespace lucid;
using namespace dbg;
using namespace argparse;

template <floating_point T>
using cubic_fz_p = cubic_fz_program<T, typename constants<T>::errt>;

using p_t = cubic_fz_p<float>;

constexpr auto interactive_flag = flag<'i', "interactive">("Run interactive");
constexpr auto input_option     = option<"input", 4>(
    "Input values",
    {"a", "b", "c", "d"},
    {1.f, 2.f, 3.f, 4.f},
    [](std::string_view s)
    {
        float ret;
        fast_float::from_chars(s.data(), s.data() + s.size(), ret);
        return ret;
    });

int
main(int argc, char* argv[])
{
    const auto pr =
        parse(std::tuple{interactive_flag, input_option}, {argc, argv});
    constexpr p_t p{};
    fmt::print("Program:\n");
    dbg::print_program(p);
    if(pr.get_opt<'i'>())
    {
        auto inputs     = get_inputs(p_t{});
        auto mask       = get_mask(p_t::variables{});
        using Inspector = debug_compiler<p_t, true>::inspector;
        Inspector insp{20, true, mask};
        std::apply(insp, inputs);
    }
    else
    {
        using Inspector = debug_compiler<p_t, false>::inspector;
        Inspector insp{100, 20, true};
        std::apply(insp, pr.get_opt<"input">());
    }

    return 0;
}
