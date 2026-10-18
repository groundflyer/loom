// -*- C++ -*-
// test.cpp
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <common/test.hpp>

inline constexpr std::bool_constant<false> failing{};
inline constexpr std::bool_constant<true>  passing{};

using namespace loom::test;

int
main()
{
    const NamedTest failing_test{failing, "Failing test"};
    NamedTest       passing_test{passing, "Passing test"};

    const auto           f = []() noexcept { return 0.1; };
    const NamedFuzzyTest failing_fuzzy{f, "Failing fuzzy test", 0.05};
    NamedFuzzyTest       passing_fuzzy{f, "Passing fuzzy test", 0.15};

    Track track;
    fmt::print("failing\n");
    track.check(failing_test);
    fmt::print("passing\n");
    track.check(passing_test);
    fmt::print("failing\n");
    track.check(failing_fuzzy);
    fmt::print("passing\n");
    track.check(passing_fuzzy);

    fmt::print("2 passing, fuzzy fails, 2 skipped\n");
    track.dep_check(passing_test,
                    passing_fuzzy,
                    failing_fuzzy,
                    failing_test,
                    passing_test);

    fmt::print(
        "pair of passing, tuple of 2 failing and 2 passing, skipping 1\n");
    track.dep_check(
        std::pair{passing_test, passing_fuzzy},
        std::tuple{failing_fuzzy, failing_test, passing_test, passing_fuzzy},
        passing_test);

    fmt::print("all passing\n");
    track.dep_check(passing_test,
                    std::pair{passing_test, passing_fuzzy},
                    std::pair{passing_test, passing_fuzzy},
                    passing_test);

    fmt::print("14/22 tests must pass\n");
    return track.total() != 22 || track.passed() != 14 || track.ret_code() == 0;
}
