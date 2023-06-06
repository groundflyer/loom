// -*- C++ -*-
/// @file tmp.hpp
/// @brief Template metaprogramming.

#pragma once

#include <array>
#include <string_view>
#include <type_traits>
#include <utility>

#include "math.hpp"

namespace lucid
{
/// @defgroup TMP Metaprogramming
/// @brief Template metaprogramming.
///
/// Here we define metafunctions as class templates,
/// where function parameters are template parameters
/// and the result is a public typedef @p type
/// or a static constexpr member @p value.
///
/// The templates are hidden in the @p detail namespace,
/// while type aliases and variable templates are visible
/// in lucid namespace.
/// @{

/// @brief Compile-time string.
template <char... Cs>
using char_sequence = std::integer_sequence<char, Cs...>;

/// @brief Sequence of types.
template <typename... Ts>
struct type_sequence
{
};
/// @}

/// @defgroup CTString Compile-time string
/// @brief Converting string literals into char_sequence at compile time.
/// @ingroup TMP
/// @{

/// @brief Wraps char[@p N].
template <size_t _N>
struct StringLiteral
{
    // cut off null terminator
    static constexpr std::size_t N = _N - 1;

    constexpr StringLiteral(const char (&str)[_N])
    {
        for(std::size_t i = 0; i < N; ++i) value[i] = str[i];
    }

    char value[N]{};

    constexpr char
    operator[](const std::size_t i) const
    {
        return value[i];
    }

    constexpr std::size_t
    size() const
    {
        return N;
    }

    constexpr std::string_view
    view() const
    {
        return std::string_view(value, N);
    }
};

namespace detail
{
template <StringLiteral s, typename Idxs>
struct sl2cs_impl;

template <StringLiteral s, std::size_t... Idxs>
struct sl2cs_impl<s, std::index_sequence<Idxs...>>
{
    using type = char_sequence<s[Idxs]...>;
};
} // namespace detail

/// @brief Convert StringLiteral into char_sequence.
template <StringLiteral s>
using sl2cs =
    typename detail::sl2cs_impl<s, std::make_index_sequence<s.size()>>::type;
/// @}

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
    using type = typename join<type_sequence<Ts...>,
                               typename repeat<N - 1, Ts...>::type>::type;
};

template <typename... Ts>
struct repeat<0, Ts...>
{
    using type = type_sequence<>;
};

template <typename List>
struct length;

template <typename... Ts>
struct length<type_sequence<Ts...>>
{
    static constexpr std::size_t value = sizeof...(Ts);
};

template <std::size_t I, typename List>
struct at;

template <typename T, typename... Ts>
struct at<0, type_sequence<T, Ts...>>
{
    using type = T;
};

template <std::size_t I, typename T, typename... Ts>
    requires(I < length<type_sequence<T, Ts...>>::value)
struct at<I, type_sequence<T, Ts...>>
{
    using type = typename at<I - 1, type_sequence<Ts...>>::type;
};

template <typename List>
struct tail;

template <typename T, typename... Ts>
struct tail<type_sequence<T, Ts...>>
{
    using type = type_sequence<Ts...>;
};

template <typename T, typename List>
struct elem;

template <typename T, typename... Ts>
struct elem<T, type_sequence<Ts...>>
{
    static constexpr bool value = (false || ... || std::is_same_v<T, Ts>);
};

template <std::size_t I, typename S, typename List>
struct index;

template <std::size_t I, typename S>
struct index<I, S, type_sequence<>>
{
    static constexpr std::size_t value = -1ul;
};

template <std::size_t I, typename S, typename T, typename... Ts>
struct index<I, S, type_sequence<T, Ts...>>
{
    static constexpr std::size_t value =
        std::is_same_v<S, T> ? I :
                               index<I + 1ul, S, type_sequence<Ts...>>::value;
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
    using type = typename foldl<F, F<T, Acc>, type_sequence<Ts...>>::type;
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
    using type = typename drop<N - 1, type_sequence<Ts...>>::type;
};

template <typename List1, typename List2>
struct zip;

template <typename X, typename Y>
struct zip<type_sequence<X>, type_sequence<Y>>
{
    using type = type_sequence<type_sequence<X, Y>>;
};

