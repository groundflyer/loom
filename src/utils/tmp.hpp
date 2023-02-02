// -*- C++ -*-
/// @file tmp.hpp
/// @brief Template metaprogramming.

#pragma once

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
/// while type aliases and variable templates are visible.

/// @defgroup Typelist Typelist
/// @brief List operations on type sequences.
/// @ingroup TMP
/// @{

/// @brief Sequence of types.
template <typename... Ts>
struct type_sequence
{
};

/// @brief Compile-time string.
template <char... Cs>
using char_sequence = std::integer_sequence<char, Cs...>;

namespace typelist
{
namespace detail
{
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
struct at<I, type_sequence<T, Ts...>>
{
    static_assert(I < length<type_sequence<T, Ts...>>::value,
                  "Index out of range");
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

template <std::size_t idx, typename Sought, typename T, typename... Ts>
constexpr std::size_t
index_impl() noexcept
{
    if constexpr(std::is_same_v<Sought, T>)
        return idx;
    else if constexpr(sizeof...(Ts) > 0ul)
        return index_impl<idx + 1, Sought, Ts...>();
    else
        static_assert(sizeof...(Ts) > 0ul, "Type not found.");
}

template <typename T, typename List>
struct index;

template <typename T, typename H, typename... Ts>
struct index<T, type_sequence<H, Ts...>>
{
    static constexpr std::size_t value = index_impl<0, T, H, Ts...>();
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
struct take<N, type_sequence<T, Ts...>>
{
    static_assert(N <= length<type_sequence<T, Ts...>>::value);

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
struct drop<N, type_sequence<T, Ts...>>
{
    static_assert(N <= length<type_sequence<T, Ts...>>::value);

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

template <typename T, typename List>
struct del;

template <typename T, typename X>
struct del<T, type_sequence<X>>
{
    using type = std::
        conditional_t<std::is_same_v<T, X>, type_sequence<>, type_sequence<X>>;
};

template <typename T, typename X, typename... Xs>
struct del<T, type_sequence<X, Xs...>>
{
    using type = std::conditional_t<
        std::is_same_v<T, X>,
        typename del<T, type_sequence<Xs...>>::type,
        typename cons<X, typename del<T, type_sequence<Xs...>>::type>::type>;
};

template <typename List>
struct same;

template <typename T, typename... Ts>
struct same<type_sequence<T, Ts...>>
{
    static constexpr bool value = (true && ... && std::is_same_v<T, Ts>);
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

template <typename T>
struct has_duplicates<type_sequence<T>>
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

template <template <typename> typename F, typename List>
struct filter;

template <template <typename> typename F, typename T>
struct filter<F, type_sequence<T>>
{
    using type =
        std::conditional_t<F<T>::value, type_sequence<T>, type_sequence<>>;
};

template <template <typename> typename F, typename T, typename... Ts>
struct filter<F, type_sequence<T, Ts...>>
{
    using rest = typename filter<F, type_sequence<Ts...>>::type;
    using type =
        std::conditional_t<F<T>::value, typename cons<T, rest>::type, rest>;
};
} // namespace detail

/// @brief Convert a class template @p Source into type_sequence.
template <typename Source>
using from = typename detail::from<Source>::type;

/// @brief Convert type_sequence @p Source to @p Target class template.
template <typename Source, template <typename...> typename Target>
using to = typename detail::to<Source, Target>::type;

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

/// @brief Length of type_sequence.
template <typename List>
constexpr std::size_t length = detail::length<List>::value;

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
constexpr bool elem = detail::elem<T, List>::value;

/// @brief The index of the first element which is the same as @p T.
template <typename T, typename List>
constexpr std::size_t index = detail::index<T, List>::value;

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
using del = typename detail::del<T, List>::type;

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
constexpr bool same = detail::same<List>::value;

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
constexpr bool has_duplicates = detail::has_duplicates<List>::value;

/// @brief Filter @p List elements with @ F.
///
/// @tparam F must have static constexpr bool value member;
template <template <typename> typename F, typename List>
using filter = typename detail::filter<F, List>::type;

/// @brief Check if @p List is empty.
template <typename List>
constexpr bool empty = length<List> == 0;

/// @brief Invoke @p f with each class of @p List as template parameter.
template <typename F, typename... Ts>
constexpr void
for_each(F f, type_sequence<Ts...>) noexcept((true &&
                                              ...&& noexcept(f.template
                                                             operator()<Ts>())))
{
    (..., f.template operator()<Ts>());
}
/// @}
} // namespace typelist

namespace intlist
{
/// @defgroup Intlist Intlist
/// @brief Compile-time list operations on std::integer_sequence.
/// @ingroup TMP
/// @{
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
struct at<I, std::integer_sequence<Int, Head, Vals...>>
{
    static_assert(I < length<std::integer_sequence<Int, Head, Vals...>>::value,
                  "Index out of range");
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

template <std::size_t idx, typename Int, Int S, Int X, Int... Is>
constexpr std::size_t
index_impl() noexcept
{
    if constexpr(S == X)
        return idx;
    else if constexpr(sizeof...(Is) > 0ul)
        return index_impl<idx + 1, Int, S, Is...>();
    else
        static_assert(sizeof...(Is) > 0ul, "Type not found.");
}

template <auto Val, typename List>
struct index;

template <typename Int, Int Val, Int... Is>
struct index<Val, std::integer_sequence<Int, Is...>>
{
    static constexpr std::size_t value = index_impl<0, Int, Val, Is...>();
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
struct map;

template <typename F, typename Int, Int... Is>
struct map<F, std::integer_sequence<Int, Is...>>
{
  private:
    static constexpr F f{};
    using R = std::invoke_result_t<F, Int>;

