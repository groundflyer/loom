// -*- C++ -*-
/// @file tmp.hpp
/// @brief Template metaprogramming.

#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <static_math/cmath.h>

#include "math.hpp"

namespace loom
{
/// @defgroup TMP Metaprogramming
/// @brief Template metaprogramming.
///
/// Here we define metafunctions as class templates,
/// where function parameters are template parameters
/// and the result is a public typedef member @p type
/// or a static constexpr member @p value.
///
/// The templates are hidden in the @p detail namespace,
/// while type aliases and variable templates are visible
/// in loom namespace.
/// @{

/// @brief Sequence of types.
template <typename... Ts>
struct type_sequence
{
};

template <typename T>
struct is_type_sequence : std::false_type
{
};

template <typename... Ts>
struct is_type_sequence<type_sequence<Ts...>> : std::true_type
{
};

/// @brief Pair of types.
template <typename A, typename B>
using type_pair = type_sequence<A, B>;

/// @brief Select first non-void type.
/// If both are void the result type is void.
template <typename T, typename Alt>
using choice_t = std::conditional_t<std::is_void_v<T>, Alt, T>;

template <typename TypePair>
struct fst;

template <typename A, typename B>
struct fst<type_pair<A, B>>
{
    using type = A;
};

template <typename TypePair>
struct snd;

template <typename A, typename B>
struct snd<type_pair<A, B>>
{
    using type = B;
};

/// @brief Extract the first type of @p Pair.
///
/// It is basically the same as typelist::head, but ensures
/// that the number of elements in type_sequence @p Pair is
/// exactly two.
template <typename Pair>
using fst_t = fst<Pair>::type;

/// @brief Extract the second type of @p Pair.
///
/// Same as typelist::last, but guarantees that type_sequence @p Pair
/// consists only of two types.
template <typename Pair>
using snd_t = snd<Pair>::type;

/// @brief Determines whether resultig type of @p F invoked with @p Args
/// satisfies predicate @p RP.
///
/// It's generalized and stricter version of std::is_invocable_v.
///
/// @tparam RP predicate meta-function. Must have public static bool member
/// value.
template <template <typename> typename RP, typename F, typename... Args>
inline constexpr bool is_strictly_invocable_rp_v =
    std::is_invocable_v<F, Args...> &&
    RP<std::invoke_result_t<F, Args...>>::value;
/// @}

// In this namespace we don't suffix typedefs with _t and values with _v.
// Same applies to intlist namespace.
namespace typelist
{
namespace detail
{
template <typename T, typename List>
struct cons;

template <typename T, typename... Ts>
struct cons<T, type_sequence<Ts...>>
{
    using type = type_sequence<T, Ts...>;
};

template <typename List1, typename List2>
struct join;

template <typename... Ts1, typename... Ts2>
struct join<type_sequence<Ts1...>, type_sequence<Ts2...>>
{
    using type = type_sequence<Ts1..., Ts2...>;
};

template <std::size_t N, typename... Ts>
struct repeat
{
    using type =
        join<type_sequence<Ts...>, typename repeat<N - 1, Ts...>::type>::type;
};

template <typename... Ts>
struct repeat<0, Ts...>
{
    using type = type_sequence<>;
};

template <typename List>
struct length;

template <typename... Ts>
struct length<type_sequence<Ts...>> :
    std::integral_constant<std::size_t, sizeof...(Ts)>
{
};

template <std::size_t I, typename List>
struct at;

template <typename T, typename... Ts>
struct at<0, type_sequence<T, Ts...>>
{
    using type = T;
};

template <std::size_t I>
struct at<I, type_sequence<>>
{
    using type = void;
};

template <std::size_t I, typename T, typename... Ts>
struct at<I, type_sequence<T, Ts...>>
{
    using type = at<I - 1, type_sequence<Ts...>>::type;
};

template <typename List>
struct tail;

template <>
struct tail<type_sequence<>>
{
    using type = type_sequence<>;
};

template <typename T, typename... Ts>
struct tail<type_sequence<T, Ts...>>
{
    using type = type_sequence<Ts...>;
};

template <typename T, typename List>
struct elem;

template <typename T, typename... Ts>
struct elem<T, type_sequence<Ts...>> :
    std::bool_constant<std::disjunction_v<std::is_same<T, Ts>...>>
{
};

template <std::size_t I, typename S, typename List>
struct index;

template <std::size_t I, typename S>
struct index<I, S, type_sequence<>> : std::integral_constant<std::size_t, -1ul>
{
};

template <std::size_t I, typename S, typename T, typename... Ts>
struct index<I, S, type_sequence<T, Ts...>> :
    std::integral_constant<std::size_t,
                           std::is_same_v<S, T> ?
                               I :
                               index<I + 1ul, S, type_sequence<Ts...>>::value>
{
};

template <template <typename, typename> typename F, typename Acc, typename List>
struct foldl;

template <template <typename, typename> typename F, typename Acc>
struct foldl<F, Acc, type_sequence<>>
{
    using type = Acc;
};

template <template <typename, typename> typename F,
          typename Acc,
          typename T,
          typename... Ts>
struct foldl<F, Acc, type_sequence<T, Ts...>>
{
    using type = foldl<F, F<T, Acc>, type_sequence<Ts...>>::type;
};

template <template <typename, typename> typename F, typename Acc, typename List>
struct foldr;

template <template <typename, typename> typename F, typename Acc>
struct foldr<F, Acc, type_sequence<>>
{
    using type = Acc;
};

template <template <typename, typename> typename F,
          typename Acc,
          typename T,
          typename... Ts>
struct foldr<F, Acc, type_sequence<T, Ts...>>
{
    using type = F<T, typename foldr<F, Acc, type_sequence<Ts...>>::type>;
};

template <template <typename> typename F, typename List>
struct map;

template <template <typename> typename F, typename T, typename... Ts>
    requires(std::is_integral_v<std::remove_cvref_t<decltype(F<T>::value)>>)
struct map<F, type_sequence<T, Ts...>>
{
    using type =
        std::integer_sequence<std::remove_cvref_t<decltype(F<T>::value)>,
                              F<T>::value,
                              F<Ts>::value...>;
};

template <template <typename> typename F, typename... Ts>
struct map<F, type_sequence<Ts...>>
{
    using type = type_sequence<F<Ts>...>;
};

template <std::size_t N, typename List>
struct take;

template <typename T, typename... Ts>
struct take<1, type_sequence<T, Ts...>>
{
    using type = type_sequence<T>;
};

template <std::size_t N, typename T, typename... Ts>
    requires(N <= length<type_sequence<T, Ts...>>::value)
struct take<N, type_sequence<T, Ts...>>
{
    using type =
        typename cons<T,
                      typename take<N - 1, type_sequence<Ts...>>::type>::type;
};

template <std::size_t N, typename List>
struct drop;

template <typename T, typename... Ts>
struct drop<1, type_sequence<T, Ts...>>
{
    using type = type_sequence<Ts...>;
};

template <std::size_t N, typename T, typename... Ts>
    requires(N <= length<type_sequence<T, Ts...>>::value)
struct drop<N, type_sequence<T, Ts...>>
{
    using type = drop<N - 1, type_sequence<Ts...>>::type;
};

template <template <typename, typename> typename F,
          typename List1,
          typename List2>
struct zip_with;

template <template <typename, typename> typename F, typename X, typename Y>
struct zip_with<F, type_sequence<X>, type_sequence<Y>>
{
    using type = type_sequence<F<X, Y>>;
};

template <template <typename, typename> typename F, typename X>
struct zip_with<F, type_sequence<X>, type_sequence<>>
{
    using type = type_sequence<>;
};

template <template <typename, typename> typename F, typename Y>
struct zip_with<F, type_sequence<>, type_sequence<Y>>
{
    using type = type_sequence<>;
};

template <template <typename, typename> typename F>
struct zip_with<F, type_sequence<>, type_sequence<>>
{
    using type = type_sequence<>;
};

template <template <typename, typename> typename F,
          typename X,
          typename Y,
          typename... Xs,
          typename... Ys>
struct zip_with<F, type_sequence<X, Xs...>, type_sequence<Y, Ys...>>
{
    using type =
        typename cons<F<X, Y>,
                      typename zip_with<F,
                                        type_sequence<Xs...>,
                                        type_sequence<Ys...>>::type>::type;
};

template <typename LZ>
struct lz_step_forward;

template <typename T, typename... Ts, typename Bs>
struct lz_step_forward<type_pair<type_sequence<T, Ts...>, Bs>>
{
    using type = type_pair<type_sequence<Ts...>, typename cons<T, Bs>::type>;
};

template <typename List>
struct same;

template <typename T, typename... Ts>
struct same<type_sequence<T, Ts...>> :
    std::bool_constant<std::conjunction_v<std::is_same<T, Ts>...>>
{
};

template <typename List, typename... Args>
struct result_of;

template <typename... Args, typename... Fs>
struct result_of<type_sequence<Fs...>, Args...>
{
    using type = type_sequence<std::invoke_result_t<Fs, Args...>...>;
};

template <typename List>
struct has_duplicates;

template <>
struct has_duplicates<type_sequence<>> : std::false_type
{
};

template <typename T, typename... Ts>
struct has_duplicates<type_sequence<T, Ts...>> :
    std::bool_constant<elem<T, type_sequence<Ts...>>::value ||
                       has_duplicates<type_sequence<Ts...>>::value>
{
};

template <template <typename> typename P, typename List>
struct all;

template <template <typename> typename P, typename... Ts>
struct all<P, type_sequence<Ts...>> :
    std::bool_constant<std::conjunction_v<P<Ts>...>>
{
};

template <template <typename> typename P, typename List>
struct any;

template <template <typename> typename P, typename... Ts>
struct any<P, type_sequence<Ts...>> :
    std::bool_constant<std::disjunction_v<P<Ts>...>>
{
};

template <template <typename> typename P, typename List>
struct filter;

template <template <typename> typename P>
struct filter<P, type_sequence<>>
{
    using type = type_sequence<>;
};

template <template <typename> typename P, typename T, typename... Ts>
struct filter<P, type_sequence<T, Ts...>>
{
    using rest = filter<P, type_sequence<Ts...>>::type;
    using type =
        std::conditional_t<P<T>::value, typename cons<T, rest>::type, rest>;
};

template <typename T, typename List>
class purge
{
    template <typename Elem>
    using f = std::negation<std::is_same<T, Elem>>;

