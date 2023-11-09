// -*- C++ -*-
// lang.hpp
//

#pragma once

#include <utils/functional.hpp>
#include <utils/string.hpp>
#include <utils/tmp.hpp>
#include <utils/tuple.hpp>
#include <utils/union.hpp>

#include <cstdint>
#include <limits>

namespace lucid
{
using index_t                    = std::uint32_t;
constexpr inline index_t end_pos = static_cast<index_t>(-1);

template <typename Type, StringLiteral Name>
    requires(std::is_integral_v<Type> || std::is_floating_point_v<Type>)
struct Variable
{
    using type                                 = Type;
    using name                                 = sl2cs<Name>;
    static constexpr std::string_view nameview = Name.view();
};

template <typename _Variable>
using vartype = typename _Variable::type;

template <typename _Variable>
using varname = typename _Variable::name;

template <typename _Variable>
constexpr inline std::string_view varview = _Variable::nameview;

/// @brief Cell Pointer.
using _CP = Variable<index_t, "CP">;

template <typename... _Vars>
inline constexpr bool not_cp =
    !(std::conjunction_v<std::is_same<varname<_CP>, varname<_Vars>>...>);

template <typename Op, typename Dst, typename... Args>
    requires(not_cp<Dst, Args...>)
struct Compute
{
    using var    = Dst;
    using action = Op;
    using args   = type_sequence<Args...>;
};

template <typename Dst, typename Src>
    requires(not_cp<Dst, Src> && std::is_same_v<vartype<Dst>, vartype<Src>>)
struct Copy
{
    using dst = Dst;
    using src = Src;
};

template <StringLiteral Name>
struct Label
{
    using name = sl2cs<Name>;
};

using EndLabel   = Label<"END">;
using StartLabel = Label<"START">;

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

template <typename Var, typename Labels>
    requires(std::is_same_v<vartype<Var>, index_t> &&
             typelist::all<is_label, Labels> && typelist::length<Labels> >= 2)
struct ParmJump
{
    using var    = Var;
    using labels = Labels;
};

template <typename Target>
    requires(is_label<Target>::value)
struct Jump
{
    using target = Target;
};

using EndJump = Jump<EndLabel>;

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
struct does_store<Copy<Ts...>>
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

template <typename Op, typename Dst, typename... Ts>
struct get_storing_var<Compute<Op, Dst, Ts...>>
{
    using type = Dst;
};

template <typename Variable, typename Src>
struct get_storing_var<Copy<Variable, Src>>
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

    template <typename T, typename A>
    constexpr std::span<T>
    operator()(UnionVector<type_sequence<Ts...>, A>& uv) const noexcept
    {
        return uv.template view<T>();
    }

    template <typename T, typename A>
    constexpr std::span<T>
    operator()(const UnionVector<type_sequence<Ts...>, A>& uv) const noexcept
    {
        return uv.template view<T>();
    }
};

template <typename RegTypes,
          typename RegNames,
          template <typename...> typename Container = std::tuple,
          template <typename> typename RegProj      = std::type_identity_t,
          template <typename> typename AccProj      = std::type_identity_t>
struct MemoryHelper
{
    using types = typelist::map<RegProj, RegTypes>;
    using table = typelist::zip<RegNames, types>;
    using data  = typelist::to<types, Container>;

    template <typename Name>
    static constexpr std::size_t regindex = typelist::index<Name, RegNames>;

    template <typename Name>
    using regtype = typelist::lookup<Name, table>;

    template <typename Var>
    using access = access_wrapper<typelist::lookup<varname<Var>, table>>;

    template <typename Var>
    static constexpr decltype(auto)
    get(data& _data) noexcept
    {
        constexpr access<Var> op{};

        return op.template operator()<AccProj<vartype<Var>>>(
            std::get<regindex<varname<Var>>>(_data));
    }
};

template <typename _ParmJump, typename LabelMap>
struct parm_jump_helper;

template <typename Var, typename Labels, typename LabelMap>
struct parm_jump_helper<ParmJump<Var, Labels>, LabelMap>
{
    template <typename L>
        requires(typelist::elem<L, typelist::map<fst_t, LabelMap>>)
    using jump_idx = typelist::lookup<L, LabelMap>;

