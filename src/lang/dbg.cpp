// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <tests/root_finding.hpp>

#include <cctype>

#define MOD_CTRL(k) ((k) & 0x1f)

using namespace std;
using namespace lucid;

template <floating_point T>
using cubic_fz_p = cubic_fz_program<T, typename constants<T>::errt>;

int main(/*int argc, char* argv[]*/)
{
    print_program(cubic_fz_p<float>{});

    RawTerminalReader reader(false);

    while(true)
    {
        char c = reader();
        if(std::iscntrl(c))
            fmt::print("{:d}\r\n", c);
        else
            fmt::print("{c:d} {c}\r\n", fmt::arg("c", c));
        if(quit_condition(c)) break;
    }

    return 0;
}
