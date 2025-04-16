// -*- C++ -*-
// dbg.cpp

#include "dbg.hpp"

using namespace std;
using namespace lucid;

int main(/*int argc, char* argv[]*/)
{
    TerminalReader reader;

    char c;
    while(reader(&c) && c != 'q');

    return 0;
}
