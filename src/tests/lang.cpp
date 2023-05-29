// -*- C++ -*-
// lang.cpp

#include <utils/tmp.hpp>
#include <utils/union.hpp>

#include <concepts>
#include <tuple>

using namespace lucid;

namespace tl = typelist;

template <typename To, typename Op, typename... Args>
struct ComputeStore
{
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
};

template <typename Name>
struct Label
{
};

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

namespace lucid::detail
{
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

template <typename Vars>
struct get_var_names;

template <typename... Vars>
struct get_var_names<type_sequence<Vars...>>
{
    using type = type_sequence<typename Vars::name...>;
};

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
                           tl::cons<typename Var::type, VarTypes>,
                           VarTypes>,
        type_sequence<Vars...>>::type;
};

template <typename Name, typename Vars>
class collect_types
{
    using collected =
        tl::nub<typename collect<Name, type_sequence<>, Vars>::type>;

  public:
    using type = std::conditional_t<tl::length<collected> == 1,
                                    tl::head<collected>,
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
    using type = tl::to<type_sequence<Ts...>, lucid::Union>;
};

template <typename T>
class access_wrapper
{
    template <std::same_as<T> T1>
    static constexpr T&
    ref(T& val) noexcept
    {
        return val;
    }
};

template <typename... Ts>
class access_wrapper<Union<Ts...>>
{
    template <typename T>
    static constexpr T&
    ref(Union<Ts...>& un) noexcept
    {
        return un.template ref<T>();
    }
};
} // namespace lucid::detail

template <typename Command>
using get_storing_var = typename detail::get_storing_var<Command>::type;

template <typename Vars>
using get_var_names = typename detail::get_var_names<Vars>::type;

template <typename Vars>
class type_table
{
    template <typename Name>
    using infer_var_type = typename detail::collected_type<
        typename detail::collect_types<Name, Vars>::type>::type;

  public:
    /// @brief Unique variable names.
    using names = tl::nub<get_var_names<Vars>>;

    /// @brief Aggregated unique types of variables we use to hold actual
    /// values.
    ///
    /// std::tuple<types...> is used as the container.
    ///
    /// If there are few different types corresponding to a single variable,
    /// lucid::Union used to hold values of the variable.
    using types = tl::map<infer_var_type, names>;

    /// @brief Variable name to type dictionary.
    using table = tl::zip<names, types>;

    /// @brief Index of a variable in the container.
    template <typename Name>
    static constexpr std::size_t var_index = tl::index<Name, names>;

    /// @brief Type of a variable stored in the container.
    template <typename Name>
    using var_type = tl::lookup<Name, table>;

    /// @brief Class to store values of the variables.
    using container = tl::to<types, std::tuple>;

    /// @brief Access a variable in a container.
    template <typename Var>
    static std::add_lvalue_reference_t<typename Var::type>
    access(container& cont) noexcept
    {
        using var_name = typename Var::name;
        using wrapper  = detail::access_wrapper<tl::lookup<var_name, table>>;
        return wrapper::template ref<var_type<var_name>>(
            std::get<var_index<var_name>>(cont));
    }
};

template <typename Commands, typename TypeTable>
struct compile_instructions
{
};

template <typename StoreConstCommand, typename Table>
struct const_set
{
    using var       = typename StoreConstCommand::var;
    using action    = typename StoreConstCommand::action;
    using container = typename Table::container;

    static constexpr action func{};

    constexpr void
    operator()(container& cont) const noexcept
    {
        Table::template access<var>(cont) = func();
    }
};

template <typename Commands, typename Table>
struct init
{
    using constants = tl::filter<detail::is_store_const, Commands>;

    template <typename Container>
    constexpr void
    operator()(Container& container) noexcept
    {
    }
};

/// @brief Basic program representation.
template <typename InVars, typename OutVars, typename Commands>
struct Program
{
    /// @brief Only commands that store values to a variable.
    using stores = tl::filter<does_store, Commands>;

    /// @brief Variables that have stored values to.
    using vars =
        tl::join<tl::join<InVars, OutVars>, tl::map<get_storing_var, stores>>;

    using vars_info = type_table<vars>;
    using var_names = typename vars_info::names;
    using var_types = typename vars_info::types;
};

#define MK_CONSTANT(TYPE, VALUE) \
    decltype([]() constexpr noexcept -> TYPE { return VALUE; })

template <typename T>
struct Add
{
    constexpr T
    operator()(const T& a, const T& b) const noexcept
    {
        return a + b;
    }
};

template <typename T>
class increment
{
    using val      = Variable<T, "val">;
    using one      = Variable<T, "one">;
    using commands = type_sequence<StoreConstant<one, MK_CONSTANT(T, 1)>,
                                   ComputeStore<val, Add<T>, val, one>>;

    using inputs  = type_sequence<val>;
    using outputs = type_sequence<val>;

  public:
    using program = Program<inputs, outputs, commands>;
};

int main(/*int argc, char* argv[]*/) { return 0; }
