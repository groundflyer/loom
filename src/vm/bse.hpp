// -*- C++ -*-
/// @file bse.hpp
/// @brief Basic scalar evaluator.

#pragma once

#include "core.hpp"

#include <common/debug.hpp>

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
        // no-op when is_straightline
        if constexpr(!K::is_straightline) memory.pc() = reset_val;
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
        // no-op when is_straightline
        if constexpr(!K::is_straightline) return memory.pc();
    }
};

#define MK_CASE(IDX) \
    case IDX: MUSTTAIL return typelist::at<IDX, ccss>::eval(mem)

/// @brief Specialized dispatch implementation.
/// @tparam K kernel
/// @tparam pc current PC
/// @tparam Compiler compiler for basic block
template <Kernel              K,
          typename K::index_t pc,
          template <typename> typename Compiler>
    requires(!allow_throws)
class threaded_dispatch
{
    using blocks  = K::blocks;
    using succs   = K::successors;
    using index_t = K::index_t;

    static constexpr index_t halt_pos = K::halt_pos;

    template <index_t I>
    using f = threaded_dispatch<K, I, Compiler>;

    using spec = sd_helper<K,
                           typelist::at<pc, blocks>,
                           typelist::at<pc, succs>,
                           f,
                           Compiler>;

  public:
    template <typename _Memory>
    static void
    eval(_Memory& mem) noexcept
    {
        if constexpr(pc != halt_pos)
        {
            using ccss = spec::ccss;

            const index_t next_pc = spec::eval_kernel(mem);

            if constexpr(spec::l == 1) MUSTTAIL
            return typelist::head<ccss>::eval(mem);
            else if constexpr(spec::l == 2)
            {
                if(next_pc) MUSTTAIL
                return snd_t<ccss>::eval(mem);
                else MUSTTAIL return fst_t<ccss>::eval(mem);
            }
            else if constexpr(spec::l == 3) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
            }
            else if constexpr(spec::l == 4) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
                MK_CASE(3);
            }
            else if constexpr(spec::l == 5) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
                MK_CASE(3);
                MK_CASE(4);
            }
            else if constexpr(spec::l == 6) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
                MK_CASE(3);
                MK_CASE(4);
                MK_CASE(5);
            }
            else if constexpr(spec::l == 7) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
                MK_CASE(3);
                MK_CASE(4);
                MK_CASE(5);
                MK_CASE(6);
            }
            else if constexpr(spec::l == 8) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
                MK_CASE(3);
                MK_CASE(4);
                MK_CASE(5);
                MK_CASE(6);
                MK_CASE(7);
            }
            else if constexpr(spec::l == 9) switch(next_pc)
            {
                MK_CASE(0);
                MK_CASE(1);
                MK_CASE(2);
                MK_CASE(3);
                MK_CASE(4);
                MK_CASE(5);
                MK_CASE(6);
                MK_CASE(7);
                MK_CASE(8);
            }
            else static_assert(false, "Add more cases");
        }
    }
};

#undef MK_CASE

/// @brief Basic CFG evaluator.
/// @tparam use_tbd use threaded block dispatch.
template <Kernel K, bool use_tbd = true>
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

            if constexpr(use_tbd)
                facade::step(
                    [](auto& mem) noexcept
                    { threaded_dispatch<K, 0, compile_block>::eval(mem); });
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
        conditional_t<K::is_straightline, single_block_evaluator, simple_evaluator>;
};

/// @brief Basic CFG evaluator.
/// @tparam use_tbd use threaded block dispatch.
template <typename K, bool use_tbd = true>
using function = basic_scalar_compiler<K, use_tbd>::function;
} // namespace loom::vm::bse