    using jump_idxs =
        typelist::to_integer_sequence<typelist::map<jump_idx, Labels>>;

    static constexpr auto table = intlist::array<jump_idxs>;
};

template <typename _ParmJump, typename LabelMap>
using pjmp_idxs = typename parm_jump_helper<_ParmJump, LabelMap>::jump_idxs;

template <typename _ParmJump, typename LabelMap>
constexpr inline auto pjmp_arr = parm_jump_helper<_ParmJump, LabelMap>::table;

template <typename _Variable>
consteval std::optional<vartype<_Variable>>
parse_literal() noexcept
{
    constexpr std::string_view s = varview<_Variable>;
    using type                   = vartype<_Variable>;

    if constexpr(std::is_integral_v<vartype<_Variable>>)
    {
        if constexpr(s.starts_with("0x") || s.starts_with("0X"))
            return s2i<type, 16>(s);
        else if constexpr((s.starts_with("0b") || s.starts_with("0B")))
            return s2i<type, 2>(s);
        else
            return s2i<type>(s);
    }
    else
        return s2f<type>(s);
}

template <typename _Variable>
struct is_literal
{
    static constexpr bool value = parse_literal<_Variable>().has_value();
};

template <typename _Variable>
inline constexpr bool is_literal_v = is_literal<_Variable>::value;

template <typename _Variable, typename _Memory>
constexpr decltype(auto)
get_value(_Memory&& mem) noexcept
{
    constexpr auto ov = parse_literal<_Variable>();
    if constexpr(ov.has_value())
        return static_cast<vartype<_Variable>>(ov.value());
    else
        return std::forward<_Memory>(mem).template get<_Variable>();
}

template <typename Command, typename LabelMap>
struct standard_eval;

template <typename Dst, typename Src, typename LabelMap>
struct standard_eval<Copy<Dst, Src>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::forward<_Memory>(mem).template get<Dst>() =
            get_value<Src>(std::forward<_Memory>(mem));
    }
};

template <typename Dst, typename Op, typename... Args, typename LabelMap>
struct standard_eval<Compute<Op, Dst, Args...>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::forward<_Memory>(mem).template get<Dst>() =
            std::invoke(Op{}, get_value<Args>(std::forward<_Memory>(mem))...);
    }
};

template <typename Target, typename LabelMap>
struct standard_eval<Jump<Target>, LabelMap>
{
    using constant = typename typelist::lookup<Target, LabelMap>;
    static constexpr index_t value = constant::value;

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::forward<_Memory>(mem).cp() = value;
    }
};

template <typename Var, typename Labels, typename LabelMap>
struct standard_eval<ParmJump<Var, Labels>, LabelMap>
{
    static constexpr auto table = pjmp_arr<ParmJump<Var, Labels>, LabelMap>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::forward<_Memory>(mem).cp() =
            table[std::forward<_Memory>(mem).template get<Var>()];
    }
};

template <typename Cell,
          typename LabelMap,
          template <typename, typename> typename Eval = standard_eval>
struct standard_compiler
{
    template <typename Command>
    using eval = Eval<Command, LabelMap>;

