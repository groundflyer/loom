// -*- C++ -*-
/// @file bse.hpp
/// @brief Basic scalar evaluator.

#pragma once

#include "core.hpp"

namespace loom::vm::bse
{
template <typename RegTypes, typename RegNames, typename PCVar>
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

    /// @brief Access program counter.
    std::add_lvalue_reference_t<vartype<PCVar>>
    pc() noexcept
    {
        return get<PCVar>();
    }

    /// @brief Access a variable.
    template <typename Var>
    std::add_lvalue_reference_t<std::add_const_t<vartype<Var>>>
    get() const noexcept
    {
        return helper::template get<Var>(m_data);
    }

    /// @brief Access program counter.
    std::add_lvalue_reference_t<std::add_const_t<vartype<PCVar>>>
    pc() const noexcept
    {
        return get<PCVar>();
    }
};

template <Kernel K,
          template <typename, typename, typename> typename _Memory = Memory,
          typename GetResetVal =
              decltype([]() constexpr noexcept -> K::index_t { return 0; })>
class MemoryFacade
{
    using reg_names   = K::reg_names;
    using reg_types   = K::reg_types;
    using pc_var      = K::pc;
    using input_vars  = K::input_vars;
    using output_vars = K::output_vars;
    using index_t     = K::index_t;

    static constexpr auto reset_val = GetResetVal{}();

    _Memory<reg_types, reg_names, pc_var> memory;

    void
    reset() noexcept
    {
        // no-op when singular
        if constexpr(!K::singular) memory.pc() = reset_val;
    }

  public:
    template <typename... Args>
        requires(typelist::length<input_vars> == sizeof...(Args))
    constexpr void
    set_inputs(Args&&... args) noexcept
    {
        typelist::for_each_arg(
            input_vars{},
            [&]<typename Var>(auto&& arg) constexpr noexcept
            { memory.template get<Var>() = arg; },
            std::forward<Args>(args)...);
        reset();
    }

    constexpr auto
    get_outputs() const noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            output_vars{},
            [&mem = memory]<typename Var>() constexpr noexcept
            { return mem.template get<Var>(); });
    }

    template <typename Block>
    constexpr void
    step(Block block) noexcept
    {
        block(memory);
    }

    constexpr auto
    pc() const noexcept
    {
        // no-op when singular
        if constexpr(!K::singular) return memory.pc();
    }
};

template <typename K>
class basic_scalar_compiler
{
    using labelmap = K::labelmap;
    using blocks   = K::blocks;

    static constexpr auto halt_pos = K::halt_pos;

    template <typename C>
    using compile_block = standard_compiler<C, labelmap>;

    using compiled_blocks = typelist::map<compile_block, blocks>;

    using facade = MemoryFacade<K>;

    class single_block_evaluator : facade
    {
        static constexpr typelist::head<compiled_blocks> block{};

      public:
        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            facade::set_inputs(args...);
            facade::step(block);
            return facade::get_outputs();
        }
    };

    class simple_evaluator : facade
    {
        static constexpr typelist::to<compiled_blocks, std::tuple> blocks{};

      public:
        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            facade::set_inputs(args...);
            while(facade::pc() != halt_pos)
                visit_at(
                    facade::pc(),
                    [&](auto block) constexpr noexcept { facade::step(block); },
                    blocks,
                    sink);

            return facade::get_outputs();
        }
    };

  public:
    using function = std::
        conditional_t<K::singular, single_block_evaluator, simple_evaluator>;
};

template <typename K>
using function = basic_scalar_compiler<K>::function;
} // namespace loom::vm::bse