  public:
    using type = filter<f, List>::type;
};

template <typename List>
struct nub;

template <>
struct nub<type_sequence<>>
{
    using type = type_sequence<>;
};

template <typename T, typename... Ts>
struct nub<type_sequence<T, Ts...>>
{
    using type =
        cons<T,
             typename nub<
                 typename purge<T, type_sequence<Ts...>>::type>::type>::type;
};

template <typename T, typename Map>
struct lookup;

template <typename T>
struct lookup<T, type_sequence<>>
{
    using type = void;
};

template <typename T, typename Key, typename Value, typename... Ts>
struct lookup<T, type_sequence<type_pair<Key, Value>, Ts...>>
{
    using type =
        std::conditional_t<std::is_same_v<T, Key>,
                           Value,
                           typename lookup<T, type_sequence<Ts...>>::type>;
};

template <template <typename> typename P, typename List>
struct find;

template <template <typename> typename P>
struct find<P, type_sequence<>>
{
    using type = void;
};

template <template <typename> typename P, typename T, typename... Ts>
struct find<P, type_sequence<T, Ts...>>
{
    using type =
        std::conditional_t<P<T>::value,
                           T,
                           typename find<P, type_sequence<Ts...>>::type>;
};

template <template <typename> typename Predicate,
          typename InputList,
          typename OutputList,
          typename TmpList>
struct Splitter
{
    template <typename T>
    using delim_p     = Predicate<T>;
    using input_list  = InputList;
    using tmp_list    = TmpList;
    using output_list = OutputList;
};

template <template <typename> typename P, typename Elem>
struct test_delim
{
    using type = std::conditional_t<P<Elem>::value, void, Elem>;
};

template <typename Elem, typename L>
struct update_tmp
{
    using type = join<L, type_sequence<Elem>>::type;
};

template <typename L>
struct update_tmp<void, L>
{
    using type = type_sequence<>;
};

template <typename Elem, typename Tmp, typename Out>
struct update_out
{
    using type = Out;
};

template <typename Tmp, typename Out>
struct update_out<void, Tmp, Out>
{
    using type = join<Out, type_sequence<Tmp>>::type;
};

template <typename _Splitter>
class split_step
{
    using old_tmp = _Splitter::tmp_list;
    using old_inp = _Splitter::input_list;
    using old_out = _Splitter::output_list;

