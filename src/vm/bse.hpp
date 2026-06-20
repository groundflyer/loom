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

    template <typename Block, typename... Args>
    constexpr decltype(auto)
    step(Args&&... args) noexcept
    {
        return Block{}(memory, std::forward<Args>(args)...);
    }

    template <typename Block, typename... Args>
    constexpr decltype(auto)
    step(Block block, Args&&... args) noexcept
    {
        return block(memory, std::forward<Args>(args)...);
    }

    constexpr auto
    pc() const noexcept
    {
        // no-op when singular
        if constexpr(!K::singular) return memory.pc();
    }
};

/// @brief Specialized dispatch implementation.
/// @tparam K kernel
/// @tparam index_t separate entry for K::index_t
/// @tparam block_idx dispatched block index
/// @tparam halt_pos separate entry for K::halt_pos
/// @tparam Compiler compiler for basic block
template <Kernel K,
          typename index_t, // C++ doesn't allow specalization of dependent
                            // template parameters, so we pass index_t,
                            // block_idx and halt_pos separately
          index_t block_idx,
          index_t halt_pos,
          template <typename> typename Compiler>
class specialized_dispatch
{
    using blocks = K::blocks;
    using succs  = K::successors;

    using block       = typelist::at<block_idx, blocks>;
    using block_succs = typelist::at<block_idx, succs>;

    template <std::size_t I>
    using f = specialized_dispatch<K, index_t, I, halt_pos, Compiler>;

    using compiled_succs = intlist::map_t<f, block_succs>;

    static constexpr Compiler<block> eval{};

  public:
    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        const index_t pc = eval(std::forward<_Memory>(mem));

        if constexpr(intlist::length<block_succs> >= 2)
            typelist::unsafe_visit_at(
                pc,
                []<typename D>(_Memory&& mem) noexcept
                { D{}(std::forward<_Memory>(mem)); },
                compiled_succs{},
                std::forward<_Memory>(mem));
        else if constexpr(intlist::length<block_succs> == 1)
            typelist::head<compiled_succs>{}(std::forward<_Memory>(mem));
        else // zero successors;
            return;
    }
};

template <Kernel K,
          typename index_t,
          index_t halt_pos,
          template <typename> typename Compiler>
class specialized_dispatch<K, index_t, halt_pos, halt_pos, Compiler>
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
template <Kernel K, bool use_sd = true>
class basic_scalar_compiler
{
    using labelmap = K::labelmap;
    using blocks   = K::blocks;
    using index_t  = K::index_t;

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
                facade::template step<specialized_dispatch<K,
                                                           index_t,
                                                           0,
                                                           halt_pos,
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
