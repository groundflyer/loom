// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

#include <cctype>

using namespace std;
using namespace lucid;

int main(/*int argc, char* argv[]*/)
{
    RawTerminalReader reader(false);

    while(true)
    {
        char c = '\0';
        reader(&c);
        if(std::iscntrl(c))
            fmt::print("{:d}\r\n", c);
        else
            fmt::print("{c:d} {c}\r\n", fmt::arg("c", c));
        if(c == 'q') break;
    }

    return 0;
}