    template <typename T>
    using p = _Splitter::template delim_p<T>;

    using elem = test_delim<p, typename at<0, old_inp>::type>::type;

    using new_tmp = update_tmp<elem, old_tmp>::type;
    using new_inp = tail<old_inp>::type;
    using new_out = update_out<elem, old_tmp, old_out>::type;

  public:
    using type = Splitter<p, new_inp, new_out, new_tmp>;
};

template <typename Inp, typename _Splitter>
struct split_cont;

template <typename T, typename... Ts, typename _Splitter>
struct split_cont<type_sequence<T, Ts...>, _Splitter>
{
    using next_splitter = split_step<_Splitter>::type;
    using type =
        split_cont<typename next_splitter::input_list, next_splitter>::type;
};

template <typename _Splitter>
struct split_cont<type_sequence<>, _Splitter>
{
    using type = _Splitter;
};

template <template <typename> typename P, typename InputList>
struct make_splitter
{
    using type = Splitter<P, InputList, type_sequence<>, type_sequence<>>;
};

template <template <typename> typename P, typename InputList>
class split_when
{
    using result_splitter =
        split_cont<InputList, typename make_splitter<P, InputList>::type>::type;

  public:
    using type = join<typename result_splitter::output_list,
                      type_sequence<typename result_splitter::tmp_list>>::type;
};

template <typename D, typename List>
class split_up
{
    template <typename U>
    using eq = std::is_same<D, U>;

  public:
    using type = split_when<eq, List>::type;
};

template <typename List, typename Aggr>
struct flatten;

template <typename Aggr>
struct flatten<type_sequence<>, Aggr>
{
    using type = Aggr;
};

template <typename Head, typename... Tail, typename Aggr>
struct flatten<type_sequence<Head, Tail...>, Aggr>
{
    using type =
        cons<Head, typename flatten<type_sequence<Tail...>, Aggr>::type>::type;
};

template <typename... Ts, typename... Tail, typename Aggr>
struct flatten<type_sequence<type_sequence<Ts...>, Tail...>, Aggr>
{
    using type =
        join<typename flatten<type_sequence<Ts...>, type_sequence<>>::type,
             typename flatten<type_sequence<Tail...>, Aggr>::type>::type;
};

template <typename Source>
struct from;

template <template <typename...> typename Source, typename... Ts>
struct from<Source<Ts...>>
{
    using type = type_sequence<Ts...>;
};

template <typename Source, template <typename...> typename Target>
struct to;

template <template <typename...> typename Target, typename... Ts>
struct to<type_sequence<Ts...>, Target>
{
    using type = Target<Ts...>;
};

template <typename List>
struct to_integer_seq;

template <typename IntConst, typename... ICs>
struct to_integer_seq<type_sequence<IntConst, ICs...>>
{
    using type = std::integer_sequence<typename IntConst::value_type,
                                       IntConst::value,
                                       ICs::value...>;
};

template <typename List>
struct max_sizeof;

template <typename... Ts>
struct max_sizeof<type_sequence<Ts...>> :
    std::integral_constant<std::size_t, std::max({sizeof(Ts)...})>
{
};

template <typename List>
struct of_same_size;

template <typename T>
struct of_same_size<type_sequence<T>> : std::true_type
{
};

template <typename T, typename... Ts>
struct of_same_size<type_sequence<T, Ts...>> :
    std::bool_constant<(... && (sizeof(T) == sizeof(Ts)))>
{
};

template <typename Dict, typename List>
class replace_with_dict
{
    template <typename T>
    using f = choice_t<typename lookup<T, Dict>::type, T>;

