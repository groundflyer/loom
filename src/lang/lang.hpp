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

namespace lucid
{
/// @brief Describes a variable.
///
/// @tparam Type must be of fundamental type or SIMD type.
/// @tparam Name must be char_sequence.
template <typename Type, typename Name>
struct Variable
{
    using type                                 = Type;
    using name                                 = Name;
    static constexpr std::string_view nameview = cs2sv<Name>;
};

/// @brief Get type of a Variable.
template <typename _Variable>
using vartype = _Variable::type;

/// @brief Get name of a Variable as char_sequence.
template <typename _Variable>
using varname = _Variable::name;

/// @brief Get name of a Variable as std::string_view.
template <typename _Variable>
constexpr inline std::string_view varview = _Variable::nameview;

/// @brief Name reserved for Cell Pointer register.
inline constexpr char _cp_char[] = "CP";

/// @brief char_sequence for _cp_char.
using _cp_name = sl2cs<_cp_char>;

/// @brief Checks that @p _Vars not named _cp_name.
template <typename... _Vars>
inline constexpr bool not_cp =
    !(std::conjunction_v<std::is_same<_cp_name, varname<_Vars>>...>);

/// @brief Describes a computation operation.
///
/// @tparam Op functional object class that performs actual operation.
/// @tparam Dst variable result written to.
/// @tparam Args variables which values used as parameters to @p Op.
template <typename Op, typename Dst, typename... Args>
    requires(not_cp<Dst, Args...> &&
             std::is_invocable_r_v<vartype<Dst>, Op, vartype<Args>...>)
struct Compute
{
    using var    = Dst;
    using action = Op;
    using args   = type_sequence<Args...>;
};

/// @brief Describes a label.
///
/// @tparam Name name of the label, char_sequence.
template <typename Name>
struct Label
{
    using name = Name;
};

/// @brief Special Label jump to halts program.
using HaltLabel = Label<sl2cs<"HALT">>;

/// @brief Special Label jump to starts program from the beginning.
using StartLabel = Label<sl2cs<"START">>;

/// @brief Checks if @p T is Label.
template <typename T>
struct is_label
{
    static constexpr bool value = false;
};

template <typename S>
struct is_label<Label<S>>
{
    static constexpr bool value = true;
};

/// @brief Describes a multiway conditional jump.
///
/// A label position in @p Labels corresponds to the value that @p Var must hold
/// to jump to the label.
///
/// @tparam Var variable that holds integral value.
/// @tparam Labels list of labels.
template <typename Var, typename Labels>
    requires(typelist::all<is_label, Labels> && typelist::length<Labels> >= 2)
struct ParmJump
{
    using var    = Var;
    using labels = Labels;
};

/// @brief Describes non-conditional jump.
///
/// @tparam Target Label to jump to.
template <typename Target>
    requires(is_label<Target>::value)
struct Jump
{
    using target = Target;
};

/// @brief Check if @p T is Compute command.
template <typename T>
struct is_compute
{
    static constexpr bool value = false;
};

template <typename... Ts>
struct is_compute<Compute<Ts...>>
{
    static constexpr bool value = true;
};

/// @brief Check if @p T is Jump command.
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

/// @brief Get the variable @p Instruction stores result to.
template <typename Instruction, typename CPVar>
struct stored_var;

template <typename CPVar, typename Op, typename Dst, typename... Ts>
struct stored_var<Compute<Op, Dst, Ts...>, CPVar>
{
    using type = Dst;
};

template <typename CPVar, typename Target>
struct stored_var<Jump<Target>, CPVar>
{
    using type = CPVar;
};

template <typename CPVar, typename Var, typename... Labels>
struct stored_var<ParmJump<Var, Labels...>, CPVar>
{
    using type = CPVar;
};

template <typename Instruction, typename CPVar>
using stored_var_t = stored_var<Instruction, CPVar>::type;

/// @brief Get list of variables used in @p Instruction.
template <typename Instruction, typename CPVar>
struct ins_vars;

template <typename CPVar, typename Op, typename Dst, typename... Args>
struct ins_vars<Compute<Op, Dst, Args...>, CPVar>
{
    using type = type_sequence<Dst, Args...>;
};

template <typename CPVar, typename Target>
struct ins_vars<Jump<Target>, CPVar>
{
    using type = type_sequence<CPVar>;
};

template <typename CPVar, typename Var, typename... Labels>
struct ins_vars<ParmJump<Var, Labels...>, CPVar>
{
    using type = type_sequence<CPVar, Var>;
};

/// @brief Get all variables used in @p Instruction.
template <typename Instruction, typename CPVar>
using ins_vars_t = ins_vars<Instruction, CPVar>::type;

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
    using type =
        collect<Name,
                std::conditional_t<std::is_same_v<Name, varname<Var>>,
                                   typelist::cons<vartype<Var>, VarTypes>,
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

template <typename Name, typename Vars>
using collect_types_t = collect_types<Name, Vars>::type;

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

/// @brief Infers register types merging variables with same names into unions.
template <typename Vars, typename Names>
struct infer_reg_types
{
    template <typename Name>
    using f = collected_type<collect_types_t<Name, Vars>>::type;

