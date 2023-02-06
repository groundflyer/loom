// -*- C++ -*-

#include "benchmark.hpp"

#include <base/linal.hpp>
#include <utils/tuple.hpp>
#include <utils/typeinfo.hpp>

#include <random>

using namespace std;
using namespace lucid;

template <typename T, std::size_t N, typename G>
static void
bench(LogFile& log, G& g, const size_t n) noexcept
{
    using Vec = Vector<T, N, array>;

    const auto typestring = get_typeinfo_string(Vec{});

    uniform_real_distribution<T> dist(T{-100000}, T{100000});
    auto vv   = [&]() noexcept { return Vec(generate<3>(dist, g)); };
    auto vgen = [&]() noexcept { return pair{vv(), vv()}; };

    microbench(log, n, fmt::format("{} dot", typestring), vgen, dot);
    microbench(log, n, fmt::format("{} cross", typestring), vgen, cross);
}

int
main(int argc, char* argv[])
{
    INIT_LOG

    random_device                         rd;
    default_random_engine                 g(rd());
    std::uniform_real_distribution<float> dist(-100000.f, 100000.f);
    auto                                  mgen = [&]() noexcept
    { return Matrix<float, 4, 4, std::array>(generate<16>(dist, g)); };
    bench<float, 3>(log, g, n);
    bench<float, 4>(log, g, n);
    bench<double, 3>(log, g, n);
    bench<double, 4>(log, g, n);

    microbench(
        log,
        n,
        "Mat4 dot",
        [&]() noexcept {
            return pair{mgen(), mgen()};
        },
        dot);
    microbench(
        log,
        n,
        "Mat4 dot Vec4",
        [&]() noexcept {
            return pair{mgen(),
                        Vector<float, 4, std::array>(generate<4>(dist, g))};
        },
        dot);

    return 0;
}
