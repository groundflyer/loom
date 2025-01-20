// -*- C++ -*-
/// @file pipeline.hpp
/// Stream pipeline.

#pragma once

#include <utils/common.hpp>

#include <range/v3/view/drop.hpp>

#include <algorithm>
#include <ranges>

namespace lucid
{
template <typename Stream, std::ranges::input_range Input, typename Output>
class StreamPipeline
{
    unsigned     m_dropped;
    Stream&      m_stream;
    const Input& m_input;
    Output       m_output;

  public:
    FORBID_ALL(StreamPipeline);

    StreamPipeline(Stream& stream, const Input& input, Output output) noexcept :
        m_stream(stream), m_input(input), m_output(output)
    {
        m_dropped = m_stream.size();
        m_stream.set_input(std::views::take(m_input, m_dropped));
    }

    void
    step() noexcept
    {
        m_stream();
    }

    void
    sort() noexcept
    {
        m_stream.sort();
    }

    bool
    body_condition() const noexcept
    {
        return m_dropped < m_input.size();
    }

    void
    body_step() noexcept
    {
        auto exs = m_stream.exchange(ranges::views::drop(m_input, m_dropped));
        m_dropped += std::ranges::size(exs);
        std::ranges::copy(exs, m_output);
    }

    void
    tail() noexcept
    {
        while(!m_stream.done())
        {
            step();
            sort();
        }
        std::ranges::copy(m_stream.get_output(), m_output);
    }
};
} // namespace lucid