    using type = typelist::map<f, Names>;
};

template <typename Vars, typename Names>
using infer_reg_types_t = infer_reg_types<Vars, Names>::type;

template <typename Cell, typename CPVar>
struct cell_vars
{
    template <typename C>
    using f = ins_vars_t<C, CPVar>;

    using type = typelist::nub<typelist::concat<typelist::map<f, Cell>>>;
};

template <typename Cell, typename CPVar>
using cell_vars_t = cell_vars<Cell, CPVar>::type;

template <typename T>
struct access_wrapper
{
    template <std::same_as<T> T1, typename Val>
    constexpr Val&
    operator()(Val& val) const noexcept
    {
        return val;
    }

    template <std::same_as<T> T1, typename Val>
    constexpr const Val
    operator()(const Val& val) const noexcept
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
          template <typename> typename RegProj      = std::type_identity_t>
    requires(typelist::length<RegTypes> == typelist::length<RegNames>)
struct MemoryHelper
{
    using table = typelist::zip<RegNames, RegTypes>;
    using data  = typelist::to<typelist::map<RegProj, RegTypes>, Container>;

    template <typename Name>
    static constexpr std::size_t regindex = typelist::index<Name, RegNames>;

    template <typename Var>
    using reg_access = access_wrapper<typelist::lookup<varname<Var>, table>>;

    template <typename Var>
    static constexpr decltype(auto)
    get(data& _data) noexcept
    {
        constexpr reg_access<Var> op{};

        return op.template operator()<vartype<Var>>(
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

    using jump_idxs = typelist::map<jump_idx, Labels>;

    static constexpr auto table = intlist::array<jump_idxs>;
};

template <typename _ParmJump, typename LabelMap>
using pjmp_idxs = parm_jump_helper<_ParmJump, LabelMap>::jump_idxs;

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

template <typename LabelMap>
using get_index_t = snd_t<typelist::head<LabelMap>>::value_type;

namespace detail
{
template <typename Var>
struct load_tag
{
    using type = Var;
};

template <typename Var>
struct store_tag
{
    using type = Var;
};

template <typename Instruction, typename CPVar>
struct load_store_seq;

template <typename CPVar, typename Op, typename Dst, typename... Args>
struct load_store_seq<Compute<Op, Dst, Args...>, CPVar>
{
    using type = type_sequence<load_tag<Args>..., store_tag<Dst>>;
};

template <typename CPVar, typename Target>
struct load_store_seq<Jump<Target>, CPVar>
{
    using type = type_sequence<store_tag<CPVar>>;
};

template <typename CPVar, typename Var, typename... Labels>
struct load_store_seq<ParmJump<Var, Labels...>, CPVar>
{
    using type = type_sequence<load_tag<Var>, store_tag<CPVar>>;
};

template <typename Instruction, typename CPVar>
using load_store_seq_t = load_store_seq<Instruction, CPVar>::type;

template <typename Tag, typename Set>
struct store_set;

template <typename Var, typename Set>
struct store_set<load_tag<Var>, Set>
{
    using type = Set;
};

template <typename Var, typename Set>
struct store_set<store_tag<Var>, Set>
{
    // save variable name to store set
    using type = typelist::cons<varname<Var>, Set>;
};

template <typename Tag, typename Set>
using store_set_t = store_set<Tag, Set>::type;

template <typename Tag, typename Stores, typename Loads>
struct load_set;

template <typename Var, typename Stores, typename Loads>
struct load_set<load_tag<Var>, Stores, Loads>
{
    // if variable name in store set don't change load list
    // otherwise add the variable to the load list
    using type = std::conditional_t<typelist::elem<varname<Var>, Stores>,
                                    Loads,
                                    typelist::cons<Var, Loads>>;
};

template <typename Var, typename Stores, typename Loads>
struct load_set<store_tag<Var>, Stores, Loads>
{
    using type = Loads;
};

template <typename Tag, typename Stores, typename Loads>
using load_set_t = load_set<Tag, Stores, Loads>::type;
} // namespace detail

/// @brief Select only variables needed to load.
template <typename Cell, typename CPVar>
class load_list
{
    template <typename Tag>
    using p = std::negation<is_literal<typename Tag::type>>;

    template <typename C>
    using f = detail::load_store_seq_t<C, CPVar>;

    using seq = typelist::filter<p, typelist::concat<typelist::map<f, Cell>>>;

    template <typename Tag, typename Acc>
    struct load_folder
    {
        using stores = fst_t<Acc>;
        using loads  = snd_t<Acc>;
        using type   = type_pair<detail::store_set_t<Tag, stores>,
                               detail::load_set_t<Tag, stores, loads>>;
    };

    template <typename Tag, typename Acc>
    using load_folder_t = load_folder<Tag, Acc>::type;

  public:
    using type = typelist::nub<
        snd_t<typelist::foldl<load_folder_t,
                              type_pair<type_sequence<>, type_sequence<>>,
                              seq>>>;
};

template <typename Cell, typename CPVar>
using load_list_t = load_list<Cell, CPVar>::type;

/// @brief Select variables to write.
template <typename Cell, typename CPVar>
class store_list
{
    template <typename I>
    using f = stored_var_t<I, CPVar>;

  public:
    using type = typelist::map<f, Cell>;
};

template <typename Cell, typename CPVar>
using store_list_t = store_list<Cell, CPVar>::type;

template <typename Instruction, typename LabelMap>
struct standard_eval;

template <typename Op, typename Dst, typename... Args, typename _>
struct standard_eval<Compute<Op, Dst, Args...>, _>
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
    static constexpr auto value = typelist::lookup<Target, LabelMap>::value;

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::forward<_Memory>(mem).cp() = value;
    }
};

template <typename Var, typename Labels, typename LabelMap>
    requires(std::is_same_v<vartype<Var>, get_index_t<LabelMap>>)
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
    template <typename Instruction>
    using eval = Eval<Instruction, LabelMap>;

