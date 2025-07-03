// -*- C++ -*-
/// @file threadpool.hpp
/// Basic thread pool.

#pragma once

#include "common.hpp"
#include "tmp.hpp"
#include "tuple.hpp"

#include <concepts>
#include <ranges>
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
        lucid::for_each(m_threads, [](std::thread& t) noexcept { t.join(); });
    }

  public:
    FORBID_DEFAULT(thread_pool);
    FORBID_COPY(thread_pool);

    template <typename... Args, std::regular_invocable<Args...> Worker>
    thread_pool(Worker worker, Args... args) noexcept :
        m_threads(lucid::uniform_init, worker, args...)
    {
    }

    template <std::regular_invocable... Workers>
        requires(sizeof...(Workers) == N)
    thread_pool(Workers&&... workers) noexcept :
        m_threads(std::move(workers)...)
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
    FORBID_DEFAULT(thread_pool);
    FORBID_COPY(thread_pool);

    template <typename... Args, std::regular_invocable<Args...> Worker>
    thread_pool(std::size_t n, Worker worker, Args... args) noexcept
    {
        m_threads.reserve(n);
        for(std::size_t i = 0; i < n; ++i)
            m_threads.emplace_back(worker, args...);
    }

    template <std::ranges::input_range Workers, typename... Args>
        requires(std::regular_invocable<std::ranges::range_value_t<Workers>,
                                        Args...>)
    thread_pool(Workers&& workers, Args... args) noexcept
    {
        std::ranges::for_each(
            std::forward<Workers>(workers),
            [&threads = this->m_threads, args...](auto&& worker)
            { threads.emplace_back(std::move(worker), args...); });
        m_threads.shrink_to_fit();
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
