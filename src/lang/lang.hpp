// -*- C++ -*-
// lang.hpp
//

#pragma once

#include <utils/functional.hpp>
#include <utils/tmp.hpp>
#include <utils/tuple.hpp>
#include <utils/union.hpp>

#include <cstdint>

namespace lucid
{
using index_t                    = std::uint32_t;
constexpr inline index_t end_pos = static_cast<index_t>(-1);

#define MK_CONSTANT(TYPE, VALUE) \
    decltype([]() constexpr noexcept -> TYPE { return VALUE; })

template <typename To, typename Op, typename... Args>
struct Compute
{
    using var    = To;
    using action = Op;
    using args   = type_sequence<Args...>;
};

template <typename To, typename Constant>
struct Store
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
    requires(std::is_same_v<typename Var::type, index_t>)
struct ParmJump
{
    using var    = Var;
    using labels = Labels;
};

template <typename Target>
struct Jump
{
    using target = Target;
};

using EndJump = Jump<EndLabel>;

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
struct does_store<Compute<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct does_store<Store<Ts...>>
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
struct is_store_const<Store<To, F>>
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
struct is_jump<Jump<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct is_jump<ParmJump<Ts...>>
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
struct get_storing_var<Compute<Variable, Ts...>>
{
    using type = Variable;
};

template <typename Variable, typename F>
struct get_storing_var<Store<Variable, F>>
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

    template <std::same_as<T> T1>
    constexpr const T1&
    operator()(const T1& val) const noexcept
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

    template <typename T>
    constexpr const T&
    operator()(const Union<Ts...>& un) const noexcept
    {
        return un.template ref<T>();
    }
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
                               typelist::append<Jump<next_label>, cell>>;
    };

    template <typename CellIdx>
    using f = typename f_impl<CellIdx>::type;

    using type = typelist::map<f, typelist::enumerate<Cells>>;
};

/// @brief Ensures that every cell ends with a jump.
template <typename Cells, typename Labels>
using ensure_last_jumps_t = typename ensure_last_jumps<Cells, Labels>::type;

/// @brief Program representation.
template <typename InputVars, typename OutputVars, typename Statements>
class Program
{
    /// @brief Variables that have stored values to.
    using stated_vars =
        typelist::join<typelist::join<InputVars, OutputVars>,
                       typelist::map<get_storing_var_t,
                                     typelist::filter<does_store, Statements>>>;

    /// @brief List of lists of commands.
    using cells_statements = typelist::split_with<is_label, Statements>;

    using stated_labels = typelist::filter<is_label, Statements>;

    static constexpr std::size_t num_cells = typelist::length<cells_statements>;

  public:
    using input_vars  = InputVars;
    using output_vars = OutputVars;

    // Ensure that every cell is labeled.
    // Only the first label can be skipped, so we just add special label at the
    // beginning.
    using labels =
        std::conditional_t<num_cells == typelist::length<stated_labels>,
                           stated_labels,
                           typelist::cons<StartLabel, stated_labels>>;

    static_assert(!typelist::has_duplicates<labels>);

    using labelmap = typelist::cons<
        type_pair<EndLabel,
                  std::integral_constant<index_t, static_cast<index_t>(-1)>>,
        typelist::enumerate<labels, index_t>>;

    /// @brief Cell Pointer variable.
    using cp = Variable<index_t, "CP">;

    static constexpr bool singular = num_cells == 1;

    using variables = std::
        conditional_t<singular, stated_vars, typelist::cons<cp, stated_vars>>;

    /// @brief Names of registers.
    using reg_names = typelist::nub<get_var_names_t<variables>>;

    template <typename Name>
    using infer_reg_type = typename collected_type<
        typename collect_types<Name, variables>::type>::type;

    /// @brief Aggregated unique types of registers we use to hold actual
    /// values.
    ///
    /// If there are few different types corresponding to the same name,
    /// lucid::Union used to hold values of the register.
    using reg_types = typelist::map<infer_reg_type, reg_names>;

    // don't check jumps for singular program.
    using cells =
        std::conditional_t<singular,
                           cells_statements,
                           ensure_last_jumps_t<cells_statements, labels>>;
};
} // namespace lucid
