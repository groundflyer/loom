#include <lang/cuda.hpp>
#include <utils/math.hpp>

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <random>

// clang-format off
template <typename T>
TBEGIN1(increment, val)
    TADD(val, val, 1)
TEND1(val)

template <std::integral T>
TBEGIN1(factorial_int, a)
    TSUB(c, a, 1),
    LABEL(loop),
    TMUL(a, a, c),
    TSUB(c, c, 1),
    TEQ(cmp, c, 0),
    PJMP2(cmp, loop, END)
TEND1(a)

int main(/*int argc, char* argv[]*/)
{
    // clang-format on
    using namespace lucid;
    int ret = 0;
    {
        using eval_incr_t = lucid::cuda::evaluator<increment<float>>;
        eval_incr_t eval_incr;
        auto        incr_r = eval_incr(cuda::BLOCK_SIZE, 0.f);
        const bool  all =
            std::ranges::all_of(incr_r, [](auto v) { return v == 1; });
        if(!all)
        {
            std::cout << "increment fail\n";
            std::ranges::for_each(incr_r,
                                  [](auto v) { std::cout << v << ", "; });
            std::cout << std::endl;
        }
        else
            std::cout << "increment ok\n";
        ret += !all;
    }

    {
        using eval_fac_t = lucid::cuda::evaluator<factorial_int<unsigned>>;
        eval_fac_t                              eval_fac;
        std::random_device                      rd;
        std::default_random_engine              g(rd());
        std::uniform_int_distribution<unsigned> dist(2, 10);
        const unsigned                          n = cuda::BLOCK_SIZE;
        std::vector<unsigned>                   input, output, val, tmp;
        std::generate_n(std::back_inserter(input),
                        n,
                        [&]() { return dist(g); });
        auto val_fac = [](auto i) noexcept { return fac(i); };
        std::transform(input.cbegin(),
                       input.cend(),
                       std::back_inserter(val),
                       val_fac);
        std::sort(val.begin(), val.end());
        auto vout = eval_fac(n, input);
        std::copy(vout.cbegin(), vout.cend(), std::back_inserter(output));
        std::sort(output.begin(), output.end());
        std::transform(val.cbegin(),
                       val.cend(),
                       output.cbegin(),
                       std::back_inserter(tmp),
                       [](unsigned v, unsigned t) -> unsigned
                       { return v == t; });
        const unsigned fac_r_acc = std::accumulate(tmp.cbegin(), tmp.cend(), 0);
        if(fac_r_acc != n)
        {
            std::cout << "factorial fail\n";
            std::ranges::for_each(output,
                                  [](auto v) { std::cout << v << ", "; });
            std::cout << std::endl;
            // std::cout << "input:\n";
            // std::ranges::for_each(input,
            //                       [](auto v) { std::cout << v << ", "; });
            // std::cout << std::endl;
            // std::cout << "valid:\n";
            // std::ranges::for_each(val, [](auto v) { std::cout << v << ", ";
            // });
        }
        else
            std::cout << "factorial ok\n";
        ret += fac_r_acc != n;
    }
    cudaDeviceReset();

    return ret;
}
