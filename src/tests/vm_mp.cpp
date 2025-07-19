// -*- C++ -*-
// vm_mp.cpp
#include <vm/bve.hpp>
#include <vm/mp.hpp>

#include "programs.hpp"
#include "root_finding.hpp"

#include <vector>

using namespace std;
using namespace lucid;

template <typename P>
using BVEStreamCompiler = bve::stream<P, bve::INTROSORT>;

template <typename P>
using BVEContext = mp::Context<P, BVEStreamCompiler>;

using fac_p = factorial_int<std::uint64_t>;
using fib_p = fib<std::uint64_t>;
using fz_p  = cubic_fz_program<double, CS(0.1)>;

using programs = type_sequence<fac_p, fib_p, fz_p>;

int main(/*int argc, char *argv[]*/)
{
    auto s10 = views::single(10ul);
    auto fz1 = views::single(tuple{1., 2., 3., 4.});

    vector<output_type<fac_p>> v_fac;
    vector<output_type<fib_p>> v_fib;
    vector<output_type<fz_p>>  v_fz;

    auto oc = mp::make_output_collection(v_fac, v_fib, v_fz);

    {
        mp::Shop<programs, BVEContext, 4> shop(1000);
        auto                              ic = shop.bind_inputs(s10, s10, fz1);
        {
            auto tp = shop.get_thread_pool();
            shop.exchange(ic, oc);
        }
        shop.finalize(oc);
    }
    {
        mp::Shop<programs, BVEContext, dynamic_size> shop(4, 1000);
        auto ic = shop.bind_inputs(s10, s10, fz1);
        {
            auto tp = shop.get_thread_pool();
            shop.exchange(ic, oc);
        }
        shop.finalize(oc);
    }
    return 0;
}
