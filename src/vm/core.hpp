// -*- C++ -*-
/// @file core.hpp
/// @brief Foundation of the virtual machine

#pragma once

#include <common/functional.hpp>
#include <common/string.hpp>
#include <common/tmp.hpp>
#include <common/tuple.hpp>
#include <common/union.hpp>

#include <cstdint>

/// @brief Namespace for virtual machine entities.
namespace loom::vm
{
/// @brief Describes a variable.
///
/// @tparam Type fundamental type.
/// @tparam Name register name.
template <typename Type, CharSequence Name>
struct Variable
{
    using type                                 = Type;
    using name                                 = Name;
    static constexpr std::string_view nameview = cs2sv<Name>;
};

/// @brief Get type of a Variable.
template <typename _Variable>
using vartype = _Variable::type;

/// @brief Get register name of a Variable as char_sequence.
template <typename _Variable>
using varname = _Variable::name;

/// @brief Get register name of a Variable as std::string_view.
template <typename _Variable>
constexpr inline std::string_view varview = _Variable::nameview;

/// @brief Name reserved for Program Counter register.
inline constexpr char _pc_char[] = "PC";

/// @brief char_sequence for _pc_char.
using _pc_name = sl2cs<_pc_char>;

/// @brief Checks that @p _Vars not named _pc_name.
template <typename... _Vars>
inline constexpr bool not_pc =
    !(std::conjunction_v<std::is_same<_pc_name, varname<_Vars>>...>);

/// @brief Describes a computation operation.
///
/// @tparam Op functional object class that performs actual operation.
/// @tparam Dst variable result written to.
/// @tparam Args variables which values used as parameters to @p Op.
template <typename Op, typename Dst, typename... Args>
    requires(not_pc<Dst, Args...> &&
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
template <CharSequence Name>
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
struct is_label : std::false_type
{
};

template <typename S>
struct is_label<Label<S>> : std::true_type
{
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
struct is_compute : std::false_type
{
};

template <typename... Ts>
struct is_compute<Compute<Ts...>> : std::true_type
{
};

/// @brief Check if @p T is Jump command.
template <typename T>
struct is_jump : std::false_type
{
};

template <typename T>
struct is_jump<Jump<T>> : std::true_type
{
};

template <typename... Ts>
struct is_jump<ParmJump<Ts...>> : std::true_type
{
};

/// @brief Check if @p T has effective statement.
template <typename T>
struct is_statement :
    std::bool_constant<is_label<T>::value || is_compute<T>::value ||
                       is_jump<T>::value>
{
};

/// @brief Get the variable @p Instruction stores result to.
template <typename Instruction, typename PCVar>
struct stored_var;

template <typename PCVar, typename Op, typename Dst, typename... Ts>
struct stored_var<Compute<Op, Dst, Ts...>, PCVar>
{
    using type = Dst;
};

template <typename PCVar, typename Target>
struct stored_var<Jump<Target>, PCVar>
{
    using type = PCVar;
};

template <typename PCVar, typename Var, typename... Labels>
struct stored_var<ParmJump<Var, Labels...>, PCVar>
{
    using type = PCVar;
};

template <typename Instruction, typename PCVar>
using stored_var_t = stored_var<Instruction, PCVar>::type;

/// @brief Get list of variables used in @p Instruction.
template <typename Instruction, typename PCVar>
struct ins_vars;

template <typename PCVar, typename Op, typename Dst, typename... Args>
struct ins_vars<Compute<Op, Dst, Args...>, PCVar>
{
    using type = type_sequence<Dst, Args...>;
};

template <typename PCVar, typename Target>
struct ins_vars<Jump<Target>, PCVar>
{
    using type = type_sequence<PCVar>;
};

template <typename PCVar, typename Var, typename... Labels>
struct ins_vars<ParmJump<Var, Labels...>, PCVar>
{
    using type = type_sequence<PCVar, Var>;
};

/// @brief Get all variables used in @p Instruction.
template <typename Instruction, typename PCVar>
using ins_vars_t = ins_vars<Instruction, PCVar>::type;

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

/// @brief Select unique types of register @p Name from variables @p Vars.
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

template <typename T, typename... Ts>
struct collected_type<type_sequence<T, Ts...>>
{
    using type = std::conditional_t<
        typelist::same<type_sequence<T, Ts...>>,
        T,
        typelist::to<typelist::nub<type_sequence<T, Ts...>>, loom::Union>>;
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

template <typename Block, typename PCVar>
struct block_vars
{
    template <typename C>
    using f = ins_vars_t<C, PCVar>;

    using type = typelist::nub<typelist::concat<typelist::map<f, Block>>>;
};

template <typename Block, typename PCVar>
using block_vars_t = block_vars<Block, PCVar>::type;

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
    constexpr const Val&
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
        requires(typelist::elem<varname<Var>, RegNames>)
    static constexpr decltype(auto)
    get(data& _data) noexcept
    {
        constexpr reg_access<Var> op{};

        return op.template operator()<vartype<Var>>(
            std::get<regindex<varname<Var>>>(_data));
    }

    template <typename Var>
        requires(typelist::elem<varname<Var>, RegNames>)
    static constexpr decltype(auto)
    get(const data& _data) noexcept
    {
        constexpr reg_access<Var> op{};

        return op.template operator()<vartype<Var>>(
            std::get<regindex<varname<Var>>>(_data));
    }
};

/// @brief Create array of target blocks for jump using label
/// mapping.
template <typename _Jump, typename LabelMap>
struct jump_helper;

template <typename Target, typename LabelMap>
struct jump_helper<Jump<Target>, LabelMap>
{
    using idx = typelist::lookup<Target, LabelMap>;

    using jump_idxs =
        std::integer_sequence<typename idx::value_type, idx::value>;

    static constexpr auto table = intlist::array<jump_idxs>;
};

template <typename Var, typename Labels, typename LabelMap>
struct jump_helper<ParmJump<Var, Labels>, LabelMap>
{
    template <typename L>
        requires(typelist::key_elem<L, LabelMap>)
    using jump_idx = typelist::lookup<L, LabelMap>;

    using jump_idxs =
        typelist::to_integer_sequence<typelist::map<jump_idx, Labels>>;

    static constexpr auto table = intlist::array<jump_idxs>;
};

template <typename _ParmJump, typename LabelMap>
using jmp_idxs = jump_helper<_ParmJump, LabelMap>::jump_idxs;

template <typename _ParmJump, typename LabelMap>
constexpr inline auto jmp_arr = jump_helper<_ParmJump, LabelMap>::table;

/// @brief Create successor table for basic blocks.
template <typename Blocks, typename LabelMap>
struct successors_table
{
    using type = type_sequence<>;
};

template <typename Blocks, typename LabelMap>
    requires(typelist::length<Blocks> > 1)
struct successors_table<Blocks, LabelMap>
{
    template <typename Block>
    using f =
        jmp_idxs<typelist::at<typelist::length<Block> - 1, Block>, LabelMap>;

    using type = typelist::map<f, Blocks>;
};

/// @brief Helper typedef.
template <typename Blocks, typename LabelMap>
using successors_table_t = successors_table<Blocks, LabelMap>::type;

template <typename _Variable>
consteval std::optional<vartype<_Variable>>
parse_literal() noexcept
{
    return s2n<vartype<_Variable>>(varview<_Variable>);
}

template <typename _Variable>
struct is_literal : std::bool_constant<parse_literal<_Variable>().has_value()>
{
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

template <typename Instruction, typename PCVar>
struct load_store_seq;

template <typename PCVar, typename Op, typename Dst, typename... Args>
struct load_store_seq<Compute<Op, Dst, Args...>, PCVar>
{
    using type = type_sequence<load_tag<Args>..., store_tag<Dst>>;
};

template <typename PCVar, typename Target>
struct load_store_seq<Jump<Target>, PCVar>
{
    using type = type_sequence<store_tag<PCVar>>;
};

template <typename PCVar, typename Var, typename... Labels>
struct load_store_seq<ParmJump<Var, Labels...>, PCVar>
{
    using type = type_sequence<load_tag<Var>, store_tag<PCVar>>;
};

template <typename Instruction, typename PCVar>
using load_store_seq_t = load_store_seq<Instruction, PCVar>::type;

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

template <typename Target, typename Dict, typename Items>
struct CleanUpState
{
    using target = Target;
    using dict   = Dict;
    using items  = Items;
};

template <typename Instructions>
struct test_label_head
{
    using type = void;
};

template <typename S, typename... Is>
struct test_label_head<type_sequence<Label<S>, Is...>>
{
    using type = Label<S>;
};

template <typename MaybeInstruction, typename Target, typename Dict>
struct update_dict;

template <typename Target, typename Dict>
struct update_dict<void, Target, Dict>
{
    using type = Dict;
};

template <typename S, typename Target, typename Dict>
struct update_dict<Label<S>, Target, Dict>
{
    using type = typelist::cons<type_pair<Label<S>, Target>, Dict>;
};

template <typename E, typename _CleanUpState>
struct trim_cont;

template <typename _CleanUpState>
struct trim_front
{
    using type =
        trim_cont<typename test_label_head<typename _CleanUpState::items>::type,
                  _CleanUpState>::type;
};

template <typename _CleanUpState>
struct trim_cont<void, _CleanUpState>
{
    using type = _CleanUpState;
};

template <typename S, typename _CleanUpState>
struct trim_cont<Label<S>, _CleanUpState>
{
  private:
    using e = Label<S>;
    using t = _CleanUpState::target;
    using i = typelist::tail<typename _CleanUpState::items>;
    using d = update_dict<e, t, typename _CleanUpState::dict>::type;
    using s = CleanUpState<t, d, i>;

  public:
    using type = trim_front<s>::type;
};

template <typename Instructions>
class trim
{
    using rf = trim_front<
        CleanUpState<StartLabel, type_sequence<>, Instructions>>::type;
    using rb =
        trim_front<CleanUpState<HaltLabel,
                                typename rf::dict,
                                typelist::reverse<typename rf::items>>>::type;

  public:
    using type =
        type_pair<typelist::reverse<typename rb::items>, typename rb::dict>;
};

template <typename ZD>
struct optimize_adjacent_labels;

template <typename S, typename T, typename... Xs, typename Ys, typename Dict>
struct optimize_adjacent_labels<
    type_pair<type_pair<type_sequence<Label<S>, Label<T>, Xs...>, Ys>, Dict>>
{
    using type = optimize_adjacent_labels<
        type_pair<type_pair<type_sequence<Label<T>, Xs...>, Ys>,
                  typelist::cons<type_pair<Label<S>, Label<T>>, Dict>>>::type;
};

template <typename Ys, typename Dict>
struct optimize_adjacent_labels<type_pair<type_pair<type_sequence<>, Ys>, Dict>>
{
    using type = type_pair<type_pair<type_sequence<>, Ys>, Dict>;
};

template <typename LZ, typename Dict>
struct optimize_adjacent_labels<type_pair<LZ, Dict>>
{
    using type = optimize_adjacent_labels<
        type_pair<typelist::list_zipper_step_forward<LZ>, Dict>>::type;
};

template <typename Instruction, typename LabelDict>
struct update_jmp
{
    using type = Instruction;
};

template <typename L, typename LabelDict>
struct update_jmp<Jump<L>, LabelDict>
{
    using type = Jump<choice_t<typelist::lookup<L, LabelDict>, L>>;
};

template <typename Var, typename Labels, typename LabelDict>
struct update_jmp<ParmJump<Var, Labels>, LabelDict>
{
    using type = ParmJump<Var, typelist::replace_with_dict<LabelDict, Labels>>;
};

template <typename Instructions, typename LabelDict>
class update_jmps
{
    template <typename I>
    using f = update_jmp<I, LabelDict>::type;

  public:
    using type = typelist::map<f, Instructions>;
};
} // namespace detail

/// @brief Select only variables needed to load.
template <typename Block, typename PCVar>
class load_list
{
    template <typename Tag>
    using p = std::negation<is_literal<typename Tag::type>>;

    template <typename C>
    using f = detail::load_store_seq_t<C, PCVar>;

    using seq = typelist::filter<p, typelist::concat<typelist::map<f, Block>>>;

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

/// @brief Helper typedef.
template <typename Block, typename PCVar>
using load_list_t = load_list<Block, PCVar>::type;

/// @brief Select variables to write.
template <typename Block, typename PCVar>
class store_list
{
    template <typename I>
    using f = stored_var_t<I, PCVar>;

  public:
    using type = typelist::map<f, Block>;
};

/// @brief Helper typedef.
template <typename Block, typename PCVar>
using store_list_t = store_list<Block, PCVar>::type;

/// @brief Default instruction evaluation.
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
    constexpr get_index_t<LabelMap>
    operator()(_Memory&& mem) const noexcept
    {
        std::forward<_Memory>(mem).pc() = value;
        return 0;
    }
};

template <typename Var, typename Labels, typename LabelMap>
    requires(std::is_same_v<vartype<Var>, get_index_t<LabelMap>>)
struct standard_eval<ParmJump<Var, Labels>, LabelMap>
{
    static constexpr auto table = jmp_arr<ParmJump<Var, Labels>, LabelMap>;

    using index_t = get_index_t<LabelMap>;

    template <typename _Memory>
    constexpr index_t
    operator()(_Memory&& mem) const noexcept
    {
        const index_t ret = std::forward<_Memory>(mem).template get<Var>();
        std::forward<_Memory>(mem).pc() = table[ret];
        return ret;
    }
};

/// @brief Default evaluator.
template <typename Block,
          typename LabelMap,
          template <typename, typename> typename Eval = standard_eval>
struct standard_compiler
{
    template <typename Instruction>
    using eval = Eval<Instruction, LabelMap>;

    using instructions = typelist::map<eval, Block>;

    template <typename... Args>
    constexpr decltype(auto)
    operator()(Args&&... args) const noexcept
    {
        return typelist::instantiate(instructions{},
                                     std::forward<Args>(args)...);
    }
};

/// @brief Ensures that every block ends with a jump.
template <typename Blocks, typename Labels>
class ensure_last_jumps
{
    template <typename BlockIdx>
    struct f_impl
    {
        using block = fst_t<BlockIdx>;

        // is either label of the next block or if there is none ending label
        using next_label =
            choice_t<typelist::at<(snd_t<BlockIdx>::value + 1), Labels>,
                     HaltLabel>;

        using type =
            std::conditional_t<is_jump<typelist::last<block>>::value,
                               block,
                               typelist::append<Jump<next_label>, block>>;
    };

    template <typename BlockIdx>
    using f = f_impl<BlockIdx>::type;

  public:
    using type = typelist::map<f, typelist::enumerate<Blocks>>;
};

/// @brief Helper typedef.
template <typename Blocks, typename Labels>
using ensure_last_jumps_t = ensure_last_jumps<Blocks, Labels>::type;

/// @brief Trim extra labels in the beginning and ending of @p Instructions
/// list.
template <typename Instructions>
using trim_t = detail::trim<Instructions>::type;

/// @brief Update jumps to deleted labels.
template <typename Instructions, typename LabelDict>
using update_jumps_t = detail::update_jmps<Instructions, LabelDict>::type;

/// @brief Remove adjascent labels.
template <typename Instructions>
using optimize_adjascent_labels = detail::optimize_adjacent_labels<
    type_pair<type_pair<Instructions, type_sequence<>>, type_sequence<>>>::type;

/// @brief Remove adjascent labels and update jumps.
template <typename Instructions>
class optimize_labels
{
    using tp = trim_t<Instructions>;
    using ti = fst_t<tp>;
    using td = snd_t<tp>;
    using tr = update_jumps_t<ti, td>;
    using ar = optimize_adjascent_labels<tr>;
    using ai = typelist::reverse<snd_t<fst_t<ar>>>;
    using od = snd_t<ar>;

  public:
    using type = update_jumps_t<ai, od>;
};

/// @brief Helper typedef.
template <typename Instructions>
using optimize_labels_t = optimize_labels<Instructions>::type;

template <typename T>
struct same_size_union : std::true_type
{
};

template <typename... Ts>
struct same_size_union<Union<Ts...>> :
    std::bool_constant<typelist::of_same_size<type_sequence<Ts...>>>
{
};

template <typename T>
struct select_first : std::type_identity<T>
{
};

template <typename T, typename... Ts>
struct select_first<Union<T, Ts...>>
{
    using type = T;
};

template <typename T>
using select_first_t = select_first<T>::type;

template <typename T>
using make_var = Variable<fst_t<T>, snd_t<T>>;

/// @brief Macro definition structure.
///
/// Exists for pretty printing only.
///
/// @tparam Name macro name.
/// @tparam Parameters Parameters names.
template <CharSequence Name, typename... Parameters>
struct macro_decl
{
    static constexpr std::string_view nameview = cs2sv<Name>;

    using parameters = type_sequence<Parameters...>;
};

template <CharSequence MacroName, typename... Args>
struct macro_inv
{
    static constexpr std::string_view nameview = cs2sv<MacroName>;

    using args = type_sequence<Args...>;
};

/// @brief Kernel definition structure.
///
/// @tparam Name kernel name.
/// @tparam InputVars input variables.
/// @tparam Statements instruction statements.
/// @tparam OutputVars output variables.
template <CharSequence   Name,
          typelist::List InputVars,
          typelist::List Statements,
          typelist::List OutputVars>
    requires(!(typelist::empty<InputVars> || typelist::empty<Statements> ||
               typelist::empty<OutputVars>))
struct kernel_blank
{
    using input_vars  = InputVars;
    using statements  = Statements;
    using output_vars = OutputVars;
    using index_t = uint_of_size_t<sizeof(vartype<typelist::head<InputVars>>)>;
    using flat_instructions =
        typelist::filter<is_statement, typelist::flatten<statements>>;
    using instructions = optimize_labels_t<flat_instructions>;

    static_assert(!std::is_void_v<index_t>);

    using pc = Variable<index_t, _pc_name>;

    template <typename C>
    using command_dst_t = stored_var_t<C, pc>;

    using stated_comps = typelist::filter<is_compute, instructions>;
    static_assert(!typelist::empty<stated_comps>);

    /// @brief Variables that have stored values to.
    using stated_vars = typelist::nub<
        typelist::join<typelist::join<InputVars, OutputVars>,
                       typelist::map<command_dst_t, stated_comps>>>;

    static_assert(!typelist::any<is_literal, stated_vars>);

    using stated_labels = typelist::filter<is_label, instructions>;
    static_assert(!typelist::elem<StartLabel, stated_labels>);
    static_assert(!typelist::elem<HaltLabel, stated_labels>);

    /// @brief List of lists of commands.
    using blocks_statements =
        typelist::purge<type_sequence<>,
                        typelist::split_when<is_label, instructions>>;

    static constexpr std::size_t num_blocks =
        typelist::length<blocks_statements>;
    static constexpr std::size_t num_labels = typelist::length<stated_labels>;

    static constexpr index_t halt_pos = -1;

    using labels = typelist::cons<StartLabel, stated_labels>;

    static_assert(!typelist::has_duplicates<labels>);

    using labelmap = typelist::cons<
        type_pair<HaltLabel, std::integral_constant<index_t, halt_pos>>,
        typelist::enumerate<labels, index_t>>;

    static constexpr bool singular = num_blocks == 1;

    using variables = std::
        conditional_t<singular, stated_vars, typelist::cons<pc, stated_vars>>;
    static_assert(typelist::all<same_size_union, variables>);

    /// @brief Names of registers.
    using reg_names = typelist::nub<typelist::map<varname, variables>>;

    /// @brief Registers types.
    ///
    /// If there are few different types corresponding to the same name,
    /// loom::Union used to hold values of the register.
    using reg_types = infer_reg_types_t<variables, reg_names>;

    // don't check jumps for singular program.
    using blocks =
        std::conditional_t<singular,
                           blocks_statements,
                           ensure_last_jumps_t<blocks_statements, labels>>;

    static constexpr std::string_view nameview = cs2sv<Name>;

    /// @brief Variables to access unique registers once.
    using variables1 = typelist::map<
        make_var,
        typelist::zip<typelist::map<select_first_t, reg_types>, reg_names>>;

    /// @brief Successors table.
    using successors = successors_table_t<blocks, labelmap>;
};

template <typename K>
struct is_kernel : std::false_type
{
};

template <typename... _>
struct is_kernel<kernel_blank<_...>> : std::true_type
{
};

/// @brief Kernel ready to compile.
template <typename T>
concept Kernel = is_kernel<T>::value;

/// @brief Helper shortcut type_sequence with input types of program @p P.
/// @tparam K kernel.
template <Kernel K>
using input_types = typelist::map<vartype, typename K::input_vars>;

/// @brief Helper shortcut type_sequence with output types of program @p P.
/// @tparam K kernel.
template <Kernel K>
using output_types = typelist::map<vartype, typename K::output_vars>;

/// @brief The type accepted to input for the program @p P.
/// @tparam K kernel.
template <Kernel K>
using input_type = typelist::maybe_to<input_types<K>, std::tuple>;

/// @brief The resulting type of the program @p P.
/// @tparam K kernel.
template <Kernel K>
using output_type = typelist::maybe_to<output_types<K>, std::tuple>;

/// @brief Helper function. Ensures that the result value is valid block number
/// or halt position.
/// @tparam K kernel.
template <Kernel K>
static constexpr K::index_t
fit_block_index(const typename K::index_t c) noexcept
{
    using index_t                = K::index_t;
    constexpr index_t num_blocks = K::num_blocks;
    constexpr index_t halt_pos   = K::halt_pos;

    return c < num_blocks ? c : halt_pos;
}

/// @brief Specialized dispatch block helper.
/// @tparam K kernel.
/// @tparam Block basic block.
/// @tparam BlockSuccessors successors of basic block @p Block.
/// @tparam F alias meta-function to specialized dispatch class.
/// @tparam Compiler compiler for basic block.
template <Kernel K,
          typename Block,
          typename BlockSuccessors,
          template <typename K::index_t> typename F,
          template <typename> typename Compiler>
struct sd_helper
{
    using bkss = BlockSuccessors;
    using ccss = intlist::map_t<F, BlockSuccessors>;
    using eval = Compiler<Block>;

    static constexpr std::size_t l = typelist::length<ccss>;
};

/// @brief HALT specialization.
template <typename K,
          template <typename K::index_t> typename F,
          template <typename> typename Compiler>
struct sd_helper<K, void, void, F, Compiler>
{
};
} // namespace loom::vm
