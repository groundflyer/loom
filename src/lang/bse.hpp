// -*- C++ -*-
/// @file bse.hpp
/// @brief Basic scalar evaluator.

#pragma once

#include "lang.hpp"

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
};

template <typename InputVars>
struct input_setter
{
    template <typename _Memory, typename... Args>
        requires(typelist::length<InputVars> == sizeof...(Args))
    constexpr void
    operator()(_Memory& mem, Args&&... args) const noexcept
    {
        typelist::for_each_arg(
            [&mem]<typename Var>(auto&& arg) constexpr noexcept
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
    using labelmap  = _Program::labelmap;
    using cells     = _Program::cells;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using cp        = _Program::cp;

    static constexpr auto halt_pos = _Program::halt_pos;

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
        static constexpr typelist::to<compiled_cells, std::tuple> cells{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            // reset state when calling multiple times
            mem.cp() = 0;
            setter(mem, args...);
            while(mem.cp() != halt_pos)
            {
                visit(
                    mem.cp(),
                    [&](auto cell) constexpr noexcept { cell(mem); },
                    cells,
                    sink);
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
using evaluator = basic_scalar_evaluator<_Program>::type;
} // namespace lucid::bse
