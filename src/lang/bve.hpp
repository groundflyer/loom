// -*- C++ -*-
/// @file bve.hpp
/// @brief Basic vector evaluator

#pragma once

#include "lang.hpp"

#include <utils/functional.hpp>

#include <algorithm>
#include <compare>
#include <ranges>

namespace lucid::bve
{
template <typename T, template <typename> typename Vector>
struct vector_union
{
    using type = Vector<T>;
};

template <template <typename> typename Vector, typename... Ts>
struct vector_union<Union<Ts...>, Vector>
{
    using type = Union<Vector<Ts>...>;
};

/// @brief If @p T is Union type the resulting type is Union of vectors,
/// otherwise it's just @p Vector of @p T.
template <typename T, template <typename> typename Vector>
using vector_union_t = typename vector_union<T, Vector>::type;

template <typename RegTypes,
          typename RegNames,
          typename CP,
          template <typename>
          typename Vector,
          typename SizeLookupVar                    = CP,
          template <typename...> typename Container = std::tuple>
class Memory
{
    template <typename T>
    using f = vector_union_t<T, Vector>;

    using VecTypes = typelist::map<f, RegTypes>;

    using Data = typelist::to<VecTypes, Container>;

    /// @brief Variable name to column dictionary.
    using vec_table = typelist::zip<RegNames, VecTypes>;

    /// @brief Type of a column/vector.
    template <typename Name>
    using vec_type = typelist::lookup<Name, vec_table>;

    /// @brief Index of a column/vector.
    template <typename Name>
    static constexpr std::size_t var_index = typelist::index<Name, RegNames>;

    Data m_data;

    class proxy_row
    {
        Data*       p_data = nullptr;
        std::size_t m_pos{};

      public:
        proxy_row() = delete;

        constexpr proxy_row(Data* data, const std::size_t pos) noexcept :
            p_data(data), m_pos(pos)
        {
        }

        template <typename Var>
        constexpr std::add_lvalue_reference_t<typename Var::type>
        get() noexcept
        {
            using var_name = typename Var::name;
            constexpr access_wrapper<typelist::lookup<var_name, vec_table>> op;

            return op.template operator()<vec_type<var_name>>(
                std::get<var_index<var_name>>(*p_data))[m_pos];
        }

        /// @brief Access cell pointer.
        constexpr std::add_lvalue_reference_t<typename CP::type>
        cp() noexcept
        {
            return get<CP>();
        }
    };

    class _iterator : public std::contiguous_iterator_tag
    {
        Data*       p_data = nullptr;
        std::size_t m_pos{};
        std::size_t m_size{};

      public:
        using difference_type = std::ptrdiff_t;
        using value_type      = proxy_row;

        constexpr _iterator() noexcept = default;

        constexpr _iterator(Data*       data,
                            std::size_t pos,
                            std::size_t size) noexcept :
            p_data(data),
            m_pos(pos), m_size(size)
        {
        }
        constexpr _iterator(const _iterator& other) noexcept :
            p_data(other.p_data), m_pos(other.m_pos), m_size(other.m_size)
        {
        }
        constexpr _iterator(_iterator&& other) noexcept :
            p_data(other.p_data), m_pos(other.m_pos), m_size(other.m_size)
        {
        }

        constexpr _iterator&
        operator=(const _iterator& rhs) noexcept
        {
            m_pos  = rhs.m_pos;
            m_size = rhs.m_size;
            p_data = rhs.p_data;
        }

        constexpr bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return m_pos == m_size;
        }

        constexpr bool
        operator==(std::default_sentinel_t) noexcept
        {
            return m_pos == m_size;
        }

        constexpr std::strong_ordering
        operator<=>(const _iterator& rhs) const noexcept
        {
            return m_pos <=> rhs.m_pos;
        }

        constexpr std::strong_ordering
        operator<=>(const _iterator& rhs) noexcept
        {
            return m_pos <=> rhs.m_pos;
        }

        constexpr value_type
        operator*() const noexcept
        {
            return proxy_row(p_data, m_pos);
        }

        constexpr value_type
        operator*() noexcept
        {
            return proxy_row(p_data, m_pos);
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
            return _iterator(p_data, m_pos + rhs, m_size);
        }

        constexpr _iterator
        operator+(difference_type rhs) noexcept
        {
            return _iterator(p_data, m_pos + rhs, m_size);
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
            return _iterator(p_data, m_pos - rhs, m_size);
        }

        constexpr _iterator
        operator-(difference_type rhs) noexcept
        {
            return _iterator(p_data, m_pos - rhs, m_size);
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
            return proxy_row(p_data, m_pos + n);
        }

        constexpr value_type
        operator[](difference_type n) noexcept
        {
            return proxy_row(p_data, m_pos + n);
        }