  public:
    using type = map<f, List>::type;
};

template <std::unsigned_integral I,
          I                      idx,
          typename Visitor,
          typename List,
          typename Default,
          typename... Args>
constexpr decltype(auto)
visit_at_impl(const I   i,
              Visitor&& visitor,
              List      list,
              Default&& def,
              Args&&... args)
{
    if(idx == i)
        return visitor.template operator()<typename at<idx, List>::type>(
            std::forward<Args>(args)...);

    if constexpr(idx < length<List>::value - 1)
        return visit_at_impl<I, idx + 1>(i,
                                         std::forward<Visitor>(visitor),
                                         list,
                                         std::forward<Default>(def),
                                         std::forward<Args>(args)...);
    else
        return std::invoke(std::forward<Default>(def));
}

struct self_caller
{
    template <typename T, typename... Args>
    decltype(auto)
    operator()(Args&&... args) const
        noexcept(std::is_nothrow_invocable_v<T, Args...>)
    {
        return std::invoke(T{}, std::forward<Args>(args)...);
    }
};
} // namespace detail

/// @defgroup Typelist Typelist
/// @brief List operations on type sequences.
/// @ingroup TMP
/// @{

/// @brief type_sequence constraint.
template <typename T>
concept List = is_type_sequence<T>::value;

/// @brief Add class @p T to the beginning of type_sequence @p L.
template <typename T, List L>
using cons = detail::cons<T, L>::type;

/// @brief Concatenate two lists of types.
template <List L1, List L2>
using join = detail::join<L1, L2>::type;

/// @brief Add class @p T to the end of type_sequence @p L.
template <typename T, List L>
using append = join<L, type_sequence<T>>;

/// @brief Constructs type_sequence repeating classes @p Ts @p N times.
template <std::size_t N, typename... Ts>
using repeat = detail::repeat<N, Ts...>::type;

/// @brief Number of types in @p L.
template <List L>
inline constexpr std::size_t length = detail::length<L>::value;

/// @brief Extract the @p Ith class from @p L.
///
/// If @p L is too short, the resulting type is void.
template <std::size_t I, List L>
using at = detail::at<I, L>::type;

/// @brief Extract the first class from @p L.
template <List L>
using head = at<0, L>;

/// @brief Extract the last element from @p L.
template <List L>
using last = at<length<L> - 1, L>;

/// @brief Extract the elements after the head of @p list as type_sequence.
template <List L>
using tail = detail::tail<L>::type;

/// @brief Check whether @p T occur in @p L.
template <typename T, List L>
inline constexpr bool elem = detail::elem<T, L>::value;

/// @brief The index of the first occurence of type @p T.
/// If there is no such type, the value is -1ul.
template <typename T, List L>
inline constexpr std::size_t index = detail::index<0ul, T, L>::value;

/// @brief std::index_sequence for @p L.
template <List L>
using indices = std::make_index_sequence<length<L>>;

/// @brief Left-associative fold of a type_sequence @p L.
template <template <typename, typename> typename F, typename Acc, List L>
using foldl = detail::foldl<F, Acc, L>::type;

/// @brief Right-associative fold of a type_sequence @p L.
template <template <typename, typename> typename F, typename Acc, List L>
using foldr = detail::foldr<F, Acc, L>::type;

/// @brief Reverse the order of elements.
template <List L>
using reverse = foldr<append, type_sequence<>, L>;

/// @brief Apply @p F to each element of @p L.
///
/// When @p F<T> has static public member @p value of integral type
/// the result is std::integer_sequence containing these values.
/// Otherwise - type_sequence.
template <template <typename> typename F, List L>
using map = detail::map<F, L>::type;

/// @brief Remove all occurences of type @p T from @p L.
///
/// Does nothing if there is no @p T in @p L.
template <typename T, List L>
using purge = detail::purge<T, L>::type;

/// @brief Prefix of @p L of length @p N.
///
/// @p N must be less than or equal of length of @p L.
template <std::size_t N, List L>
using take = detail::take<N, L>::type;

/// @brief Suffix of @p L after the first @p N elements.
///
/// @p N must be less than or equal of length of @p L.
template <std::size_t N, List L>
using drop = detail::drop<N, L>::type;

/// @brief Generalization of zip. Zips with the meta-function
/// given as the first argument, instead of type_pair.
///
/// If one input list is shorter than the other,
/// excess elements of the longer list are discarded.
template <template <typename, typename> typename F, typename Xs, typename Ys>
using zip_with = detail::zip_with<F, Xs, Ys>::type;

/// @brief Make list of pairs type_sequence<type_pair<X, Y>...> of
/// corresponding types from lists @p Xs and @p Ys.
///
/// If one input list is shorter than the other,
/// excess elements of the longer list are discarded.
template <typename Xs, typename Ys>
using zip = zip_with<type_pair, Xs, Ys>;

/// @brief The concatenation of all the elements of a list @p L of lists.
template <List L>
using concat = foldr<join, type_sequence<>, L>;

/// @brief Flatten list that possibly contains lists of arbitrary nesting depth.
template <List L>
using flatten = detail::flatten<L, type_sequence<>>::type;

/// @brief Given a list zipper (a pair of lists) take a step forward moving
/// the head element from @p a to @p b.
template <typename ListZipper>
using list_zipper_step_forward = detail::lz_step_forward<ListZipper>::type;

/// @brief Replace items occuring as keys of @p Dict in @p L with
/// corresponding values.
///
/// @tparam Dict type_sequence<type_pair<Key, Value>....>
/// @tparam L type_sequence<...>
template <typename Dict, List L>
using replace_with_dict = detail::replace_with_dict<Dict, L>::type;

/// @brief Check whether the classes of @p L are the same.
template <List L>
inline constexpr bool same = detail::same<L>::value;

/// @brief Considering the classes of type_sequence @p Fs as functions
/// get the list of return types by applying the functions to
/// arguments of types @p Args.
///
/// @tparam Fs type_sequence of callable types.
/// @tparam Args Arguments to apply function from @p Fs to.
template <typename Fs, typename... Args>
using result_of = detail::result_of<Fs, Args...>;

/// @brief Check if there are duplicates in @p L.
template <List L>
inline constexpr bool has_duplicates = detail::has_duplicates<L>::value;

/// @brief Determines whether all elements of the @p L satisfy the predicate
/// @p P.
///
/// @tparam P must have static constexpr bool value member.
template <template <typename> typename P, List L>
inline constexpr bool all = detail::all<P, L>::value;

/// @brief Determines whether any element of the @p L satisfy the predicate
/// @p P.
///
/// @tparam P must have static constexpr bool value member.
template <template <typename> typename P, List L>
inline constexpr bool any = detail::any<P, L>::value;

/// @brief Remove duplicate elements from @p L.
template <List L>
using nub = detail::nub<L>::type;

/// @brief Filter @p L elements with a predicate @p P.
///
/// @tparam P must have static constexpr bool value member.
///
/// The resulting type_sequence contains elements that
/// satisfy P, i.e. P<T>::value == true.
template <template <typename> typename P, List L>
using filter = detail::filter<P, L>::type;

/// @brief Lookup a key @p Key in association list @p Map.
///
/// @tparam Map consists of key-value pairs type_sequence<Key, Value>.
///
/// If @p Key not found the resulting type is void.
template <typename Key, List Map>
using lookup = detail::lookup<Key, Map>::type;

/// @brief Extract list of keys from association list @p Map.
///
/// @tparam Map consists of key-value pairs type_sequence<Key, Value>.
template <List Map>
using keys = map<fst_t, Map>;

/// @brief Extract list of values from association list @p Map
///
/// @tparam Map consists of key-value pairs type_sequence<Key, Value>.
template <List Map>
using values = map<snd_t, Map>;

/// @brief Helper meta-function. Check if @p Map has element with key @p Key.
///
/// @tparam Map consists of key-value pairs type_sequence<Key, Value>.
template <typename Key, List Map>
inline constexpr bool key_elem = elem<Key, keys<Map>>;

/// @brief Get the leftmost type from @p L matching predicate @p P.
///
/// @tparam P must have static constexpr bool value member.
///
/// If there is no type matching @p P the resulting type is void.
template <template <typename> typename P, List L>
using find = detail::find<P, L>::type;

/// @brief Check if @p L is empty.
template <List L>
inline constexpr bool empty = length<L> == 0;

/// @brief Split typelist into sublists with a predicate.
///
/// @tparam P has static constexpr member @p value.
///
/// The delimeter elements are such that satisfy predicate @p P,
/// i.e. P<T>::value == true.
template <template <typename> typename P, List L>
using split_when = detail::split_when<P, L>::type;

/// @brief Split typelist into sublists with a delimeter.
///
/// The resulting type is type_sequence<type_sequence<...>...>.
template <typename Delimeter, List L>
using split_up = detail::split_up<Delimeter, L>::type;

/// @brief Convert a class template @p Source into type_sequence.
template <typename Source>
using from = detail::from<Source>::type;

/// @brief Convert type_sequence @p Source to @p Target class template.
template <typename Source, template <typename...> typename Target>
using to = detail::to<Source, Target>::type;

/// @brief If @p L consists of single element it's the resulting type,
/// otherwise it's @p Target of the elements.
template <List L, template <typename...> typename Target>
    requires(length<L> != 0)
using maybe_to = std::conditional_t<length<L> == 1, head<L>, to<L, Target>>;

/// @brief Convert type_sequence of std::integer_constant into a
/// std::integer_sequence.
///
/// The value type of the first element is used as the value type for the
/// resulting std::integer_sequence.
template <List L>
using to_integer_sequence = detail::to_integer_seq<L>::type;

/// @brief Size of the biggest type in the list.
template <List L>
constexpr inline std::size_t max_sizeof = detail::max_sizeof<L>::value;

/// @brief Check if all types in @p L have the same size.
template <List L>
constexpr inline bool of_same_size = detail::of_same_size<L>::value;

/// @brief Invoke @p f with each class of @p L as template parameter.
template <typename F, typename... Ts, typename... Args>
constexpr void
for_each(type_sequence<Ts...> /* list */, F&& f, Args&&... args) noexcept(
    (... && noexcept(f.template operator()<Ts>(std::forward<Args>(args)...))))
{
    (..., f.template operator()<Ts>(std::forward<Args>(args)...));
}

/// @brief Same as typelist::for_each, except each argument of @p args passed
/// separately to coresponding invocation of @p f.
template <typename F, typename... Ts, typename... Args>
    requires(sizeof...(Ts) == sizeof...(Args))
constexpr void
for_each_arg(type_sequence<Ts...> /* list */, F&& f, Args&&... args) noexcept(
    (... && noexcept(f.template operator()<Ts>(std::forward<Args>(args)))))
{
    (..., f.template operator()<Ts>(std::forward<Args>(args)));
}

/// @brief Consecutively invoke @p f with each class of @p list as the first
/// template parameter and @p args as function parameters. If theare more than
/// one entries in @p list, collect results into an instance of @p Tuple.
///
/// It's similar to std::transform or loom::map_tuple, except the input is a
/// list of types, so it can be seen as transformation of virtual entities
/// (types) to actual values.
///
/// Each invocation looks roughly like this:
/// @code
/// auto v_n = f<T_n>(args...);
/// @endcode
///
/// @return @p Tuple of resulting
/// @tparam Tuple class template to hold resulting values.
template <template <typename...> typename Tuple = std::tuple,
          typename F,
          typename... Ts,
          typename... Args>
constexpr decltype(auto)
maybe_collect(type_sequence<Ts...> /*list*/, F&& f, Args&&... args) noexcept(
    (... && noexcept(f.template operator()<Ts>(std::forward<Args>(args)...))))
{
    if constexpr(sizeof...(Ts) > 1)
        return Tuple<std::remove_cvref_t<decltype(f.template operator()<Ts>(
            std::forward<Args>(args)...))>...> {
            f.template operator()<Ts>(std::forward<Args>(args)...)...
        };
    else
        return f.template operator()<Ts...>(std::forward<Args>(args)...);
}

/// @brief Same as typelist::maybe_collect, except each argument of @p args
/// passed separately to corresponding invocation of @p f.
///
/// This function is kind of loom::realise(zip(@p list, @p args)).
///
/// Each invocation looks roughly like this:
/// @code
/// auto v_n = f<T_n>(arg_n);
/// @endcode
///
/// @return @p Tuple of elements if there are more than one elements in @p list
/// and return value of @p f is not void.
/// @tparam Tuple class template to hold resulting values.
template <template <typename...> typename Tuple = std::tuple,
          typename F,
          typename... Ts,
          typename... Args>
    requires(sizeof...(Ts) == sizeof...(Args))
constexpr decltype(auto)
maybe_collect_arg(type_sequence<Ts...> /*list*/,
                  F&& f,
                  Args&&... args) noexcept((... &&
                                            noexcept(f.template operator()<Ts>(
                                                std::forward<Args>(args)))))
{
    if constexpr(sizeof...(Ts) > 1)
        return Tuple<std::remove_cvref_t<decltype(f.template operator()<Ts>(
            std::forward<Args>(args)))>...> {
            f.template operator()<Ts>(std::forward<Args>(args))...
        };
    else
        return f.template operator()<Ts...>(std::forward<Args>(args)...);
}

/// @brief Instantiate every class in typelist as a functional object and invoke
/// it with @p args.
template <typename... Fs, typename... Args>
    requires(... && (std::is_nothrow_default_constructible_v<Fs> &&
                     std::is_invocable_v<Fs, Args...>))
constexpr void
instantiate(type_sequence<Fs...> list, Args&&... args) noexcept(
    std::conjunction_v<std::is_nothrow_invocable<Fs, Args...>...>)
{
    for_each(list, detail::self_caller{}, std::forward<Args>(args)...);
}

/// @brief Invoke @p f with elements of typelist::type_sequence as template
/// parameter with @p args until the result is true or control reaches end of
/// the list.
///
/// @return true if at least one invocation returns true, otherwise false.
template <typename... Ts, typename F, typename... Args>
constexpr bool
first_of(type_sequence<Ts...> /*list*/, F&& f, Args&&... args)
{
    return (... || f.template operator()<Ts>(std::forward<Args>(args)...));
}

/// @brief Invoke every functional object @p Fs in the typelist with @p args
/// until the result is true or control reaches end of the list.
///
/// @return true if at least one invocation returns true, otherwise false.
template <typename... Fs, typename... Args>
    requires(std::conjunction_v<std::is_invocable_r<bool, Fs, Args...>...>)
constexpr bool
first_of_funcs(type_sequence<Fs...> list, Args&&... args) noexcept(
    std::conjunction_v<std::is_nothrow_invocable<Fs, Args...>...>)
{
    return first_of(list, detail::self_caller{}, std::forward<Args>(args)...);
}

/// @brief Invoke @p visitor on the @p i'th element of @p L.
///
/// @p visitor is incoked with an element of the @p L as a template
/// parameter.
///
/// @param i which @p L element to invoke @p visitor on.
/// @param visitor callable to invoke. Is called with @p L element
/// as template argument.
/// @param def function that invoked without arguments if @p i out of range.
/// @param args arguments passed to @p visitor on invocation.
/// @tparam L contains elements to invoke @p visitor on.
template <std::unsigned_integral I,
          typename Visitor,
          List L,
          typename Default,
          typename... Args>
constexpr decltype(auto)
visit_at(const I i, Visitor&& visitor, L list, Default&& def, Args&&... args)
{
    return detail::visit_at_impl<I, 0u>(i,
                                        std::forward<Visitor>(visitor),
                                        list,
                                        std::forward<Default>(def),
                                        std::forward<Args>(args)...);
}

/// @brief Same as typelist::visit_at , but typelist @p L contains callables.
/// @tparam L consists of callable types.
/// @param i which @p L element to invoke @p visitor on.
/// @param def function that invoked without arguments if @p i out of range.
/// @param args arguments passed to the invoked element.
template <std::unsigned_integral I, List L, typename Default, typename... Args>
constexpr decltype(auto)
visit_funcs_at(const I i, L list, Default&& def, Args&&... args)
{
    return visit_at(i,
                    detail::self_caller{},
                    list,
                    std::forward<Default>(def),
                    std::forward<Args>(args)...);
}
/// @}
} // namespace typelist

namespace intlist
{
namespace detail
{
template <typename T>
struct is_integer_sequence : std::false_type
{
};

template <std::integral Int, Int... Is>
struct is_integer_sequence<std::integer_sequence<Int, Is...>> : std::true_type
{
};

template <auto Val, typename List>
struct cons;

template <typename Int, Int Head>
struct cons<Head, std::integer_sequence<Int>>
{
    using type = std::integer_sequence<Int, Head>;
};

template <typename Int, Int Head, Int... Rest>
struct cons<Head, std::integer_sequence<Int, Rest...>>
{
    using type = std::integer_sequence<Int, Head, Rest...>;
};

template <typename List1, typename List2>
struct join;

template <typename Int, Int... Ints1, Int... Ints2>
struct join<std::integer_sequence<Int, Ints1...>,
            std::integer_sequence<Int, Ints2...>>
{
    using type = std::integer_sequence<Int, Ints1..., Ints2...>;
};

template <std::size_t N, typename Int, Int... Vals>
struct repeat
{
    using type =
        typename join<std::integer_sequence<Int, Vals...>,
                      typename repeat<N - 1, Int, Vals...>::type>::type;
};

template <typename Int, Int... Vals>
struct repeat<0, Int, Vals...>
{
    using type = std::integer_sequence<Int>;
};

template <typename List, std::size_t N, auto Val>
struct nappend;

template <std::size_t N, typename Int, Int Val, Int... Vals>
struct nappend<std::integer_sequence<Int, Vals...>, N, Val>
{
    using type = join<std::integer_sequence<Int, Vals...>,
                      typename repeat<N, Int, Val>::type>::type;
};

template <typename List>
struct length;

template <typename Int, Int... Nums>
struct length<std::integer_sequence<Int, Nums...>> :
    std::integral_constant<std::size_t, sizeof...(Nums)>
{
};

template <std::size_t I, typename List>
struct at;

template <typename Int, Int Head, Int... Vals>
struct at<0, std::integer_sequence<Int, Head, Vals...>> :
    std::integral_constant<Int, Head>
{
};

template <std::size_t I, typename Int, Int Head, Int... Vals>
    requires(I < length<std::integer_sequence<Int, Head, Vals...>>::value)
struct at<I, std::integer_sequence<Int, Head, Vals...>> :
    std::integral_constant<
        Int,
        at<I - 1, std::integer_sequence<Int, Vals...>>::value>
{
};

template <typename List>
struct tail;

template <typename Int, Int X, Int... Xs>
struct tail<std::integer_sequence<Int, X, Xs...>>
{
    using type = std::integer_sequence<Int, Xs...>;
};

template <auto V, typename List>
struct elem;

template <typename Int, Int V, Int... Is>
struct elem<V, std::integer_sequence<Int, Is...>> :
    std::bool_constant<(... || (V == Is))>
{
};

template <std::size_t I, auto S, typename List>
struct index;

template <std::size_t I, typename Int, Int S>
struct index<I, S, std::integer_sequence<Int>> :
    std::integral_constant<std::size_t, -1ul>
{
};

template <std::size_t I, typename Int, Int S, Int X, Int... Xs>
struct index<I, S, std::integer_sequence<Int, X, Xs...>> :
    std::integral_constant<
        std::size_t,
        S == X ? I :
                 index<I + 1ul, S, std::integer_sequence<Int, Xs...>>::value>
{
};

template <typename List>
struct reverse;

template <typename Int, Int x>
struct reverse<std::integer_sequence<Int, x>>
{
    using type = std::integer_sequence<Int, x>;
};

template <typename Int, Int x, Int... xs>
struct reverse<std::integer_sequence<Int, x, xs...>>
{
    using type =
        typename join<typename reverse<std::integer_sequence<Int, xs...>>::type,
                      std::integer_sequence<Int, x>>::type;
};

template <typename F, typename List>
struct map_f;

template <typename F, typename Int, Int... Is>
    requires(std::is_integral_v<std::invoke_result_t<F, Int>>)
struct map_f<F, std::integer_sequence<Int, Is...>>
{
  private:
    static constexpr F f{};
    using R = std::invoke_result_t<F, Int>;

