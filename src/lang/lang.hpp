// -*- C++ -*-
// lang.hpp
//

#pragma once

#include <utils/functional.hpp>
#include <utils/tmp.hpp>
#include <utils/tuple.hpp>
#include <utils/union.hpp>

#include <cstdint>
#include <limits>

namespace lucid
{
template <typename To, typename Op, typename... Args>
struct ComputeStore
{
    using var    = To;
    using action = Op;
    using args   = type_sequence<Args...>;
};

template <typename To, typename Constant>
struct StoreConstant
{
    using var    = To;
    using action = Constant;
};

template <typename To, typename From>
struct Copy
{
    using to   = To;
    using from = From;
};

template <StringLiteral Name>
struct Label
{
    using name = sl2cs<Name>;
};

struct EndLabel
{
};

struct StartLabel
{
};

template <typename Var, typename Labels>
    requires(std::is_integral_v<typename Var::type>)
struct Jump
{
    using var    = Var;
    using labels = Labels;
};

template <typename Target>
struct AlwaysJump
{
    using target = Target;
};

using EndJump = AlwaysJump<EndLabel>;

template <typename Type, StringLiteral Name>
struct Variable
{
    using type = Type;
    using name = sl2cs<Name>;
};

template <typename T>
struct does_store
{
    static constexpr bool value = false;
};

template <typename... Ts>
struct does_store<ComputeStore<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct does_store<StoreConstant<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct does_store<Copy<Ts...>>
{
    static constexpr bool value = true;
};

template <typename T>
struct is_store_const
{
    static constexpr bool value = false;
};

template <typename To, typename F>
struct is_store_const<StoreConstant<To, F>>
{
    static constexpr bool value = true;
};

template <typename T>
struct is_label
{
    static constexpr bool value = false;
};

template <StringLiteral s>
struct is_label<Label<s>>
{
    static constexpr bool value = true;
};

template <>
struct is_label<EndLabel>
{
    static constexpr bool value = true;
};

template <>
struct is_label<StartLabel>
{
    static constexpr bool value = true;
};

template <typename T>
struct is_jump
{
    static constexpr bool value = false;
};

template <typename... Ts>
struct is_jump<AlwaysJump<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct is_jump<Jump<Ts...>>
{
    static constexpr bool value = true;
};

template <typename T>
struct is_cell
{
    static constexpr bool value =
        typelist::length<T> >= 2 && is_jump<typelist::last<T>>::value;
};

template <typename T>
struct get_storing_var;

template <typename Variable, typename... Ts>
struct get_storing_var<ComputeStore<Variable, Ts...>>
{
    using type = Variable;
};

template <typename Variable, typename F>
struct get_storing_var<StoreConstant<Variable, F>>
{
    using type = Variable;
};

template <typename Variable, typename From>
struct get_storing_var<Copy<Variable, From>>
{
    using type = Variable;
};

template <typename Command>
using get_storing_var_t = typename get_storing_var<Command>::type;

template <typename Vars>
struct get_var_names;

template <typename... Vars>
struct get_var_names<type_sequence<Vars...>>
{
    using type = type_sequence<typename Vars::name...>;
};

template <typename Vars>
using get_var_names_t = typename get_var_names<Vars>::type;

template <typename Name, typename VarTypes, typename Vars>
struct collect;

template <typename Name, typename VarTypes>
struct collect<Name, VarTypes, type_sequence<>>
{
    using type = VarTypes;
};

template <typename Name, typename VarTypes, typename Var, typename... Vars>
struct collect<Name, VarTypes, type_sequence<Var, Vars...>>
{
    using type = typename collect<
        Name,
        std::conditional_t<std::is_same_v<Name, typename Var::name>,
                           typelist::cons<typename Var::type, VarTypes>,
                           VarTypes>,
        type_sequence<Vars...>>::type;
};

template <typename Name, typename Vars>
class collect_types
{
    using collected =
        typelist::nub<typename collect<Name, type_sequence<>, Vars>::type>;

  public:
    using type = std::conditional_t<typelist::length<collected> == 1,
                                    typelist::head<collected>,
                                    collected>;
};

template <typename VarType>
struct collected_type
{
    using type = VarType;
};

template <typename... Ts>
struct collected_type<type_sequence<Ts...>>
{
    using type = typelist::to<type_sequence<Ts...>, lucid::Union>;
};

template <typename T>
struct access_wrapper
{
    template <std::same_as<T> T1>
    constexpr T1&
    operator()(T1& val) const noexcept
    {
        return val;
    }
};

template <typename... Ts>
struct access_wrapper<Union<Ts...>>
{
    template <typename T>
    constexpr T&
    operator()(Union<Ts...>& un) const noexcept
    {
        return un.template ref<T>();
    }
};

template <std::size_t NMax, typename List>
struct index_type
{
    template <typename T>
    struct p
    {
        static constexpr bool value = NMax < std::numeric_limits<T>::max();
    };