template <typename X, typename Y, typename... Xs, typename... Ys>
struct zip<type_sequence<X, Xs...>, type_sequence<Y, Ys...>>
{
    using type = typename cons<
        type_sequence<X, Y>,
        typename zip<type_sequence<Xs...>, type_sequence<Ys...>>::type>::type;
};

template <typename List>
struct same;

template <typename T, typename... Ts>
struct same<type_sequence<T, Ts...>>
{
    static constexpr bool value = std::conjunction_v<std::is_same<T, Ts>...>;
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
struct has_duplicates<type_sequence<>>
{
    static constexpr bool value = false;
};

template <typename T, typename... Ts>
struct has_duplicates<type_sequence<T, Ts...>>
{
    using ts = type_sequence<Ts...>;
    static constexpr bool value =
        elem<T, ts>::value || has_duplicates<ts>::value;
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
    using rest = typename filter<P, type_sequence<Ts...>>::type;
    using type =
        std::conditional_t<P<T>::value, typename cons<T, rest>::type, rest>;
};

template <typename T, typename List>
class purge
{
    template <typename Elem>
    using f = std::negation<std::is_same<T, Elem>>;

  public:
    using type = typename filter<f, List>::type;
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
    using type = typename cons<
        T,
        typename nub<typename purge<T, type_sequence<Ts...>>::type>::type>::
        type;
};

template <typename T, typename Map>
struct lookup;

template <typename T>
struct lookup<T, type_sequence<>>
{
    using type = void;
};

template <typename T, typename Key, typename Value, typename... Ts>
struct lookup<T, type_sequence<type_sequence<Key, Value>, Ts...>>
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

template <template <typename> typename P, typename Ms, typename Rs, typename Xs>
struct split_with;

template <template <typename> typename P, typename Ms, typename Rs>
struct split_with<P, Ms, Rs, type_sequence<>>
{
    using type = typename join<Ms, type_sequence<Rs>>::type;
};

template <template <typename> typename P, typename Ms, typename Rs, typename X, typename... Xs>
struct split_with<P, Ms, Rs, type_sequence<X, Xs...>>
{
    using type = std::conditional_t<
        P<X>::value,
        typename split_with<P,
                       typename join<Ms, type_sequence<Rs>>::type,
                       type_sequence<>,
                       type_sequence<Xs...>>::type,
        typename split_with<P,
                       Ms,
                       typename join<Rs, type_sequence<X>>::type,
                       type_sequence<Xs...>>::type>;
};

template <typename D, typename List>
class split
{
    template <typename U>
    using eq = std::is_same<D, U>;