  public:
    using type = std::integer_sequence<R, std::invoke(f, Is)...>;
};

template <template <auto> typename F, typename List>
struct map_t;

template <typename Int, template <auto> typename F, Int... Is>
struct map_t<F, std::integer_sequence<Int, Is...>>
{
    using type = type_sequence<F<Is>...>;
};

template <std::size_t N, typename List>
struct take;

template <typename Int, Int I, Int... Is>
struct take<1, std::integer_sequence<Int, I, Is...>>
{
    using type = std::integer_sequence<Int, I>;
};

template <std::size_t N, typename Int, Int I, Int... Is>
    requires(N <= length<std::integer_sequence<Int, I, Is...>>::value)
struct take<N, std::integer_sequence<Int, I, Is...>>
{
    using type =
        cons<I, typename take<N - 1, std::integer_sequence<Int, Is...>>::type>::
            type;
};

template <std::size_t N, typename List>
struct drop;

template <typename Int, Int I, Int... Is>
struct drop<1, std::integer_sequence<Int, I, Is...>>
{
    using type = std::integer_sequence<Int, Is...>;
};

template <std::size_t N, typename Int, Int I, Int... Is>
    requires(N <= length<std::integer_sequence<Int, I, Is...>>::value)
struct drop<N, std::integer_sequence<Int, I, Is...>>
{
    using type = drop<N - 1, std::integer_sequence<Int, Is...>>::type;
};

template <auto Val, typename List>
struct purge;

template <typename Int, Int Val>
struct purge<Val, std::integer_sequence<Int>>
{
    using type = std::integer_sequence<Int>;
};

template <typename Int, Int Last, Int Val>
struct purge<Val, std::integer_sequence<Int, Last>>
{
    using type = std::conditional_t<Last == Val,
                                    std::integer_sequence<Int>,
                                    std::integer_sequence<Int, Last>>;
};

template <typename Int, Int Val, Int Head, Int... Rest>
struct purge<Val, std::integer_sequence<Int, Head, Rest...>>
{
  private:
    using rest = std::integer_sequence<Int, Rest...>;

