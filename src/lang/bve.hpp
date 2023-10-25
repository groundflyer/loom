// -*- C++ -*-
/// @file bve.hpp
/// @brief Basic vector evaluator

#pragma once

#include "lang.hpp"

#include <utils/functional.hpp>
#include <utils/tuple.hpp>

#include <range/v3/range/operations.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/filter.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <compare>
#include <ranges>

namespace lucid::bve
{
template <typename T, template <typename> typename Array>
struct array_union
{
    using type = Array<T>;
};

template <template <typename> typename Array, typename... Ts>
struct array_union<Union<Ts...>, Array>
{
    using type = Union<Array<Ts>...>;
};

/// @brief If @p T is Union type the resulting type is Union of arrays,
/// otherwise it's just @p Array of @p T.
template <typename T, template <typename> typename Array>
using array_union_t = typename array_union<T, Array>::type;

template <typename RegTypes, typename RegNames, std::size_t Size>
class Memory
{
    template <typename T>
    using Array = std::array<T, Size>;

    template <typename T>
    using f = array_union_t<T, Array>;

    using helper = MemoryHelper<RegTypes, RegNames, std::tuple, f, f>;

    using Data = helper::data;

    Data m_data;

    class proxy_row
    {
        Data&       m_data;
        std::size_t m_pos;

        constexpr void
        copy_values(proxy_row rhs) noexcept
        {
            lucid::for_each([&](auto& c) noexcept { c[m_pos] = c[rhs.m_pos]; },
                            m_data);
        }

      public:
        proxy_row() = delete;

        constexpr proxy_row(const proxy_row& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos)
        {
        }

        constexpr proxy_row(proxy_row&& other) noexcept :
            m_data(other.m_data), m_pos(other.m_pos)
        {
        }

        constexpr proxy_row(Data& data, const std::size_t pos) noexcept :
            m_data(data), m_pos(pos)
        {
        }

        constexpr proxy_row&
        operator=(const proxy_row& rhs) noexcept
        {
            copy_values(rhs);
            m_pos = rhs.m_pos;

            return *this;
        }

        constexpr proxy_row&
        operator=(proxy_row&& rhs) noexcept
        {
            copy_values(rhs);
            m_pos = rhs.m_pos;

            return *this;
        }

        constexpr bool
        operator==(const proxy_row rhs) const noexcept
        {
            return cp() == rhs.cp();
        }

        constexpr std::strong_ordering
        operator<=>(const proxy_row rhs) const noexcept
        {
            return cp() <=> rhs.cp();
        }

        template <typename Var>
        constexpr std::add_lvalue_reference_t<vartype<Var>>
        get() const noexcept
        {
            return helper::template get<Var>(m_data)[m_pos];
            // using var_name = typename Var::name;
            // constexpr access_wrapper<typelist::lookup<var_name, vec_table>>
            // op;

            // return op.template operator()<vec_type<var_name>>(
            //     std::get<var_index<var_name>>(m_data))[m_pos];
        }

        /// @brief Access cell pointer.
        constexpr std::add_lvalue_reference_t<vartype<_CP>>
        cp() const noexcept
        {
            return get<_CP>();
        }

        friend void
        swap(proxy_row a, proxy_row b) noexcept
        {
            lucid::for_each([&](auto& c) noexcept
                            { std::swap(c[a.m_pos], c[b.m_pos]); },
                            a.m_data);
        }
    };

    class _iterator : public std::contiguous_iterator_tag
    {
        Data*       p_data = nullptr;
        std::size_t m_pos{};

      public:
        using difference_type = std::ptrdiff_t;
        using value_type      = proxy_row;

        constexpr _iterator() noexcept = default;

        constexpr _iterator(Data* data, std::size_t pos) noexcept :
            p_data(data), m_pos(pos)
        {
        }
        constexpr _iterator(const _iterator& other) noexcept :
            p_data(other.p_data), m_pos(other.m_pos)
        {
        }
        constexpr _iterator(_iterator&& other) noexcept :
            p_data(other.p_data), m_pos(other.m_pos)
        {
        }

        constexpr _iterator&
        operator=(const _iterator& rhs) noexcept
        {
            p_data = rhs.p_data;
            m_pos  = rhs.m_pos;

            return *this;
        }

