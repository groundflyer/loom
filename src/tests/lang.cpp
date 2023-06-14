// -*- C++ -*-
// lang.cpp

#include <utils/tmp.hpp>
#include <utils/union.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <tuple>

using namespace lucid;

namespace tl = typelist;
namespace il = intlist;

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

template <typename Var, typename Labels>
    requires(std::is_integral_v<typename Var::type>)
struct ConditionalJump
{
    using var    = Var;
    using labels = Labels;
};

template <typename Target>
struct AlwaysJump
{
    using target = Target;
};

using End = AlwaysJump<EndLabel>;

template <typename Type, StringLiteral Name>
struct Variable
{
    using type = Type;
    using name = sl2cs<Name>;
};

namespace lucid::detail
{
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
struct is_jump<ConditionalJump<Ts...>>
{
    static constexpr bool value = true;
};

template <typename T>
struct is_cell
{
    static constexpr bool value =
        tl::length<T> >= 2 && is_jump<tl::last<T>>::value;
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

    using type = tl::find<p, List>;
};
} // namespace lucid::detail

template <typename Command>
using get_storing_var = typename detail::get_storing_var<Command>::type;

template <typename Vars>
using get_var_names = typename detail::get_var_names<Vars>::type;

template <typename Command>
inline constexpr bool is_jump = detail::is_jump<Command>::value;

template <std::size_t NMax,
          typename List = type_sequence<std::uint_fast8_t,
                                        std::uint_fast16_t,
                                        std::uint_fast32_t,
                                        std::uint_fast64_t>>
using index_type = typename detail::index_type<NMax, List>::type;

template <typename T>
concept Cell = detail::is_cell<T>::value;

template <typename VarTypes,
          typename VarNames,
          typename VarMap,
          typename CP,
          template <typename...> typename Container = std::tuple>
class Memory
{
    using Data = tl::to<VarTypes, Container>;
    Data m_data;

    /// @brief Type of a variable stored in the memory.
    template <typename Name>
    using var_type = tl::lookup<Name, VarMap>;

    /// @brief Index of a variable.
    template <typename Name>
    static constexpr std::size_t var_index = tl::index<Name, VarNames>;

  public:
    /// @brief Access a variable.
    template <typename Var>
    std::add_lvalue_reference_t<typename Var::type>
    get() noexcept
    {
        using var_name = typename Var::name;
        using wrapper  = detail::access_wrapper<tl::lookup<var_name, VarMap>>;
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
    using infer_var_type = typename detail::collected_type<
        typename detail::collect_types<Name, Vars>::type>::type;

  public:
    /// @brief Cell Pointer variable is always first in @p Vars.
    using cp = tl::head<Vars>;

    /// @brief Unique variable names.
    using names = tl::nub<get_var_names<Vars>>;

    /// @brief Aggregated unique types of variables we use to hold actual
    /// values.
    ///
    /// std::tuple<types...> is used as the memory.
    ///
    /// If there are few different types corresponding to a single variable,
    /// lucid::Union used to hold values of the variable.
    using types = tl::map<infer_var_type, names>;

    /// @brief Variable name to type dictionary.
    using table = tl::zip<names, types>;

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
    using type = typename tl::lookup<Target, LabelMap>::value_type;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        memory.cp() = type::value;
    }
};

template <typename Var, typename Labels, typename LabelMap>
struct instruction<ConditionalJump<Var, Labels>, LabelMap>
{
    template <typename L>
    using jump_idx = tl::lookup<L, LabelMap>;

    using jump_idxs = tl::to_integer_sequence<tl::map<jump_idx, Labels>>;

    static constexpr auto table = il::array<jump_idxs>;

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

    using instructions = tl::map<compiled, C>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        tl::unroll(instructions{}, memory);
    }
};

template <typename StoreConstCommand>
struct set_constant
{
    using var    = typename StoreConstCommand::var;
    using action = typename StoreConstCommand::action;
    static constexpr action func{};

    template <typename Memory>
    constexpr void
    operator()(Memory& mem) const noexcept
    {
        mem.template get<var>() = func();
    }
};

template <typename Commands>
struct const_setter
{
    using instructions =
        tl::map<set_constant, tl::filter<detail::is_store_const, Commands>>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory& memory) const noexcept
    {
        tl::unroll(instructions{}, memory);
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
class SingleCellEvaluator
{
    static constexpr typename _Program::init          init{};
    static constexpr tl::head<typename _Program::ops> op{};
    static constexpr typename _Program::setter        setter{};
    static constexpr typename _Program::getter        getter{};

    typename _Program::memory mem;

  public:
    constexpr SingleCellEvaluator() { init(mem); }

    template <typename... Args>
    constexpr auto
    operator()(const Args&... args) noexcept
    {
        setter(mem, args...);
        op(mem);
        return getter(mem);
    }
};

/// @brief Basic program representation.
template <typename InputVars, typename OutputVars, typename Statements>
class Program
{
    /// @brief Only commands that store values to a variable.
    using stores = tl::filter<detail::does_store, Statements>;

    /// @brief Variables that have stored values to.
    using vars = tl::join<tl::join<InputVars, OutputVars>,
                          tl::map<get_storing_var, stores>>;

    // filter out constant statements
    template <typename C>
    using f        = std::negation<detail::is_store_const<C>>;
    using commands = tl::filter<f, Statements>;

    /// @brief List of lists of commands.
    using cells = tl::split_with<detail::is_label, commands>;

    using labels = tl::filter<detail::is_label, commands>;

    static constexpr std::size_t num_cells   = tl::length<labels>;
    static constexpr bool        is_singular = num_cells == 1;

    using index_t = index_type<num_cells>;

    template <auto i>
    using index_constant =
        std::integral_constant<index_t, static_cast<index_t>(i)>;

    using labels_idxs = std::make_index_sequence<num_cells>;
    using labelmap    = tl::cons<
        type_pair<EndLabel,
                  std::integral_constant<index_t, static_cast<index_t>(-1)>>,
        tl::zip<labels, il::map_t<index_constant, labels_idxs>>>;

    /// @brief Cell Pointer variable.
    using cp = Variable<index_t, "CP">;

    template <typename C>
    using cell_compiler = cell_instructions<C, labelmap>;

    // singlular cell optimizations:
    // don't add cell pointer
    using all_vars = std::conditional_t<is_singular, vars, tl::cons<cp, vars>>;

  public:
    using memory = typename VarTable<all_vars>::memory;
    using init   = const_setter<Statements>;
    using ops    = tl::map<cell_compiler, cells>;
    using setter = input_setter<InputVars>;
    using getter = output_getter<OutputVars>;
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
    using val = Variable<T, "val">;
    using one = Variable<T, "one">;
    using commands =
        type_sequence<StoreConstant<one, std::integral_constant<T, 1>>,
                      ComputeStore<val, std::plus<T>, val, one>>;

    using inputs  = type_sequence<val>;
    using outputs = type_sequence<val>;

  public:
    using program = Program<inputs, outputs, commands>;
    using eval    = SingleCellEvaluator<program>;
};

int main(/*int argc, char* argv[]*/)
{
    typename increment<int>::eval p;
    int                           ret = p(-1);
    return ret;
}