    using instructions = typelist::map<eval, Cell>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& memory) const noexcept
    {
        typelist::instantiate(instructions{}, std::forward<_Memory>(memory));
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
                     HaltLabel>;

        using type =
            std::conditional_t<is_jump<typelist::last<cell>>::value,
                               cell,
                               typelist::append<Jump<next_label>, cell>>;
    };

    template <typename CellIdx>
    using f = f_impl<CellIdx>::type;

    using type = typelist::map<f, typelist::enumerate<Cells>>;
};

/// @brief Ensures that every cell ends with a jump.
template <typename Cells, typename Labels>
using ensure_last_jumps_t = ensure_last_jumps<Cells, Labels>::type;

template <typename T>
struct same_size_union
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct same_size_union<Union<Ts...>>
{
    static constexpr bool value = typelist::of_same_size<type_sequence<Ts...>>;
};

template <typename T>
class infer_index
{
    template <typename C>
    struct p
    {
        static constexpr bool value = sizeof(C) == sizeof(T);
    };

  public:
    using type = typelist::find<p,
                                type_sequence<std::uint8_t,
                                              std::uint16_t,
                                              std::uint32_t,
                                              std::uint64_t,
                                              std::uintmax_t>>;
};

/// @brief Get unsigned integer type that has the same size as @p T.
template <typename T>
using infer_index_t = infer_index<T>::type;

/// @brief Program representation.
template <typename InputVars, typename Statements, typename OutputVars>
struct Program
{
    using input_vars  = InputVars;
    using output_vars = OutputVars;
    using index_t     = infer_index_t<vartype<typelist::head<InputVars>>>;

    static_assert(!std::is_void_v<index_t>);

    using cp = Variable<index_t, _cp_name>;

    template <typename C>
    using command_dst_t = stored_var_t<C, cp>;

    /// @brief Variables that have stored values to.
    using stated_vars = typelist::nub<typelist::join<
        typelist::join<InputVars, OutputVars>,
        typelist::map<command_dst_t,
                      typelist::filter<is_compute, Statements>>>>;

    static_assert(!typelist::any<is_literal, stated_vars>);

    /// @brief List of lists of commands.
    using cells_statements = typelist::split_with<is_label, Statements>;

    using stated_labels = typelist::filter<is_label, Statements>;

    static constexpr std::size_t num_cells = typelist::length<cells_statements>;

    static constexpr index_t halt_pos = -1;

    // Ensure that every cell is labeled.
    // Only the first label can be skipped, so we just add special label at the
    // beginning.
    using labels =
        std::conditional_t<num_cells == typelist::length<stated_labels>,
                           stated_labels,
                           typelist::cons<StartLabel, stated_labels>>;

