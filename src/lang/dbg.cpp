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

int
main(int argc, char* argv[])
{
    const auto pr = parse(
        std::tuple{
            flag<'p', "print">("print program"),
            flag<'i', "interactive">("run interactive"),
            option<"input", 4>("input values",
                               {"a", "b", "c", "d"},
                               {1.f, 2.f, 3.f, 4.f},
                               read_number<float, true>),
            option<'w', "column-width">("column width. Default is 20",
                                        "N",
                                        20u,
                                        read_number<unsigned, true>),
            option<'s', "max-steps">("maximum number of steps. Default is 100",
                                     "N",
                                     100u,
                                     read_number<unsigned, true>),
            option_list<'t', "track">("variables to track", "vars")},
        {argc, argv});
    constexpr p_t p{};

    unsigned                        cw = 20;
    unsigned                        ms = 100;
    decltype(pr.get_opt<"input">()) fi{1, 2, 3, 4};

    try
    {
        cw = pr.get_opt<'w'>();
        ms = pr.get_opt<'s'>();
        fi = pr.get_opt<"input">();
    }
    catch(const ReadException& ex)
    {
        print_errc(ex.ec, ex.sv);
        return 1;
    }

    if(pr.get_opt<'p'>())
    {
        fmt::print("Program:\n");
        dbg::print_program(p);
    }

    if(pr.get_opt<'i'>())
    {
        auto mask       = interactive_get_mask(p_t::variables{});
        auto inputs     = interactive_get_inputs(p_t{});
        using Inspector = debug_compiler<p_t, true>::inspector;
        Inspector insp{cw, true, mask};
        std::apply(insp, inputs);
    }
    else
    {
        using Inspector = debug_compiler<p_t, false>::inspector;
        Inspector insp{
            ms,
            cw,
            true,
            read_var_mask(typename p_t::variables{}, pr.get_opt<'t'>())};
        std::apply(insp, fi);
    }

    return 0;
}
