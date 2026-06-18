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
    constexpr decltype(auto)
    step() noexcept
    {
        return Block{}(memory);
    }

    template <typename Block>
    constexpr decltype(auto)
    step(Block block) noexcept
    {
        return block(memory);
    }

    constexpr auto
    pc() const noexcept
    {
        // no-op when singular
        if constexpr(!K::singular) return memory.pc();
    }
};

template <std::unsigned_integral IndexType,
          IndexType              halt_pos,
          IndexType              block_idx,
          typename Blocks,
          typename Successors,
          template <typename> typename Compiler>
class specialized_dispatch
{
    using successors = typelist::at<block_idx, Successors>;
    using block      = typelist::at<block_idx, Blocks>;

    template <IndexType I>
    using f = specialized_dispatch<IndexType,
                                   halt_pos,
                                   I,
                                   Blocks,
                                   Successors,
                                   Compiler>;

    using compiled_successors = intlist::map_t<f, successors>;

    static constexpr Compiler<block> eval{};

  public:
    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        const auto pc = eval(std::forward<_Memory>(mem));

        if constexpr(intlist::length<successors> >= 2)
            typelist::visit_at(
                pc,
                []<typename D>(_Memory&& mem) noexcept
                { D{}(std::forward<_Memory>(mem)); },
                compiled_successors{},
                sink,
                std::forward<_Memory>(mem));
        else if constexpr(intlist::length<successors> == 1)
            typelist::head<compiled_successors>{}(std::forward<_Memory>(mem));
        else // zero successors;
            return;
    }
};

template <std::unsigned_integral IndexType,
          IndexType              halt_pos,
          typename Blocks,
          typename Successors,
          template <typename> typename Compiler>
class specialized_dispatch<IndexType,
                           halt_pos,
                           halt_pos,
                           Blocks,
                           Successors,
                           Compiler>
{
  public:
    template <typename _Memory>
    constexpr void
    operator()(_Memory&& /* mem */) const noexcept
    {
    }
};

/// @brief Basic CFG evaluator.
/// @tparam use_sd use specialized dispatch.
template <typename K, bool use_sd = true>
class basic_scalar_compiler
{
    using labelmap = K::labelmap;
    using blocks   = K::blocks;
    using index_t  = K::index_t;
    using succs    = K::successors;

    static constexpr index_t halt_pos = K::halt_pos;

    template <typename C>
    using compile_block = standard_compiler<C, labelmap>;

    using compiled_blocks = typelist::map<compile_block, blocks>;

    using facade = MemoryFacade<K>;

    struct single_block_evaluator : private facade
    {
        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            facade::set_inputs(args...);
            facade::template step<typelist::head<compiled_blocks>>();
            return facade::get_outputs();
        }
    };

    struct simple_evaluator : private facade
    {
        template <typename... Args>
        constexpr auto
        operator()(const Args&... args) noexcept
        {
            facade::set_inputs(args...);

            if constexpr(use_sd)
                facade::template step<specialized_dispatch<index_t,
                                                           halt_pos,
                                                           0,
                                                           blocks,
                                                           succs,
                                                           compile_block>>();
            else
                while(facade::pc() != halt_pos)
                    typelist::visit_at(
                        facade::pc(),
                        [&]<typename Block>() constexpr noexcept
                        { facade::template step<Block>(); },
                        compiled_blocks{},
                        sink);

            return facade::get_outputs();
        }
    };

  public:
    using function = std::
        conditional_t<K::singular, single_block_evaluator, simple_evaluator>;
};

/// @brief Basic CFG evaluator.
/// @tparam use_sd use specialized dispatch.
template <typename K, bool use_sd = true>
using function = basic_scalar_compiler<K, use_sd>::function;
} // namespace loom::vm::bse