  public:
    using type = std::conditional_t<
        Head == Val,
        typename purge<Val, rest>::type,
        typename cons<Head, typename purge<Val, rest>::type>::type>;
};

template <typename List>
struct array;

template <typename Int, Int... Is>
struct array<std::integer_sequence<Int, Is...>>
{
    static constexpr Int value[sizeof...(Is)] = {Is...};
};

template <typename List>
struct stdarray;

template <typename Int, Int... Is>
struct stdarray<std::integer_sequence<Int, Is...>>
{
    static constexpr std::array<Int, sizeof...(Is)> value = {Is...};
};

template <typename List>
struct has_duplicates;

template <typename Int, Int I>
struct has_duplicates<std::integer_sequence<Int, I>> : std::false_type
{
};

template <typename Int, Int X, Int... Xs>
struct has_duplicates<std::integer_sequence<Int, X, Xs...>> :
    std::bool_constant<elem<X, std::integer_sequence<Int, Xs...>>::value ||
                       has_duplicates<std::integer_sequence<Int, Xs...>>::value>
{
};

template <typename List>
struct nub;

template <typename Int>
struct nub<std::integer_sequence<Int>>
{
    using type = std::integer_sequence<Int>;
};

template <typename Int, Int X, Int... Xs>
struct nub<std::integer_sequence<Int, X, Xs...>>
{
    using type =
        cons<X,
             typename nub<typename purge<X, std::integer_sequence<Int, Xs...>>::
                              type>::type>::type;
};

template <std::integral auto N>
struct powersof2s
{
    template <std::integral auto i>
    using f = std::integral_constant<decltype(N), smath::pow(2, i)>;