    static_assert(!typelist::has_duplicates<labels>);

    using labelmap = typelist::cons<
        type_pair<HaltLabel, std::integral_constant<index_t, halt_pos>>,
        typelist::enumerate<labels, index_t>>;

    static constexpr bool singular = num_cells == 1;

    using variables = std::
        conditional_t<singular, stated_vars, typelist::cons<cp, stated_vars>>;
    static_assert(typelist::all<same_size_union, variables>);

    /// @brief Names of registers.
    using reg_names = typelist::nub<typelist::map<varname, variables>>;

    /// @brief Registers types.
    ///
    /// If there are few different types corresponding to the same name,
    /// lucid::Union used to hold values of the register.
    using reg_types = infer_reg_types_t<variables, reg_names>;

    // don't check jumps for singular program.
    using cells =
        std::conditional_t<singular,
                           cells_statements,
                           ensure_last_jumps_t<cells_statements, labels>>;
};

struct choice_op
{
    template <typename C, typename T>
    constexpr T
    operator()(const C c, const T a, const T b) const noexcept
    {
        return c ? a : b;
    }
};

template <std::integral T>
struct lshift_op
{
    constexpr T
    operator()(const T v, const T s) const noexcept
    {
        return v << s;
    }
};

template <std::integral T>
struct rshift_op
{
    constexpr T
    operator()(const T v, const T s) const noexcept
    {
        return v >> s;
    }
};
} // namespace lucid

#ifndef MAJOR_TYPE
#define MAJOR_TYPE T
#endif

#define VAR(TYPE, NAME) lucid::Variable<TYPE, lucid::sl2cs<#NAME>>
#define IDX_TYPE        lucid::infer_index_t<MAJOR_TYPE>
#define TVAR(NAME)      VAR(MAJOR_TYPE, NAME)
#define IVAR(NAME)      VAR(IDX_TYPE, NAME)

#define BEGIN(NAME, ...)                                             \
    using NAME = lucid::Program < lucid::type_sequence<__VA_ARGS__>, \
          lucid::type_sequence <
#define END(...)            >, lucid::type_sequence < __VA_ARGS__ >>
#define TBEGIN1(NAME, v)    BEGIN(NAME, TVAR(v))
#define TBEGIN2(NAME, a, b) BEGIN(NAME, TVAR(a), TVAR(b))
#define TEND1(v) \
    END          \
    (TVAR(v))
#define TEND2(a, b) \
    END             \
    (TVAR(a), TVAR(b))
#define IBEGIN1(NAME, v)    BEGIN(NAME, IVAR(v))
#define IBEGIN2(NAME, a, b) BEGIN(NAME, IVAR(a), IVAR(b))
#define IEND1(v) \
    END          \
    (IVAR(v))
#define IEND2(a, b) \
    END             \
    (IVAR(a), IVAR(b))

#define MOV(a, b)   lucid::Compute<std::identity, a, b>
#define TMOV(a, b)  MOV(TVAR(a), TVAR(b))
#define IMOV(a, b)  MOV(IVAR(a), IVAR(b))

#define LABEL(NAME) lucid::Label<lucid::sl2cs<#NAME>>
#define JMP(TARGET)             \
    lucid::Jump < LABEL(TARGET) \
    >

#define PJMPN(varname, ...) \
    lucid::ParmJump<VAR(IDX_TYPE, varname), lucid::type_sequence<__VA_ARGS__>>

#define PJMP2(varname, L1, L2)                                             \
    lucid::ParmJump < VAR(IDX_TYPE, varname), lucid::type_pair < LABEL(L1) \
    , LABEL(L2)                                                            \
    >>

#define PJMP3(varname, L1, L2, L3)                                             \
    lucid::ParmJump < VAR(IDX_TYPE, varname), lucid::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                                \
    , LABEL(L3)                                                                \
    >>

#define PJMP4(varname, L1, L2, L3, L4)                                         \
    lucid::ParmJump < VAR(IDX_TYPE, varname), lucid::type_sequence < LABEL(L1) \
    , LABEL(L2)                                                                \
    , LABEL(L3)                                                                \
    , LABEL(L4)                                                                \
    >>

#define BINOP(OP, TYPE, DST, A, B) \
    lucid::Compute<OP<TYPE>, VAR(TYPE, DST), VAR(TYPE, A), VAR(TYPE, B)>

#define UNOP(OP, TYPE, DST, V) \
    lucid::Compute<OP<TYPE>, VAR(TYPE, DST), VAR(TYPE, V)>

