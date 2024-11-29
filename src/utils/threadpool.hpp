// -*- C++ -*-
/// @file threadpool.hpp
/// Basic thread pool.

#pragma once

#include "tmp.hpp"
#include "tuple.hpp"

#include <thread>
#include <vector>

namespace lucid
{
constexpr inline std::size_t dynamic_size = static_cast<std::size_t>(-1);

template <std::size_t N>
class thread_pool
{
    using threads_t =
        typelist::to<typelist::repeat<N, std::thread>, unique_tuple>;
    threads_t m_threads;

    void
    join_all() noexcept
    {
        lucid::for_each([](std::thread& t) noexcept { t.join(); }, m_threads);
    }

  public:
    thread_pool()                   = delete;
    thread_pool(const thread_pool&) = delete;
    thread_pool&
    operator=(const thread_pool&) = delete;

    template <typename Worker, typename... Args>
    thread_pool(Worker&& worker, Args&&... args) noexcept :
        m_threads(lucid::uniform_init,
                  std::forward<Worker>(worker),
                  std::forward<Args>(args)...)
    {
    }

    thread_pool(thread_pool&& other) noexcept :
        m_threads(std::move(other.m_threads))
    {
    }
    thread_pool&
    operator=(thread_pool&& rhs) noexcept
    {
        join_all();
        m_threads = std::move(rhs.m_threads);
    }

    ~thread_pool() { join_all(); }
};

template <>
class thread_pool<dynamic_size>
{
    std::vector<std::thread> m_threads;

    void
    join_all() noexcept
    {
        for(std::thread& t: m_threads) t.join();
    }

  public:
    thread_pool()                   = delete;
    thread_pool(const thread_pool&) = delete;
    thread_pool&
    operator=(const thread_pool&) = delete;

    template <typename Worker, typename... Args>
    thread_pool(std::size_t n, Worker&& worker, Args&&... args) noexcept
    {
        m_threads.reserve(n);
        for(std::size_t i = 0; i < n; ++i)
            m_threads.emplace_back(std::forward<Worker>(worker),
                                   std::forward<Args>(args)...);
    }

    thread_pool(thread_pool&& other) noexcept :
        m_threads(std::move(other.m_threads))
    {
    }
    thread_pool&
    operator=(thread_pool&& rhs) noexcept
    {
        join_all();
        m_threads = std::move(rhs.m_threads);
        return *this;
    }

    ~thread_pool() { join_all(); }
};
} // namespace lucid
