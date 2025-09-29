// -*- C++ -*-
// stream_wrapper.hpp
//

#pragma once

#include <utils/debug.hpp>
#include <vm/core.hpp>

#include <range/v3/view/subrange.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

template <typename S>
class StreamWrapper
{
    S* const p_stream;

  public:
    StreamWrapper(S* stream) noexcept : p_stream(stream) {}

    template <typename Input>
    auto
    operator()(const Input& input) const noexcept
    {
        // prepare result container
        using program = S::program;
        std::vector<lucid::vm::output_type<program>> out;
        out.reserve(input.size());
        auto inserter = std::back_inserter(out);

        auto step = [s = this->p_stream]() noexcept
        {
            (*s)();
            s->sort();
        };

        // set initial data
        auto i = p_stream->set_inputs(input);
        step();

        // feed all the input data
        while(i != std::ranges::end(input))
        {
            auto exs = p_stream->exchange(
                ranges::subrange(i, std::ranges::end(input)));
            i += std::ranges::size(exs);
            std::ranges::copy(exs, inserter);
            step();
        }

        // compute the rest
        while(!p_stream->done()) step();

        std::ranges::copy(p_stream->get_output_zipped(), inserter);

        ASSERT(out.size() == input.size(), "Output size doesn't match");

        return out;
    }
};
