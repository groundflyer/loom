// -*- C++ -*-
// threadpool.cpp
#include <utils/test.hpp>
#include <utils/threadpool.hpp>

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
