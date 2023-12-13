// -*- C++ -*-
/// @file bse.hpp
/// @brief Basic scalar evaluator.

#pragma once

#include "lang.hpp"

#ifndef NDEBUG
#include <chrono>
#include <fmt/core.h>
#include <thread>
#endif

namespace lucid::bse
{
template <typename RegTypes, typename RegNames, typename _CP>
class Memory
{
    using helper = MemoryHelper<RegTypes, RegNames>;

    using Data = helper::data;

    Data m_data;

  public:
    /// @brief Access a variable.
    template <typename Var>
    std::add_lvalue_reference_t<vartype<Var>>
    get() noexcept
    {
        return helper::template get<Var>(m_data);
    }

    /// @brief Access cell pointer.
    std::add_lvalue_reference_t<vartype<_CP>>
    cp() noexcept
    {
        return get<_CP>();
    }

#ifndef NDEBUG
    friend void
    print(const Memory& m, unsigned char a = 10) noexcept
    {
        lucid::for_each([&](const auto& c) noexcept
                        { fmt::print("{:>{}} |", c, a); },
                        m.m_data);
        fmt::print("\n");
    }
#endif
};

template <typename InputVars>
struct input_setter
{
    template <typename _Memory, typename... Args>
    constexpr void
    operator()(_Memory& mem, Args&&... args) const noexcept
    {
        typelist::for_each([&mem]<typename Var>(auto&& arg) constexpr noexcept
                           { mem.template get<Var>() = arg; },
                           InputVars{},
                           std::forward<Args>(args)...);
    }
};

template <typename OutputVars>
struct output_getter
{
    template <typename _Memory>
    constexpr decltype(auto)
    operator()(_Memory& mem) const noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            [&mem]<typename Var>() constexpr noexcept
            { return mem.template get<Var>(); },
            OutputVars{});
    }
};

template <typename _Program>
class basic_scalar_evaluator
{
    using labelmap  = typename _Program::labelmap;
    using cells     = typename _Program::cells;
    using reg_names = typename _Program::reg_names;
    using reg_types = typename _Program::reg_types;
    using cp        = typename _Program::cp;

    static constexpr auto end_pos = _Program::end_pos;

    template <typename C>
    using compile_cell = standard_compiler<C, labelmap>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    static constexpr input_setter<typename _Program::input_vars>   setter{};
    static constexpr output_getter<typename _Program::output_vars> getter{};

    using memory = Memory<reg_types, reg_names, cp>;

    struct single_cell_evaluator
    {
        static constexpr typelist::head<compiled_cells> cell{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            setter(mem, args...);
            cell(mem);
            return getter(mem);
        }
    };

    struct simple_evaluator
    {
        static constexpr typename typelist::to<compiled_cells, std::tuple>
            cells{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            // reset state when calling multiple times
            mem.cp() = 0;
            setter(mem, args...);
            while(mem.cp() != end_pos)
            {
                visit(
                    mem.cp(),
                    [&](auto cell) constexpr noexcept { cell(mem); },
                    cells,
                    sink);
#ifndef NDEBUG
                {
                    constexpr unsigned _w = 10;
                    typelist::for_each(
                        [_w]<typename Name>() noexcept
                        {
                            constexpr std::string_view n =
                                std::string_view(intlist::array<Name>,
                                                 intlist::length<Name>);
                            fmt::print("{:>{}} |", n, _w);
                        },
                        reg_names{});
                    fmt::print("\n");
                    print(mem, _w);
                    typelist::for_each([_w]<typename Name>() noexcept
                                       { fmt::print("{:*>{}} |", '*', _w); },
                                       reg_names{});
                    fmt::print("\n");
                    // using namespace std::chrono_literals;
                    // std::this_thread::sleep_for(1s);
                }
#endif
            }
            return getter(mem);
        }
    };

  public:
    using type = std::conditional_t<_Program::singular,
                                    single_cell_evaluator,
                                    simple_evaluator>;
};

template <typename _Program>
using evaluator = typename basic_scalar_evaluator<_Program>::type;
} // namespace lucid::bse