    using type = typelist::to_integer_sequence<
        typename map_t<f, std::make_index_sequence<N>>::type>;
};

template <auto A, auto B, typename List>
struct range_impl
{
    using type = range_impl<A + 1, B, typename nappend<List, 1, A>::type>::type;
};

template <auto A, typename List>
struct range_impl<A, A, List>
{
    using type = List;
};
} // namespace detail

/// @defgroup Intlist Intlist
/// @brief Compile-time list operations on std::integer_sequence.
/// @ingroup TMP
/// @{

/// @brief std::integer_sequence constraint.
template <typename T>
concept List = detail::is_integer_sequence<T>::value;

/// @brief Add integer value @p Val to the beginning of @p L.
template <auto Val, List L>
using cons = detail::cons<Val, L>::type;

/// @brief Concatenate two std::integer_sequence.
template <List L1, List L2>
using join = detail::join<L1, L2>::type;

/// @brief Constructs std::integer_sequence repeating @p Vals @p N
/// times.
template <std::size_t N, std::integral Int, Int... Vals>
using repeat = detail::repeat<N, Int, Vals...>::type;

/// @brief Constructs std::integer_sequence of range [A, B).
template <std::integral auto A, std::integral auto B>
    requires(std::is_same_v<decltype(A), decltype(B)> && A < B)
using range =
    detail::range_impl<A, B, std::integer_sequence<decltype(A)>>::type;

/// @brief Reverse the order of the elements in @p L.
template <List L>
using reverse = detail::reverse<L>::type;

/// @brief Apply @p F to each element of @p L.
///
/// @f$\mathrm{F}(\mathrm{I}) \rightarrow \mathrm{T}@f$,
/// The resulting type @f$\mathrm{T}@f$ must be integer.
template <typename F, List L>
using map_f = detail::map_f<F, L>::type;

/// @brief Make types from elements of @p L by
/// specializing template class F with the values.
///
/// The resulting type is type_sequence<F<Int>...>.
template <template <auto> typename F, List L>
using map_t = detail::map_t<F, L>::type;

/// @brief Add @p Val @p N times to the end of @p L.
template <List L, std::size_t N, std::integral auto Val>
using nappend = detail::nappend<L, N, Val>::type;

/// @brief Add @p Val to the end of @p L.
template <List L, std::integral auto Val>
using append = nappend<L, 1, Val>;

/// @brief Length of an std::integer_sequence.
template <List L>
inline constexpr std::size_t length = detail::length<L>::value;

/// @brief Extract the @p Ith elementh from @p L.
template <std::size_t I, List L>
inline constexpr auto at = detail::at<I, L>::value;

/// @brief Extract the first element from @p L.
template <List L>
inline constexpr auto head = at<0, L>;

/// @brief Extract the last element from @p L.
template <List L>
inline constexpr auto last = at<length<L> - 1, L>;

/// @brief Extract the elements after the head of @p list as type_sequence.
template <List L>
using tail = detail::tail<L>::type;

/// @brief Check whether @p X occur in @p L.
template <std::integral auto X, List L>
inline constexpr bool elem = detail::elem<X, L>::value;

/// @brief The index of the first occurence of @p X.
/// If there is no @p X in @p L the value is -1ul.
template <std::integral auto X, List L>
inline constexpr std::size_t index = detail::index<0ul, X, L>::value;

/// @brief std::index_sequence for @p L.
template <List L>
using indices = std::make_index_sequence<length<L>>;

/// @brief Make std::integer_sequence without @p X.
template <std::integral auto X, List L>
using purge = detail::purge<X, L>::type;

/// @brief Prefix of @p L of length @p N.
///
/// @p N must be less than or equal of length of @p L.
template <std::size_t N, List L>
using take = detail::take<N, L>::type;

/// @brief Suffix of @p L after the first @p N elements.
///
/// @p N must be less than or equal of length of @p L.
template <std::size_t N, List L>
using drop = detail::drop<N, L>::type;

/// @brief Convert std::integer_sequence into C array.
template <List L>
inline constexpr auto array = detail::array<L>::value;

/// @brief Convert std::integer_sequence into std::array.
template <List L>
inline constexpr auto stdarray = detail::stdarray<L>::value;

/// @brief Check if there are duplicates in @p L.
template <List L>
inline constexpr bool has_duplicates = detail::has_duplicates<L>::value;

/// @brief Remove duplicate elements from @p L.
template <List L>
using nub = detail::nub<L>::type;

template <std::size_t N, std::size_t I>
struct lehmer_code_impl
{
  private:
    static constexpr std::size_t quot = N / I;
    static constexpr std::size_t rem  = N % I;