  public:
    using type =
        typename split_with<eq, type_sequence<>, type_sequence<>, List>::type;
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
} // namespace detail

/// @defgroup Typelist Typelist
/// @brief List operations on type sequences.
/// @ingroup TMP
/// @{

/// @brief Add class @p T to the beginning of type_sequence @p List.
template <typename T, typename List>
using cons = typename detail::cons<T, List>::type;

/// @brief Concatenate two lists of types.
template <typename List1, typename List2>
using join = typename detail::join<List1, List2>::type;

/// @brief Add class @p T to the end of type_sequence @p List.
template <typename T, typename List>
using append = join<List, type_sequence<T>>;

/// @brief Constructs type_sequence repeating classes @p Ts @p N times.
template <std::size_t N, typename... Ts>
using repeat = typename detail::repeat<N, Ts...>::type;

/// @brief Number of types in @p List.
template <typename List>
inline constexpr std::size_t length = detail::length<List>::value;

/// @brief Extract the @p Ith class from @p List.
template <std::size_t I, typename List>
using at = typename detail::at<I, List>::type;

/// @brief Extract the first class from @p List.
template <typename List>
using head = at<0, List>;

/// @brief Extract the last element from @p List.
template <typename List>
using last = at<length<List> - 1, List>;

/// @brief Extract the elements after the head of @p list as type_sequence.
template <typename List>
using tail = typename detail::tail<List>::type;

/// @brief Check whether @p T occur in @p List.
template <typename T, typename List>
inline constexpr bool elem = detail::elem<T, List>::value;

/// @brief The index of the first occurence of type @p T.
/// If there is no such type, the value is -1ul.
template <typename T, typename List>
inline constexpr std::size_t index = detail::index<0ul, T, List>::value;

/// @brief std::index_sequence for @p List.
template <typename List>
using indicies = std::make_index_sequence<length<List>>;

/// @brief Left-associative fold of a type_sequence @p List.
template <template <typename, typename> typename F, typename Acc, typename List>
using foldl = typename detail::foldl<F, Acc, List>::type;

/// @brief Right-associative fold of a type_sequence @p List.
template <template <typename, typename> typename F, typename Acc, typename List>
using foldr = typename detail::foldr<F, Acc, List>::type;

/// @brief Reverse the order of elements.
template <typename List>
using reverse = foldr<append, type_sequence<>, List>;

/// @brief Apply @p F to each element of @p List.
template <template <typename> typename F, typename List>
using map = typename detail::map<F, List>::type;

/// @brief Remove all occurences of type @p T from @p List.
///
/// Does nothing if there is no @p T in @p List.
template <typename T, typename List>
using purge = typename detail::purge<T, List>::type;

/// @brief Prefix of @p List of length @p N.
///
/// @p N must be less than or equal of length of @p List.
template <std::size_t N, typename List>
using take = typename detail::take<N, List>::type;

/// @brief Suffix of @p List after the first @p N elements.
///
/// @p N must be less than or equal of length of @p List.
template <std::size_t N, typename List>
using drop = typename detail::drop<N, List>::type;

/// @brief Make list of pairs type_sequence<type_sequence<X, Y>...> of
/// corresponding types from lists @p Xs and @p Ys.
///
/// @p Xs and @p Ys must be of the same length.
template <typename Xs, typename Ys>
using zip = typename detail::zip<Xs, Ys>::type;

/// @brief The concatenation of all the elements of a list @p List of lists.
template <typename List>
using concat = foldr<join, type_sequence<>, List>;

/// @brief Check whether the classes of @p List are the same.
template <typename List>
inline constexpr bool same = detail::same<List>::value;

/// @brief Considering the classes of type_sequence @p Fs as functions
/// get the list of return types by applying the functions to
/// arguments of types @p Args.
///
/// @tparam Fs type_sequence of callable types.
/// @tparam Args Arguments to apply function from @p Fs to.
template <typename Fs, typename... Args>
using result_of = typename detail::result_of<Fs, Args...>;

/// @brief Check if there are duplicates in @p List.
template <typename List>
inline constexpr bool has_duplicates = detail::has_duplicates<List>::value;

/// @brief Remove duplicate elements from @p List.
template <typename List>
using nub = typename detail::nub<List>::type;

/// @brief Filter @p List elements with a predicate @p P.
///
/// @tparam P must have static constexpr bool value member.
///
/// The resulting type_sequence contains elements that
/// satisfy P, i.e. P<T>::value == true.
template <template <typename> typename P, typename List>
using filter = typename detail::filter<P, List>::type;

/// @brief Lookup a key @p Key in association list @p Map.
///
/// @tparam Map consists of key-value pairs type_sequence<Key, Value>.
///
/// If @p Key not found the resulting type is void.
template <typename Key, typename Map>
using lookup = typename detail::lookup<Key, Map>::type;

/// @brief Get the leftmost type from @p List matching predicate @p P.
///
/// @tparam P must have static constexpr bool value member.
///
/// If there is no type matching @p P the resulting type is void.
template <template <typename> typename P, typename List>
using find = typename detail::find<P, List>::type;

/// @brief Check if @p List is empty.
template <typename List>
inline constexpr bool empty = length<List> == 0;

/// @brief Split typelist into sublists with a predicate.
///
/// @tparam P has static constexpr member @p value.
///
/// The delimeter elements are such that satisfy predicate @p P,
/// i.e. P<T>::value == true.
template <template <typename> typename P, typename List>
using split_with =
    purge<type_sequence<>,
          typename detail::
              split_with<P, type_sequence<>, type_sequence<>, List>::type>;

/// @brief Split typelist into sublists with a delimeter.
///
/// The resulting type is type_sequence<type_sequence<...>...>.
template <typename Delimeter, typename List>
using split =
    purge<type_sequence<>, typename detail::split<Delimeter, List>::type>;

/// @brief Convert a class template @p Source into type_sequence.
template <typename Source>
using from = typename detail::from<Source>::type;

/// @brief Convert type_sequence @p Source to @p Target class template.
template <typename Source, template <typename...> typename Target>
using to = typename detail::to<Source, Target>::type;

/// @brief Convert type_sequence of std::integer_sequence into a single
/// std::integer_sequence.
///
/// The value type of the first element is used as the value type for the
/// resulting std::integer_sequence.
template <typename List>
using to_integer_sequence = typename detail::to_integer_seq<List>::type;

/// @brief Invoke @p f with each class of @p List as template parameter.
template <typename F, typename... Ts>
constexpr void
for_each(F f, type_sequence<Ts...>) noexcept((...&& noexcept(f.template
                                                             operator()<Ts>())))
{
    (..., f.template operator()<Ts>());
}

/// @brief Invoke every functional object in a typelist with @p args.
template <typename... Fs, typename... Args>
constexpr void
unroll(type_sequence<Fs...>, Args&&... args) noexcept
{
    (..., std::invoke(Fs{}, std::forward<Args>(args)...));
}
/// @}
} // namespace typelist

