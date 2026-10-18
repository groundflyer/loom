// -*- C++ -*-
// threadpool.cpp
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
#include <common/threadpool.hpp>

#include <initializer_list>
#include <utility>

using namespace loom;
using namespace loom::test;

struct Tester2
{
    int*        arr;
    std::size_t pos;

    void
    operator()()
    {
        arr[pos] = 1;
    }
};

int
main()
{
    Track track;
    track.check(NamedTest(
        []() noexcept
        {
            bool a = false;
            {
                thread_pool<1> p([&a]() noexcept { a = true; });
            }
            return a;
        },
        "static size, 1 thread"));
    track.check(NamedTest(
        []() noexcept
        {
            bool a = false;
            bool b = false;
            {
                thread_pool<2> p([&a]() noexcept { a = true; },
                                 [&b]() noexcept { b = true; });
            }
            return a && b;
        },
        "static size, 2 threads, unique workers"));
    track.check(NamedTest(
        []() noexcept
        {
            bool a = false;
            {
                thread_pool<dynamic_size> p(1, [&a]() noexcept { a = true; });
            }
            return a;
        },
        "dynamic size, 1 thread"));
    track.check(NamedTest(
        []() noexcept
        {
            int arr[] = {0, 0};
            {
                thread_pool<dynamic_size> p(std::initializer_list<Tester2>{
                    Tester2{arr, 0},
                    Tester2{arr, 1}
                });
            }
            return arr[0] == 1 && arr[1] == 1;
        },
        "dynamic size, 2 threads, unique workers"));
    return track.ret_code();
}
