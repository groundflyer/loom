// -*- C++ -*-
// smallpt.cpp
#include "pt_macros.hpp"

#include <smallpt/vm/enc.hpp>
#include <smallpt/vm/pt.hpp>
#include <vm/dbg.hpp>

using namespace std;
using namespace loom;
using namespace vm;
using namespace dbg;
using namespace argparse;

int
main(int argc, char* argv[])
{
    return inspect<
        pt2_const<float, constants<CS(40), CS(0.0001), CS(64), CS(1), CS(5)>>,
        pt_macros>(
        argc,
        argv,
        std::tuple{option<"pp", 2>("pixel position",
                                   {"X", "Y"},
                                   {0, 0},
                                   read_number<unsigned, true>)},
        [](const auto& pr)
        { return std::tuple{encode_pp(Vec2u{pr.template get_opt<"pp">()})}; });
}
