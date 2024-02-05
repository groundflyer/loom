#include <lang/cuda.hpp>

#include <iostream>

// clang-format off
template <typename T>
TBEGIN1(increment, val)
    TADD(val, val, 1)
TEND1(val)

int main(/*int argc, char* argv[]*/)
{
    using eval_t = lucid::cuda::evaluator<increment<float>>::single_cell_evaluator;
    eval_t eval;
    auto r = eval(100, 0.f);
    int ret = 0;
    const bool all = std::ranges::all_of(r, [](auto v) { return v == 1; });
    if (!all)
    {
        std::cout << "not all\n";
        std::ranges::for_each(r, [](auto v) { std::cout << v << ", "; });
        std::cout << std::endl;
    }
    ret += !all;
    return ret;
}