    using type = typelist::find<p, List>;
};

template <typename Cells, typename Labels>
struct ensure_last_jumps
{
    template <typename CellIdx>
    struct f_impl
    {
        using cell = fst_t<CellIdx>;

        // is either label of the next cell or if there is none ending label
        using next_label =
            choice_t<typelist::at<(snd_t<CellIdx>::value + 1), Labels>,
                     EndLabel>;

        using type =
            std::conditional_t<is_jump<typelist::last<cell>>::value,
                               cell,
                               typelist::append<AlwaysJump<next_label>, cell>>;
    };

    template <typename CellIdx>
    using f = typename f_impl<CellIdx>::type;

    using type = typelist::map<f, typelist::enumerate<Cells>>;
};

/// @brief Ensures that every cell ends with a jump.
template <typename Cells, typename Labels>
using ensure_last_jumps_t = typename ensure_last_jumps<Cells, Labels>::type;

/// @brief Selects the smallest unsinged integer type that can hold at least @p
/// NMax value.
template <std::size_t NMax,
          typename List = type_sequence<std::uint_fast8_t,
                                        std::uint_fast16_t,
                                        std::uint_fast32_t,
                                        std::uint_fast64_t>>
using index_type_t = typename index_type<NMax, List>::type;

template <typename VarTypes,
          typename VarNames,
          typename VarMap,
          typename CP,
          template <typename...> typename Container = std::tuple>
class Memory
{
    using Data = typelist::to<VarTypes, Container>;
    Data m_data;

    /// @brief Type of a variable stored in the memory.
    template <typename Name>
    using var_type = typelist::lookup<Name, VarMap>;

    /// @brief Index of a variable.
    template <typename Name>
    static constexpr std::size_t var_index = typelist::index<Name, VarNames>;

  public:
    /// @brief Access a variable.
    template <typename Var>
    std::add_lvalue_reference_t<typename Var::type>
    get() noexcept
    {
        using var_name = typename Var::name;
        using wrapper  = access_wrapper<typelist::lookup<var_name, VarMap>>;
        constexpr wrapper  op;
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

template <typename Vars>
class VarTable
{
    template <typename Name>
    using infer_var_type =
        typename collected_type<typename collect_types<Name, Vars>::type>::type;

  public:
    /// @brief Cell Pointer variable is always first in @p Vars.
    using cp = typelist::head<Vars>;

    /// @brief Unique variable names.
    using names = typelist::nub<get_var_names_t<Vars>>;

    /// @brief Aggregated unique types of variables we use to hold actual
    /// values.
    ///
    /// std::tuple<types...> is used as the memory.
    ///
    /// If there are few different types corresponding to a single variable,
    /// lucid::Union used to hold values of the variable.
    using types = typelist::map<infer_var_type, names>;

    /// @brief Variable name to type dictionary.
    using table = typelist::zip<names, types>;

    /// @brief Class to store values of the variables.
    using memory = Memory<types, names, table, cp>;
};

template <typename Command, typename LabelMap>
struct instruction;

template <typename To, typename From, typename LabelMap>
struct instruction<Copy<To, From>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        mem.template get<To>() = mem.template get<From>();
    }
};

template <typename To, typename Constant, typename LabelMap>
struct instruction<StoreConstant<To, Constant>, LabelMap>
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
struct instruction<ComputeStore<To, Op, Args...>, LabelMap>
{
    static constexpr Op op{};

    template <typename _Memory>
    constexpr void
    operator()(_Memory& mem) const noexcept
    {
        mem.template get<To>() = std::invoke(op, mem.template get<Args>()...);
    }
};

template <typename Target, typename LabelMap>
struct instruction<AlwaysJump<Target>, LabelMap>
{
    using constant = typename typelist::lookup<Target, LabelMap>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        memory.cp() = constant::value;
    }
};

template <typename Var, typename Labels, typename LabelMap>
struct instruction<Jump<Var, Labels>, LabelMap>
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

template <typename C, typename LabelMap>
struct cell_instructions
{
    template <typename Command>
    using compiled = instruction<Command, LabelMap>;

    using instructions = typelist::map<compiled, C>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        typelist::unroll(instructions{}, memory);
    }
};