    using instructions = typelist::map<eval, Cell>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& memory) const noexcept
    {
        typelist::unroll(instructions{}, std::forward<_Memory>(memory));
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

template <typename T>
struct same_size_union
{
    static constexpr bool value = true;
};

template <typename T, typename... Ts>
struct same_size_union<Union<T, Ts...>>
{
    static constexpr bool value = (... && (sizeof(T) == sizeof(Ts)));
};

/// @brief Program representation.
template <typename InputVars, typename Statements, typename OutputVars>
class Program
{
    /// @brief Variables that have stored values to.
    using stated_vars =
        typelist::join<typelist::join<InputVars, OutputVars>,
                       typelist::map<get_storing_var_t,
                                     typelist::filter<does_store, Statements>>>;

    static_assert(!typelist::any<is_literal, stated_vars>);

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
        type_pair<EndLabel, std::integral_constant<index_t, end_pos>>,
        typelist::enumerate<labels, index_t>>;

    static constexpr bool singular = num_cells == 1;

    using variables = std::
        conditional_t<singular, stated_vars, typelist::cons<_CP, stated_vars>>;
    static_assert(typelist::all<same_size_union, variables>);

    /// @brief Names of registers.
    using reg_names = typelist::nub<get_var_names_t<variables>>;

    template <typename Name>
    using infer_reg_type = typename collected_type<
        typename collect_types<Name, variables>::type>::type;

    /// @brief Registers types.
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

#define VAR(TYPE, NAME) Variable<TYPE, #NAME>
#define TVAR(NAME)      VAR(T, NAME)

#define BEGIN(NAME, ...) \
    using NAME = Program < type_sequence<__VA_ARGS__>, type_sequence <
#define END(...) >, type_sequence < __VA_ARGS__ >> ;

#define TBEGIN1(NAME, VAR) \
    using NAME = Program < type_sequence<TVAR(VAR)>, type_sequence <
#define TBEGIN2(NAME, VAR1, VAR2)                                 \
    using NAME = Program < type_sequence<TVAR(VAR1), TVAR(VAR2)>, \
          type_sequence <

#define TEND1(VAR)        >, type_sequence < TVAR(VAR) >> ;
#define TEND2(VAR1, VAR2) >, type_sequence < TVAR(VAR1), TVAR(VAR2) >> ;

#define LABEL(NAME)       Label<#NAME>

#define JMP(TARGET)       Jump<Label<#TARGET>>

#define PJMPN(VAR, ...) \
    ParmJump<Variable<index_t, #VAR>, type_sequence<__VA_ARGS__>>

#define PJMP2(VAR, L1, L2)                                    \
    ParmJump < Variable<index_t, #VAR>, type_pair < LABEL(L1) \
    , LABEL(L2)                                               \
    >>

#define PJMP3(VAR, L1, L2, L3)                                    \
    ParmJump < Variable<index_t, #VAR>, type_sequence < LABEL(L1) \
    , LABEL(L2)                                                   \
    , LABEL(L3)                                                   \
    >>

#define BINOP(OP, TYPE, DST, A, B) \
    Compute<OP<TYPE>,              \
            Variable<TYPE, #DST>,  \
            Variable<TYPE, #A>,    \
            Variable<TYPE, #B>>

#define UNOP(OP, TYPE, DST, VAR) \
    Compute<OP<TYPE>, Variable<TYPE, #DST>, Variable<TYPE, #VAR>>

#define CMP(OP, TYPE, DST, A, B)     \
    Compute<OP<TYPE>,                \
            Variable<index_t, #DST>, \
            Variable<TYPE, #A>,      \
            Variable<TYPE, #B>>

#define ADD(...)  BINOP(std::plus, __VA_ARGS__)
#define SUB(...)  BINOP(std::minus, __VA_ARGS__)
#define MUL(...)  BINOP(std::multiplies, __VA_ARGS__)
#define DIV(...)  BINOP(std::divides, __VA_ARGS__)
#define MOD(...)  BINOP(std::modulus, __VA_ARGS__)
#define NEG(...)  UNOP(std::negate, __VA_ARGS__)

#define EQ(...)   CMP(std::equal_to, __VA_ARGS__)
#define NEQ(...)  CMP(std::not_equal_to, __VA_ARGS__)
#define GT(...)   CMP(std::greater, __VA_ARGS__)
#define LT(...)   CMP(std::less, __VA_ARGS__)
#define GE(...)   CMP(std::greater_equal, __VA_ARGS__)
#define LE(...)   CMP(std::less_equal, __VA_ARGS__)

#define TADD(...) ADD(T, __VA_ARGS__)
#define TSUB(...) SUB(T, __VA_ARGS__)
#define TMUL(...) MUL(T, __VA_ARGS__)
#define TDIV(...) DIV(T, __VA_ARGS__)
#define TMOD(...) MOD(T, __VA_ARGS__)
#define TNEG(...) NEG(T, __VA_ARGS__)

#define TEQ(...)  EQ(T, __VA_ARGS__)
#define TNEQ(...) NEQ(T, __VA_ARGS__)
#define TGT(...)  GT(T, __VA_ARGS__)
#define TLT(...)  LT(T, __VA_ARGS__)
#define TGE(...)  GE(T, __VA_ARGS__)
#define TLE(...)  LE(T, __VA_ARGS__)
} // namespace lucid
