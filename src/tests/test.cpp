// -*- C++ -*-
// test.cpp
#include <utils/test.hpp>

inline constexpr std::bool_constant<false> failing{};
inline constexpr std::bool_constant<true>  passing{};

using namespace lucid::test;

int
main()
{
    const NamedTest failing_test{failing, "Failing test"};
    NamedTest       passing_test{passing, "Passing test"};

    const auto           f = []() noexcept { return 0.1; };
    const NamedFuzzyTest failing_fuzzy{f, "Failing fuzzy test", 0.05};
    NamedFuzzyTest       passing_fuzzy{f, "Passing fuzzy test", 0.15};

    Track track;
    track.check(failing_test);
    track.check(passing_test);
    track.check(failing_fuzzy);
    track.check(passing_fuzzy);

    track.dep_check(passing_test,
                    passing_fuzzy,
                    failing_fuzzy,
                    failing_test,
                    passing_test);

    track.dep_check(
        std::pair{passing_test, passing_fuzzy},
        std::tuple{failing_fuzzy, failing_test, passing_test, passing_fuzzy},
        passing_test);

    track.dep_check(passing_test,
                    std::pair{passing_test, passing_fuzzy},
                    std::pair{passing_test, passing_fuzzy},
                    passing_test);

    return track.total() != 22 || track.passed() != 14;
}
