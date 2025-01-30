// -*- C++ -*-
// threadpool.cpp
#include <utils/test.hpp>
#include <utils/threadpool.hpp>

using namespace lucid;
using namespace lucid::test;

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
        "thread pool 1"));
    track.check(NamedTest(
        []() noexcept
        {
            bool a = false;
            {
                thread_pool<dynamic_size> p(1, [&a]() noexcept { a = true; });
            }
            return a;
        },
        "thread pool 2"));
    track.check(NamedTest(
        []() noexcept
        {
            constexpr unsigned I = 8;
            unsigned           i = 0;
            {
                thread_pool<I> p([&i]() noexcept { ++i; });
            }
            return i == I;
        },
        "sequential initialization"));
    return track.ret_code();
}