        constexpr _iterator&
        operator=(_iterator&& rhs) noexcept
        {
            p_data = rhs.p_data;
            m_pos  = rhs.m_pos;

            return *this;
        }

        constexpr bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_pos == Size;
        }

        constexpr bool
        operator==(const _iterator& rhs) const noexcept
        {
            return m_pos == rhs.m_pos;
        }

        constexpr std::strong_ordering
        operator<=>(const _iterator& rhs) const noexcept
        {
            return m_pos <=> rhs.m_pos;
        }

        constexpr value_type
        operator*() const noexcept
        {
            return {*p_data, m_pos};
        }

        constexpr _iterator&
        operator++() noexcept
        {
            ++m_pos;
            return *this;
        }

        constexpr _iterator
        operator++(int) noexcept
        {
            _iterator ret(*this);
            ++m_pos;
            return ret;
        }

        constexpr _iterator&
        operator--() noexcept
        {
            --m_pos;
            return *this;
        }

        constexpr _iterator
        operator--(int) noexcept
        {
            _iterator ret(*this);
            --m_pos;
            return ret;
        }

        constexpr _iterator
        operator+(difference_type rhs) const noexcept
        {
            return {p_data, m_pos + rhs};
        }

        constexpr difference_type
        operator+(_iterator rhs) const noexcept
        {
            return m_pos + rhs.m_pos;
        }

        constexpr _iterator&
        operator+=(difference_type rhs) noexcept
        {
            m_pos += rhs;
            return *this;
        }

        constexpr _iterator
        operator-(difference_type rhs) const noexcept
        {
            return {p_data, m_pos - rhs};
        }

        constexpr difference_type
        operator-(_iterator rhs) const noexcept
        {
            return m_pos - rhs.m_pos;
        }

        constexpr _iterator&
        operator-=(difference_type rhs) noexcept
        {
            m_pos -= rhs;
            return *this;
        }

        constexpr value_type
        operator[](difference_type n) const noexcept
        {
            return {*p_data, m_pos + n};
        }

        constexpr std::size_t
        count() const noexcept
        {
            return Size - m_pos;
        }

        friend _iterator
        operator+(difference_type n, _iterator j) noexcept
        {
            return _iterator(j.p_data, n + j.m_pos, j.m_size);
        }
    };

  public:
    using value_type = proxy_row;
    using iterator   = _iterator;

    constexpr proxy_row
    operator[](const std::size_t n) noexcept
    {
        return {m_data, n};
    }

    constexpr iterator
    begin() noexcept
    {
        return {&m_data, 0ul};
    }

    constexpr iterator
    end() noexcept
    {
        return {&m_data, size()};
    }

    constexpr std::size_t
    size() noexcept
    {
        return Size;
    }

    /// @brief Access a column.
    template <typename Var>
    constexpr Array<vartype<Var>>&
    get() noexcept
    {
        return helper::template get<Var>(m_data);
    }

    /// @brief Access cell pointers column.
    constexpr Array<vartype<_CP>>&
    cp() noexcept
    {
        return get<_CP>();
    }

    constexpr friend iterator
    begin(Memory& m) noexcept
    {
        return m.begin();
    }

    constexpr friend std::default_sentinel_t
    end(Memory&) noexcept
    {
        return std::default_sentinel;
    }
};

template <typename T>
struct is_mem
{
    static constexpr bool value = false;
};

template <typename... Ts>
struct is_mem<Memory<Ts...>>
{
    static constexpr bool value = true;
};

template <typename Cell, typename LabelMap>
struct compile_cell
{
    template <typename Command>
    struct eval_instruction;

    template <typename Dst, typename Src>
    struct eval_instruction<Copy<Dst, Src>>
    {
        template <typename _Memory>
        constexpr void
        operator()(_Memory&& mem) const noexcept
        {
            if constexpr(is_mem<_Memory>::value)
                std::ranges::copy(mem.template get<Src>(),
                                  mem.template get<Dst>().begin());
            else
                std::ranges::for_each(std::forward<_Memory>(mem),
                                      [](auto row) constexpr noexcept {
                                          row.template get<Dst>() =
                                              get_value<Src>(row);
                                      });
        }
    };

