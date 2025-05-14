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
    using p_t = cubic_fz_p<float>;
    constexpr p_t p{};
    fmt::print("Program:\n");
    print_program(p);
    fmt::print("Variables:\n");
    print_variables(p_t::variables{});
    print_header(p,
                 std::bitset<typelist::length<p_t::variables>>(-1ull << 1),
                 4,
                 true);
    fmt::print("\n");

    fmt::print("Ctrl+Q to quit.\n");
    RawTerminalReader reader(false);

    char c = reader();
    while(!quit_condition(c))
    {
        if(std::iscntrl(c))
            fmt::print("{}\r\n", c);
        else
            fmt::print("{c:d} {c}\r\n", fmt::arg("c", c));
        c = reader();
    }

    return 0;
}