namespace intlist
{
namespace detail
{
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
    using type = typename join<std::integer_sequence<Int, Vals...>,
                               typename repeat<N, Int, Val>::type>::type;
};

template <typename List>
struct length;

template <typename Int, Int... Nums>
struct length<std::integer_sequence<Int, Nums...>>
{
    static constexpr std::size_t value = sizeof...(Nums);
};

template <std::size_t I, typename List>
struct at;

template <typename Int, Int Head, Int... Vals>
struct at<0, std::integer_sequence<Int, Head, Vals...>>
{
    static constexpr Int value = Head;
};

template <std::size_t I, typename Int, Int Head, Int... Vals>
    requires(I < length<std::integer_sequence<Int, Head, Vals...>>::value)
struct at<I, std::integer_sequence<Int, Head, Vals...>>
{
    static constexpr Int value =
        at<I - 1, std::integer_sequence<Int, Vals...>>::value;
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
struct elem<V, std::integer_sequence<Int, Is...>>
{
    static constexpr bool value = (false || ... || (V == Is));
};

template <std::size_t I, auto S, typename List>
struct index;

template <std::size_t I, typename Int, Int S>
struct index<I, S, std::integer_sequence<Int>>
{
    static constexpr std::size_t value = -1ul;
};

template <std::size_t I, typename Int, Int S, Int X, Int... Xs>
struct index<I, S, std::integer_sequence<Int, X, Xs...>>
{
    static constexpr std::size_t value =
        S == X ? I :
                 index<I + 1ul, S, std::integer_sequence<Int, Xs...>>::value;
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
    using type = std::integer_sequence<R, f(Is)...>;
};

template <template <auto> typename F, typename List>
struct map_t;

template <typename Int, template <Int> typename F, Int... Is>
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
    using type = typename cons<
        I,
        typename take<N - 1, std::integer_sequence<Int, Is...>>::type>::type;
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
    using type = typename drop<N - 1, std::integer_sequence<Int, Is...>>::type;
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
struct has_duplicates<std::integer_sequence<Int, I>>
{
    static constexpr bool value = false;
};

template <typename Int, Int X, Int... Xs>
struct has_duplicates<std::integer_sequence<Int, X, Xs...>>
{
    using xs = std::integer_sequence<Int, Xs...>;
    static constexpr bool value =
        elem<X, xs>::value || has_duplicates<xs>::value;
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
    using type = typename cons<
        X,
        typename nub<typename purge<X, std::integer_sequence<Int, Xs...>>::
                         type>::type>::type;
};
} // namespace detail

/// @defgroup Intlist Intlist
/// @brief Compile-time list operations on std::integer_sequence.
/// @ingroup TMP
/// @{

/// @brief Add integer value @p Val to the beginning of @p List.
template <auto Val, typename List>
using cons = typename detail::cons<Val, List>::type;

/// @brief Concatenate two std::integer_sequence.
template <typename List1, typename List2>
using join = typename detail::join<List1, List2>::type;

/// @brief Constructs std::integer_sequence repeating @p Vals @p N
/// times.
template <std::size_t N, typename Int, Int... Vals>
using repeat = typename detail::repeat<N, Int, Vals...>::type;

/// @brief Reverse the order of the elements in @p List.
template <typename List>
using reverse = typename detail::reverse<List>::type;

/// @brief Apply @p F to each element of @p List.
///
/// @f$\mathrm{F}(\mathrm{I}) \rightarrow \mathrm{T}@f$,
/// The resulting type @f$\mathrm{T}@f$ must be integer.
template <typename F, typename List>
using map_f = typename detail::map_f<F, List>::type;

/// @brief Make types from elements of @p List by
/// specializing template class F with the values.
///
/// The resulting type is type_sequence<F<Int>...>.
template <template <auto> typename F, typename List>
using map_t = typename detail::map_t<F, List>::type;

/// @brief Add @p Val @p N times to the end of @p List.
template <typename List, std::size_t N, auto Val>
using nappend = typename detail::nappend<List, N, Val>::type;

/// @brief Add @p Val to the end of @p List.
template <typename List, auto Val>
using append = nappend<List, 1, Val>;

/// @brief Length of an std::integer_sequence.
template <typename List>
inline constexpr std::size_t length = detail::length<List>::value;

/// @brief Extract the @p Ith elementh from @p List.
template <std::size_t I, typename List>
inline constexpr auto at = detail::at<I, List>::value;

/// @brief Extract the first element from @p List.
template <typename List>
inline constexpr auto head = at<0, List>;

/// @brief Extract the last element from @p List.
template <typename List>
inline constexpr auto last = at<length<List> - 1, List>;

/// @brief Extract the elements after the head of @p list as type_sequence.
template <typename List>
using tail = typename detail::tail<List>::type;

/// @brief Check whether @p X occur in @p List.
template <auto X, typename List>
inline constexpr bool elem = detail::elem<X, List>::value;

/// @brief The index of the first occurence of @p X.
/// If there is no @p X in @p List the value is -1ul.
template <auto X, typename List>
inline constexpr std::size_t index = detail::index<0ul, X, List>::value;

/// @brief std::index_sequence for @p List.
template <typename List>
using indicies = std::make_index_sequence<length<List>>;

/// @brief Make std::integer_sequence without @p X.
template <auto X, typename List>
using purge = typename detail::purge<X, List>::type;

/// @brief Prefix of @p List of length @p N.
///
/// @p N must be less than or equal of length of @p List.
template <std::size_t N, typename List>
using take = typename detail::take<N, List>::type;

/// @brief Suffix of @p List after the first @p N elements.
///
/// @p N must be less than or equal of length of @p List.
template <std::size_t N, typename List>
using drop = typename detail::drop<N, List>::type;

/// @brief Convert std::integer_sequence into C array.
template <typename List>
inline constexpr auto array = detail::array<List>::value;

/// @brief Convert std::integer_sequence into std::array.
template <typename List>
inline constexpr auto stdarray = detail::stdarray<List>::value;

/// @brief Check if there are duplicates in @p List.
template <typename List>
inline constexpr bool has_duplicates = detail::has_duplicates<List>::value;

/// @brief Remove duplicate elements from @p List.
template <typename List>
using nub = typename detail::nub<List>::type;

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

template <typename Idxs, typename FacNum>
struct lehmer_perm_impl;

template <typename Idxs>
struct lehmer_perm_impl<Idxs, std::index_sequence<>>
{
    using type = std::index_sequence<>;
};

template <typename Idxs, std::size_t Head, std::size_t... Rest>
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
using lehmer_code = typename lehmer_code_impl<N, 1>::type;

/// @brief Generate a permutation of index array @p Idxs from
/// its factorial representation @p FacNum.
template <typename Idxs, typename FacNum>
using lehmer_perm = typename lehmer_perm_impl<Idxs, FacNum>::type;

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
using nlperm = typename nlperm_impl<N, L>::type;
/// @}
} // namespace intlist
} // namespace lucid