        constexpr std::size_t
        count() const noexcept
        {
            return m_size - m_pos;
        }

        constexpr std::size_t
        count() noexcept
        {
            return m_size - m_pos;
        }
    };

  public:
    using value_type = proxy_row;
    using iterator   = _iterator;

    constexpr proxy_row
    operator[](const std::size_t n) const noexcept
    {
        return proxy_row(&m_data, n);
    }

    constexpr iterator
    begin() const noexcept
    {
        return iterator(&m_data, 0ul, size());
    }

    constexpr iterator
    begin() noexcept
    {
        return iterator(&m_data, 0ul, size());
    }

    constexpr iterator
    cbegin() const noexcept
    {
        return begin();
    }

    constexpr iterator
    end() const noexcept
    {
        return iterator(&m_data, size(), size());
    }

    constexpr iterator
    end() noexcept
    {
        return iterator(&m_data, size(), size());
    }

    constexpr iterator
    cend() const noexcept
    {
        return end();
    }

    constexpr std::size_t
    size() const noexcept
    {
        return column<SizeLookupVar>().size();
    }

    constexpr std::size_t
    size() noexcept
    {
        return column<SizeLookupVar>().size();
    }

    /// @brief Access a column.
    template <typename Var>
    constexpr Vector<typename Var::type>&
    column() noexcept
    {
        using var_name = typename Var::name;
        constexpr access_wrapper<typelist::lookup<var_name, vec_table>> op{};

        return op.template operator()<vec_type<var_name>>(
            std::get<var_index<var_name>>(m_data));
    }

    /// @brief Access cell pointers column.
    constexpr Vector<typename CP::type>&
    cp() noexcept
    {
        return column<CP>();
    }

    /// @brief Access a column.
    template <typename Var>
    constexpr const Vector<typename Var::type>&
    column() const noexcept
    {
        using var_name = typename Var::name;
        constexpr access_wrapper<typelist::lookup<var_name, vec_table>> op{};

        return op.template operator()<vec_type<var_name>>(
            std::get<var_index<var_name>>(m_data));
    }

    /// @brief Access cell pointers column.
    constexpr const Vector<typename CP::type>&
    cp() const noexcept
    {
        return column<CP>();
    }

    constexpr friend iterator
    begin(const Memory& m) noexcept
    {
        return m.begin();
    }

    constexpr friend iterator
    begin(Memory& m) noexcept
    {
        return m.begin();
    }

    constexpr friend std::default_sentinel_t
    end(const Memory&) noexcept
    {
        return std::default_sentinel;
    }

    constexpr friend std::default_sentinel_t
    end(Memory&) noexcept
    {
        return std::default_sentinel;
    }

    constexpr friend std::size_t
    size(const Memory& m) noexcept
    {
        return m.size();
    }

    constexpr friend std::size_t
    size(Memory& m) noexcept
    {
        return m.size();
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
        std::ranges::copy(mem.template column<From>(),
                          mem.template column<To>().begin());
    }
};

template <typename To, typename Constant, typename LabelMap>
struct eval_instruction<Store<To, Constant>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        std::ranges::generate(mem.template column<To>(), Constant{});
    }
};

template <typename To, typename Op, typename... Args, typename LabelMap>
struct eval_instruction<Compute<To, Op, Args...>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        std::ranges::for_each(mem,
                              [](auto row) constexpr noexcept {
                                  row.template get<To>() =
                                      std::invoke(Op{},
                                                  row.template get<Args>()...);
                              });
    }
};

template <typename Target, typename LabelMap>
struct eval_instruction<Jump<Target>, LabelMap>
{
    using constant = typename typelist::lookup<Target, LabelMap>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        std::ranges::fill(mem.cp(), std::invoke(constant{}));
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
    operator()(_Memory& mem) const noexcept
    {
        std::ranges::for_each(
            mem,
            [](auto row) noexcept
            { row.cp() = table[row.template get<Var>()]; },
            mem);
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
        typelist::for_each(
            [&mem]<typename Var, typename T>(const T& value) constexpr noexcept
            {
                std::ranges::range auto& column = mem.template column<Var>();
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
            { return std::views::all(mem.template column<Var>()); },
            OutputVars{});
    }
};

template <typename _Program,
          template <typename>
          typename _Vector,
          typename SizeLookupVar = typename _Program::cp>
class basic_vector_evaluator
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

  public:
    struct type
    {
        static constexpr typelist::head<compiled_cells> cell{};

        using memory = Memory<reg_types, reg_names, cp, _Vector, SizeLookupVar>;
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
};

template <typename _Program,
          template <typename>
          typename _Vector,
          typename SizeLookupVar = typename _Program::cp>
using evaluator =
    typename basic_vector_evaluator<_Program, _Vector, SizeLookupVar>::type;
} // namespace lucid::bve