template <typename InputVars>
struct input_setter;

template <typename... InputVars>
struct input_setter<type_sequence<InputVars...>>
{
    template <typename _Memory, typename... Args>
    constexpr void
    operator()(_Memory& mem, const Args&... args) const noexcept
    {
        const auto setter =
            [&mem]<typename Var>(const auto& arg) constexpr noexcept
        { mem.template get<Var>() = arg; };
        (..., setter.template operator()<InputVars>(args));
    }
};

template <typename OutputVars>
struct output_getter;

template <typename OutputVar>
struct output_getter<type_sequence<OutputVar>>
{
    template <typename _Memory>
    constexpr auto
    operator()(_Memory& mem) const noexcept
    {
        return mem.template get<OutputVar>();
    }
};

template <typename... OutputVars>
struct output_getter<type_sequence<OutputVars...>>
{
    template <typename _Memory>
    constexpr auto
    operator()(_Memory& mem) const noexcept
    {
        return std::tuple{mem.template get<OutputVars>()...};
    }
};

template <typename _Program>
struct SingleCellEvaluator
{
    static constexpr typelist::head<typename _Program::cells> cell{};
    static constexpr typename _Program::setter                setter{};
    static constexpr typename _Program::getter                getter{};

    typename _Program::memory mem;

    template <typename... Args>
    constexpr auto
    operator()(const Args&... args) noexcept
    {
        setter(mem, args...);
        cell(mem);
        return getter(mem);
    }
};

template <typename _Program>
struct SimpleEvaluator
{
    static constexpr typename typelist::to<typename _Program::cells, std::tuple>
                                               cells{};
    static constexpr typename _Program::setter setter{};
    static constexpr typename _Program::getter getter{};

    using _Memory = typename _Program::memory;
    using index_t = typename _Program::index_t;

    _Memory mem;

    template <typename... Args>
    constexpr auto
    operator()(const Args&... args) noexcept
    {
        setter(mem, args...);
        while(mem.cp() != static_cast<index_t>(-1))
            visit(
                mem.cp(),
                [&](auto&& cell) constexpr noexcept { cell(mem); },
                cells,
                sink);
        return getter(mem);
    }
};

template <typename _Program>
using SequentialEvaluator = std::conditional_t<_Program::is_singular,
                                               SingleCellEvaluator<_Program>,
                                               SimpleEvaluator<_Program>>;

/// @brief Basic program representation.
template <typename InputVars, typename OutputVars, typename Statements>
struct Program
{
    /// @brief Only commands that store values to a variable.
    using stores = typelist::filter<does_store, Statements>;

    /// @brief Variables that have stored values to.
    using vars = typelist::join<typelist::join<InputVars, OutputVars>,
                                typelist::map<get_storing_var_t, stores>>;

    /// @brief List of lists of commands.
    using cells_statements = typelist::split_with<is_label, Statements>;

    using stated_labels = typelist::filter<is_label, Statements>;

    static constexpr std::size_t num_cells = typelist::length<cells_statements>;

    // Ensure that every cell is labeled.
    // Only the first label can be skipped, so we just add special label at the
    // beginning.
    using labels =
        std::conditional_t<num_cells == typelist::length<stated_labels>,
                           stated_labels,
                           typelist::cons<StartLabel, stated_labels>>;

    static_assert(!typelist::has_duplicates<labels>);

    using index_t = std::uint32_t;

    template <auto i>
    using index_constant =
        std::integral_constant<index_t, static_cast<index_t>(i)>;

    using labelmap = typelist::cons<
        type_pair<EndLabel,
                  std::integral_constant<index_t, static_cast<index_t>(-1)>>,
        typelist::enumerate<labels, index_t>>;

    /// @brief Cell Pointer variable.
    using cp = Variable<index_t, "CP">;

    template <typename C>
    using cell_compiler = cell_instructions<C, labelmap>;

    static constexpr bool is_singular = num_cells == 1;

    // singlular cell optimizations:
    // don't add cell pointer
    using all_vars =
        std::conditional_t<is_singular, vars, typelist::cons<cp, vars>>;
    // don't check jumps
    using cells = typelist::map<
        cell_compiler,
        std::conditional_t<is_singular,
                           cells_statements,
                           ensure_last_jumps_t<cells_statements, labels>>>;
    using memory = typename VarTable<all_vars>::memory;
    using setter = input_setter<InputVars>;
    using getter = output_getter<OutputVars>;
};

#define MK_CONSTANT(TYPE, VALUE) \
    decltype([]() constexpr noexcept -> TYPE { return VALUE; })
} // namespace lucid