  public:
    using type = append<typename lehmer_code_impl<quot, I + 1>::type, rem>;
};

template <std::size_t I>
struct lehmer_code_impl<0, I>
{
    using type = std::index_sequence<>;
};

template <List Idxs, typename FacNum>
struct lehmer_perm_impl;

template <List Idxs>
struct lehmer_perm_impl<Idxs, std::index_sequence<>>
{
    using type = std::index_sequence<>;
};

template <List Idxs, std::size_t Head, std::size_t... Rest>
struct lehmer_perm_impl<Idxs, std::index_sequence<Head, Rest...>>
{
  private:
    static constexpr std::size_t elem = at<Head, Idxs>;

  public:
    using type =
        cons<elem,
             typename lehmer_perm_impl<purge<elem, Idxs>,
                                       std::index_sequence<Rest...>>::type>;
};

/// @brief Factorial representation of number @p N.
///
/// See https://en.wikipedia.org/wiki/Factorial_number_system
/// and https://en.wikipedia.org/wiki/Lehmer_code
template <std::size_t N>
using lehmer_code = lehmer_code_impl<N, 1>::type;

/// @brief Generate a permutation of index array @p Idxs from
/// its factorial representation @p FacNum.
template <List Idxs, typename FacNum>
using lehmer_perm = lehmer_perm_impl<Idxs, FacNum>::type;

template <std::size_t N, std::size_t L>
    requires(N < fac(L))
struct nlperm_impl
{
  private:
    using _lehmer_code                = lehmer_code<N>;
    static constexpr std::size_t diff = L - length<_lehmer_code>;

  public:
    using type =
        lehmer_perm<std::make_index_sequence<L>,
                    join<repeat<diff, std::size_t, 0ul>, _lehmer_code>>;
};

/// @brief @p Nth permutation in lexicographical order
/// of an index sequence of length @p L.
template <std::size_t N, std::size_t L>
using nlperm = nlperm_impl<N, L>::type;

/// @brief First N numbers of power of two series.
template <std::integral auto N>
using powerof2s = typename detail::powersof2s<N>::type;

/// @brief Invoke @p f with each value of @p L as template parameter.
template <typename F, typename Int, Int... Is, typename... Args>
constexpr void
for_each(F&& f,
         std::integer_sequence<Int, Is...> /*L*/,
         Args&&... args) noexcept((... &&
                                   noexcept(f.template operator()<Is>(
                                       std::forward<Args>(args)...))))
{
    (..., f.template operator()<Is>(std::forward<Args>(args)...));
}
/// @}
} // namespace intlist

namespace typelist
{
namespace detail
{
template <typename L, std::integral Index, Index start>
struct enumerate
{
    template <Index i>
    using index_type = std::integral_constant<Index, start + i>;

    using type = typelist::zip<
        L,
        intlist::map_t<index_type,
                       std::make_index_sequence<typelist::length<L>>>>;
};
} // namespace detail

/// @ingroup Typelist
/// @brief Enumerate types in @p L with indices of type @p Index
/// @f$[(T_0, S + 0), (T_1, S + 1), ... (T_n, S + n)]@f$
/// @tparam L list to enumerate.
/// @tparam I index type.
/// @tparam S start number.
///
/// Type of the index member is std::integer_constant<Index, value>.
template <List L, std::integral I = std::size_t, I S = 0>
using enumerate = detail::enumerate<L, I, S>::type;

/// @brief Masked version of typelist::for_each.
template <typename F, List L, std::size_t N, typename... Args>
    requires(N == length<L>)
constexpr void
for_each_masked(L /* list */, F&& f, const std::bitset<N>& mask, Args&&... args)
{
    for_each(
        enumerate<L>{},
        [&]<typename P>(Args&&... args)
        {
            if(mask.test(snd_t<P>::value))
                f.template operator()<fst_t<P>>(std::forward<Args>(args)...);
        },
        std::forward<Args>(args)...);
}

/// @brief Masked version of typelist::for_each_arg.
template <typename F, List L, std::size_t N, typename... Args>
    requires(N == length<L>)
constexpr void
for_each_arg_masked(L /* list */,
                    F&&                   f,
                    const std::bitset<N>& mask,
                    Args&&... args)
{
    for_each_arg(
        enumerate<L>{},
        [&]<typename P>(auto arg)
        {
            if(mask.test(snd_t<P>::value))
                f.template operator()<fst_t<P>>(
                    std::forward<decltype(arg)>(arg));
        },
        std::forward<Args>(args)...);
}
} // namespace typelist

template <std::size_t N>
class int_of_bits
{
    template <typename C>
    struct p : std::bool_constant<sizeof(C) * 8 >= N>
    {
    };

  public:
    using uint_t = typelist::find<p,
                                  type_sequence<std::uint8_t,
                                                std::uint16_t,
                                                std::uint32_t,
                                                std::uint64_t,
                                                std::uintmax_t>>;
    using int_t  = typelist::find<p,
                                  type_sequence<std::int8_t,
                                                std::int16_t,
                                                std::int32_t,
                                                std::int64_t,
                                                std::intmax_t>>;
};

/// @brief Unsigned integer type capable of holding @p N bits.
///
/// void if there is no such.
template <std::size_t N>
using uint_of_bits_t = int_of_bits<N>::uint_t;

/// @brief Unsigned integer type at least of size @p S bytes.
///
/// void if there is no such.
template <std::size_t S>
using uint_of_size_t = uint_of_bits_t<S * 8>;

/// @brief Integer type capable of holding @p N bits.
///
/// void if there is no such.
template <std::size_t N>
using int_of_bits_t = int_of_bits<N>::int_t;

/// @brief Integer type of at leas size @p S bytes.
///
/// void if there is no such.
template <std::size_t S>
using int_of_size_t = int_of_bits_t<S * 8>;
} // namespace loom
