// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <tests/root_finding.hpp>

#include <cctype>

#define MOD_CTRL(k) ((k) & 0x1f)

using namespace std;
using namespace lucid;

int main(/*int argc, char* argv[]*/)
{
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