#define CMP(OP, TYPE, DST, A, B) \
    lucid::Compute<OP<TYPE>, IVAR(DST), VAR(TYPE, A), VAR(TYPE, B)>

#define CHOICE(TYPE, DST, C, A, B)   \
    lucid::Compute<lucid::choice_op, \
                   VAR(TYPE, DST),   \
                   VAR(IDX_TYPE, C), \
                   VAR(TYPE, A),     \
                   VAR(TYPE, B)>

#define ADD(...)     BINOP(std::plus, __VA_ARGS__)
#define SUB(...)     BINOP(std::minus, __VA_ARGS__)
#define MUL(...)     BINOP(std::multiplies, __VA_ARGS__)
#define DIV(...)     BINOP(std::divides, __VA_ARGS__)
#define MOD(...)     BINOP(std::modulus, __VA_ARGS__)
#define NEG(...)     UNOP(std::negate, __VA_ARGS__)

#define EQ(...)      CMP(std::equal_to, __VA_ARGS__)
#define NEQ(...)     CMP(std::not_equal_to, __VA_ARGS__)
#define GT(...)      CMP(std::greater, __VA_ARGS__)
#define LT(...)      CMP(std::less, __VA_ARGS__)
#define GE(...)      CMP(std::greater_equal, __VA_ARGS__)
#define LE(...)      CMP(std::less_equal, __VA_ARGS__)

#define LAND(...)    BINOP(std::logical_and, __VA_ARGS__)
#define LOR(...)     BINOP(std::logical_or, __VA_ARGS__)
#define LNOT(...)    UNOP(std::logical_not, __VA_ARGS__)

#define TADD(...)    ADD(MAJOR_TYPE, __VA_ARGS__)
#define TSUB(...)    SUB(MAJOR_TYPE, __VA_ARGS__)
#define TMUL(...)    MUL(MAJOR_TYPE, __VA_ARGS__)
#define TDIV(...)    DIV(MAJOR_TYPE, __VA_ARGS__)
#define TMOD(...)    MOD(MAJOR_TYPE, __VA_ARGS__)
#define TNEG(...)    NEG(MAJOR_TYPE, __VA_ARGS__)

#define TEQ(...)     EQ(MAJOR_TYPE, __VA_ARGS__)
#define TNEQ(...)    NEQ(MAJOR_TYPE, __VA_ARGS__)
#define TGT(...)     GT(MAJOR_TYPE, __VA_ARGS__)
#define TLT(...)     LT(MAJOR_TYPE, __VA_ARGS__)
#define TGE(...)     GE(MAJOR_TYPE, __VA_ARGS__)
#define TLE(...)     LE(MAJOR_TYPE, __VA_ARGS__)

#define IADD(...)    ADD(IDX_TYPE, __VA_ARGS__)
#define ISUB(...)    SUB(IDX_TYPE, __VA_ARGS__)
#define IMUL(...)    MUL(IDX_TYPE, __VA_ARGS__)
#define IDIV(...)    DIV(IDX_TYPE, __VA_ARGS__)
#define IMOD(...)    MOD(IDX_TYPE, __VA_ARGS__)
#define INEG(...)    NEG(IDX_TYPE, __VA_ARGS__)

#define IEQ(...)     EQ(IDX_TYPE, __VA_ARGS__)
#define INEQ(...)    NEQ(IDX_TYPE, __VA_ARGS__)
#define IGT(...)     GT(IDX_TYPE, __VA_ARGS__)
#define ILT(...)     LT(IDX_TYPE, __VA_ARGS__)
#define IGE(...)     GE(IDX_TYPE, __VA_ARGS__)
#define ILE(...)     LE(IDX_TYPE, __VA_ARGS__)

#define TAND(...)    LAND(MAJOR_TYPE, __VA_ARGS__)
#define TOR(...)     LOR(MAJOR_TYPE, __VA_ARGS__)
#define TNOT(...)    LNOT(MAJOR_TYPE, __VA_ARGS__)

#define IAND(...)    LAND(IDX_TYPE, __VA_ARGS__)
#define IOR(...)     LOR(IDX_TYPE, __VA_ARGS__)
#define INOT(...)    LNOT(IDX_TYPE, __VA_ARGS__)

#define TCHOICE(...) CHOICE(MAJOR_TYPE, __VA_ARGS__)
#define ICHOICE(...) CHOICE(INDEX_TYPE, __VA_ARGS__)

#define LSHFT(...)   BINOP(lucid::lshift_op, IDX_TYPE, __VA_ARGS__)
#define RSHFT(...)   BINOP(lucid::rshift_op, IDX_TYPE, __VA_ARGS__)