    template <typename Dst, typename Op, typename... Args>
    struct eval_instruction<Compute<Dst, Op, Args...>>
    {
        template <typename _Memory>
        constexpr void
        operator()(_Memory&& mem) const noexcept
        {
            std::ranges::for_each(std::forward<_Memory>(mem),
                                  [](auto row) constexpr noexcept {
                                      row.template get<Dst>() =
                                          std::invoke(Op{},
                                                      get_value<Args>(row)...);
                                  });
        }
    };

    template <typename Target>
    struct eval_instruction<Jump<Target>>
    {
        using constant = typename typelist::lookup<Target, LabelMap>;
        static constexpr constant f{};

        template <typename _Memory>
        constexpr void
        operator()(_Memory&& mem) const noexcept
        {
            if constexpr(is_mem<_Memory>::value)
                std::ranges::fill(mem.cp(), std::invoke(f));
            else
                std::ranges::for_each(std::forward<_Memory>(mem),
                                      [](auto row) constexpr noexcept
                                      { row.cp() = std::invoke(f); });
        }
    };

    template <typename Var, typename Labels>
    struct eval_instruction<ParmJump<Var, Labels>>
    {
        template <typename L>
        using jump_idx = typelist::lookup<L, LabelMap>;

        using jump_idxs =
            typelist::to_integer_sequence<typelist::map<jump_idx, Labels>>;

        static constexpr auto table = intlist::array<jump_idxs>;

        template <typename _Memory>
        constexpr void
        operator()(_Memory&& mem) const noexcept
        {
            std::ranges::for_each(std::forward<_Memory>(mem),
                                  [](auto row) noexcept {
                                      row.cp() = table[row.template get<Var>()];
                                  });
        }
    };

    using instructions = typelist::map<eval_instruction, Cell>;

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
        typelist::for_each(
            [&mem]<typename Var, typename T>(const T& value) constexpr noexcept
            {
                std::ranges::range auto& column = mem.template get<Var>();
                if constexpr(std::ranges::range<T>)
                    std::ranges::copy(value, column.begin());
                else
                    std::ranges::fill(column, value);
            },
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
            { return std::views::all(mem.template get<Var>()); },
            OutputVars{});
    }
};

template <typename _Program, std::size_t ArraySize>
struct basic_vector_evaluator
{
    using labelmap  = typename _Program::labelmap;
    using cells     = typename _Program::cells;
    using vars      = typename _Program::variables;
    using reg_names = typename _Program::reg_names;
    using reg_types = typename _Program::reg_types;

    template <typename C>
    using compile_cell = compile_cell<C, labelmap>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    static constexpr input_setter<typename _Program::input_vars>   setter{};
    static constexpr output_getter<typename _Program::output_vars> getter{};

    using memory = Memory<reg_types, reg_names, ArraySize>;

    struct single_cell_evaluator
    {
        static constexpr typelist::head<compiled_cells> cell{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(Args&&... args) noexcept
        {
            setter(mem, std::forward<Args>(args)...);
            cell(mem);
            return getter(mem);
        }
    };

    struct evaluator
    {
        static constexpr typename typelist::to<compiled_cells, std::tuple>
            cells{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(Args&&... args) noexcept
        {
            setter(mem, std::forward<Args>(args)...);

            while(mem.cp()[0] != end_pos)
            {
                std::ranges::for_each(
                    ranges::views::filter(mem,
                                          [](auto row) constexpr noexcept
                                          { return row.cp() != end_pos; }) |
                        ranges::views::chunk_by(
                            [](auto a, auto b) constexpr noexcept
                            { return a.cp() == b.cp(); }),
                    [](auto grp) constexpr noexcept
                    {
                        visit(
                            ranges::front(grp).cp(),
                            [&](auto& cell) constexpr noexcept { cell(grp); },
                            cells,
                            sink);
                    });

#ifdef _LIBCPP_VERSION
                std::sort(mem.begin(), mem.end());
#else
                // libstdc++ std::sort doesn't use swap internally
                // so we sort it manually
                for(std::size_t i = 1; i < mem.size(); ++i)
                    for(std::size_t j = i; j > 0 && mem[j - 1] > mem[j]; --j)
                        swap(mem[j], mem[j - 1]);
#endif
            }

            return getter(mem);
        }
    };

  public:
    using type = std::
        conditional_t<_Program::singular, single_cell_evaluator, evaluator>;
};

template <typename _Program, std::size_t ArraySize>
using evaluator = typename basic_vector_evaluator<_Program, ArraySize>::type;
} // namespace lucid::bve