  public:
    using type = std::integer_sequence<R, f(Is)...>;
};

template <std::size_t N, typename List>
struct take;

template <typename Int, Int I, Int... Is>
struct take<1, std::integer_sequence<Int, I, Is...>>
{
    using type = std::integer_sequence<Int, I>;
};

template <std::size_t N, typename Int, Int I, Int... Is>
struct take<N, std::integer_sequence<Int, I, Is...>>
{
    static_assert(N <= length<std::integer_sequence<Int, I, Is...>>::value);

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
struct drop<N, std::integer_sequence<Int, I, Is...>>
{
    static_assert(N <= length<std::integer_sequence<Int, I, Is...>>::value);

    using type = typename drop<N - 1, std::integer_sequence<Int, Is...>>::type;
};

template <auto Val, typename List>
struct del;

template <typename Int, Int Last, Int Val>
struct del<Val, std::integer_sequence<Int, Last>>
{
    using type = std::conditional_t<Last == Val,
                                    std::integer_sequence<Int>,
                                    std::integer_sequence<Int, Last>>;
};

template <typename Int, Int Val, Int Head, Int... Rest>
struct del<Val, std::integer_sequence<Int, Head, Rest...>>
{
  private:
    using rest = std::integer_sequence<Int, Rest...>;

  public:
    using type = std::conditional_t<
        Head == Val,
        typename del<Val, rest>::type,
        typename cons<Head, typename del<Val, rest>::type>::type>;
};

template <typename List>
struct array;

template <typename Int, Int... Is>
struct array<std::integer_sequence<Int, Is...>>
{
    static constexpr Int value[sizeof...(Is)] = {Is...};
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
} // namespace detail

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
using map = typename detail::map<F, List>::type;

/// @brief Add @p Val @p N times to the end of @p List.
template <typename List, std::size_t N, auto Val>
using nappend = typename detail::nappend<List, N, Val>::type;

/// @brief Add @p Val to the end of @p List.
template <typename List, auto Val>
using append = nappend<List, 1, Val>;

/// @brief Length of an std::integer_sequence.
template <typename List>
constexpr std::size_t length = detail::length<List>::value;

/// @brief Extract the @p Ith elementh from @p List.
template <std::size_t I, typename List>
constexpr auto at = detail::at<I, List>::value;

/// @brief Extract the first element from @p List.
template <typename List>
constexpr auto head = at<0, List>;

/// @brief Extract the last element from @p List.
template <typename List>
constexpr auto last = at<length<List> - 1, List>;

/// @brief Extract the elements after the head of @p list as type_sequence.
template <typename List>
using tail = typename detail::tail<List>::type;

/// @brief Check whether @p X occur in @p List.
template <auto X, typename List>
constexpr bool elem = detail::elem<X, List>::value;

/// @brief The index of the first element which equals @p X.
template <auto X, typename List>
constexpr std::size_t index = detail::index<X, List>::value;

/// @brief std::index_sequence for @p List.
template <typename List>
using indicies = std::make_index_sequence<length<List>>;

/// @brief Make std::integer_sequence without @p X.
template <auto X, typename List>
using del = typename detail::del<X, List>::type;

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
constexpr auto array = detail::array<List>::value;

/// @brief Check if there are duplicates in @p List.
template <typename List>
constexpr bool has_duplicates = detail::has_duplicates<List>::value;

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
             typename lehmer_perm_impl<del<elem, Idxs>,
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
struct nlperm_impl
{
    static_assert(N < fac(L), "N should be less than (L!)");

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
