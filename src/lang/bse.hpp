// -*- C++ -*-
/// @file bse.hpp
/// @brief Basic scalar evaluator.

#pragma once

#include "lang.hpp"

namespace lucid::bse
{
template <typename RegTypes, typename RegNames, typename CPVar>
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
    std::add_lvalue_reference_t<vartype<CPVar>>
    cp() noexcept
    {
        return get<CPVar>();
    }
};

template <typename RegTypes,
          typename RegNames,
          typename CPVar,
          typename InputVars,
          typename OutputVars>
struct MemoryFacade
{
    Memory<RegTypes, RegNames, CPVar> memory;

    template <typename... Args>
        requires(typelist::length<InputVars> == sizeof...(Args))
    constexpr void
    set_input(Args&&... args) noexcept
    {
        typelist::for_each_arg([&]<typename Var>(auto&& arg) constexpr noexcept
                               { memory.template get<Var>() = arg; },
                               InputVars{},
                               std::forward<Args>(args)...);
    }

    constexpr auto
    get_output() noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            [&]<typename Var>() constexpr noexcept
            { return memory.template get<Var>(); },
            OutputVars{});
    }

    template <typename Cell>
    constexpr void
    step(Cell cell) noexcept
    {
        cell(memory);
    }
};

template <typename _Program>
class basic_scalar_evaluator
{
    using labelmap    = _Program::labelmap;
    using cells       = _Program::cells;
    using reg_names   = _Program::reg_names;
    using reg_types   = _Program::reg_types;
    using cp          = _Program::cp;
    using input_vars  = _Program::input_vars;
    using output_vars = _Program::output_vars;

    static constexpr auto halt_pos = _Program::halt_pos;

    template <typename C>
    using compile_cell = standard_compiler<C, labelmap>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    using facade =
        MemoryFacade<reg_types, reg_names, cp, input_vars, output_vars>;

    struct single_cell_evaluator : facade
    {
        static constexpr typelist::head<compiled_cells> cell{};

        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            facade::set_input(args...);
            facade::step(cell);
            return facade::get_output();
        }
    };

    struct simple_evaluator : facade
    {
        static constexpr typelist::to<compiled_cells, std::tuple> cells{};

        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            // reset state when calling multiple times
            facade::memory.cp() = 0;
            facade::set_input(args...);
            while(facade::memory.cp() != halt_pos)
                visit_at(
                    facade::memory.cp(),
                    [&](auto cell) constexpr noexcept { facade::step(cell); },
                    cells,
                    sink);

            return facade::get_output();
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
