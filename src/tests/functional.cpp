// -*- C++ -*-
/// @file functional.cpp
/// @brief functional.hpp tests.

#include <common/functional.hpp>
#include <common/test.hpp>

using namespace std;
using namespace loom;
using namespace test;

int
main()
{
    Track track;

    /// @test fold with +
    track.check(NamedTest([]() noexcept
                          { return fold(std::plus<int>{}, 1, 2, 3, 4) == 10; },
                          "fold"));

    /// @test composition with constant passing
    track.check(NamedTest(
        []() noexcept
        {
            const auto                a = [](const int& n) { return n + 10; };
            const auto                b = [](const int n) { return n * 2; };
            const auto                c = [](const int& n) { return n / 5; };
            const auto                f = compose(a, b, c);
            std::decay_t<decltype(f)> f1;
            f1           = f;
            const int n  = 10;
            const int rc = f1(n);
            const int rs = a(b(c(n)));
            return rc == rs;
        },
        "composition with constant passing"));

    /// @test composition with reference passing
    track.check(NamedTest(
        []() noexcept
        {
            auto a = [](int& n) -> int&
            {
                n += 10;
                return n;
            };
            auto b = [](int& n) -> int&
            {
                n *= 2;
                return n;
            };
            auto c = [](int& n) -> int&
            {
                n /= 5;
                return n;
            };
            auto       f      = compose(a, b, c);
            int        n1     = 10;
            int        n2     = n1;
            int&       r1     = f(n1);
            int&       r2     = a(b(c(n2)));
            const bool val_ok = r1 == r2;
            const bool ref_ok = &n1 == &r1;
            return val_ok && ref_ok;
        },
        "composition with reference passing"));

    /// @test flip
    track.check(NamedTest(
        []() noexcept
        {
            auto        f = flip(std::minus<float>{});
            const float a = 10.f;
            const float b = 2.f;
            const float r = f(a, b);
            const float t = b - a;
            return r == t;
        },
        "flip"));

    /// @test constructor composition
    track.check(NamedTest(
        []() noexcept
        {
            struct MakeInt
            {
                int n = 0;

                explicit constexpr MakeInt(int _n) : n(_n) {}
            };

            struct MakeFloat
            {
                float f = 0.f;

                explicit constexpr MakeFloat(const MakeInt& n) : f(n.n) {}
            };

            struct MakeDouble
            {
                double d = 0.;

                explicit constexpr MakeDouble(const MakeFloat& f) : d(f.f) {}
            };

            constexpr auto cc =
                compose(maker<MakeDouble>, maker<MakeFloat>, maker<MakeInt>);
            auto dd = cc(10);
            static_assert(std::is_same_v<decltype(dd), MakeDouble>);
            return dd.d == 10.0;
        },
        "maker compose"));

    return track.ret_code();
}
