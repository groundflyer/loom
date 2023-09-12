// -*- C++ -*-
/// @file bse.hpp
/// @brief Basic scalar evaluator.

#pragma once

#include "lang.hpp"

namespace lucid::bse
{
template <typename RegTypes,
          typename RegNames,
          typename CP,
          template <typename...> typename Container = std::tuple>
class Memory
{
    using Data = typelist::to<RegTypes, Container>;

    Data m_data;

    /// @brief Variable name to type dictionary.
    using table = typelist::zip<RegNames, RegTypes>;

    /// @brief Type of a variable stored in the memory.
    template <typename Name>
    using var_type = typelist::lookup<Name, table>;

    /// @brief Index of a variable.
    template <typename Name>
    static constexpr std::size_t var_index = typelist::index<Name, RegNames>;

  public:
    /// @brief Access a variable.
    template <typename Var>
    std::add_lvalue_reference_t<typename Var::type>
    get() noexcept
    {
        using var_name = typename Var::name;
        constexpr access_wrapper<typelist::lookup<var_name, table>> op{};

        return op.template operator()<var_type<var_name>>(
            std::get<var_index<var_name>>(m_data));
    }

    /// @brief Access cell pointer.
    std::add_lvalue_reference_t<typename CP::type>
    cp() noexcept
    {
        return get<CP>();
    }
};

template <typename Command, typename LabelMap>
struct eval_instruction;

template <typename To, typename From, typename LabelMap>
struct eval_instruction<Copy<To, From>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        mem.template get<To>() = mem.template get<From>();
    }
};

template <typename To, typename Constant, typename LabelMap>
struct eval_instruction<Store<To, Constant>, LabelMap>
{
    static constexpr Constant func{};

    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        mem.template get<To>() = func();
    }
};

template <typename To, typename Op, typename... Args, typename LabelMap>
struct eval_instruction<Compute<To, Op, Args...>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        mem.template get<To>() = std::invoke(Op{}, mem.template get<Args>()...);
    }
};

template <typename Target, typename LabelMap>
struct eval_instruction<Jump<Target>, LabelMap>
{
    using constant = typename typelist::lookup<Target, LabelMap>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        memory.cp() = std::invoke(constant{});
    }
};

template <typename Var, typename Labels, typename LabelMap>
struct eval_instruction<ParmJump<Var, Labels>, LabelMap>
{
    template <typename L>
    using jump_idx = typelist::lookup<L, LabelMap>;

    using jump_idxs =
        typelist::to_integer_sequence<typelist::map<jump_idx, Labels>>;

    static constexpr auto table = intlist::array<jump_idxs>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        memory.cp() = table[memory.template get<Var>()];
    }
};

template <typename Cell, typename LabelMap>
struct compile_cell
{
    template <typename Command>
    using compiled = eval_instruction<Command, LabelMap>;

    using instructions = typelist::map<compiled, Cell>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        typelist::unroll(instructions{}, memory);
    }
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
    using vars      = typename _Program::variables;
    using reg_names = typename _Program::reg_names;
    using reg_types = typename _Program::reg_types;
    using cp        = typename _Program::cp;

    template <typename C>
    using compile_cell = compile_cell<C, labelmap>;

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
            setter(mem, args...);
            while(mem.cp() != end_pos)
                visit(
                    mem.cp(),
                    [&](auto&& cell) constexpr noexcept { cell(mem); },
                    cells,
                    sink);
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
