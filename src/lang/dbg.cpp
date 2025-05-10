// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <tests/root_finding.hpp>

#include <cctype>

using namespace std;
using namespace lucid;

template <floating_point T>
using cubic_fz_p = cubic_fz_program<T, typename constants<T>::errt>;

int main(/*int argc, char* argv[]*/)
{
    using p = cubic_fz_p<float>;
    fmt::print("Program:\n");
    print_program(p{});
    fmt::print("Variables:\n");
    print_variables(p{});

    RawTerminalReader reader(false);
    fmt::print("Ctrl+Q to quit.\n");

    while(true)
    {
        char c = reader();
        if(std::iscntrl(c))
            fmt::print("{}\r\n", c);
        else
            fmt::print("{c:d} {c}\r\n", fmt::arg("c", c));
        if(quit_condition(c)) break;
    }

    return 0;
}
