// -*- C++ -*-
// seq.hpp
//

#pragma once

#include "functional.hpp"
#include "tmp.hpp"

#include <variant>

namespace lucid
{

namespace detail
{
template <typename Tuple, std::size_t... Ids>
constexpr auto
enumerate_impl(const Tuple& tuple, std::index_sequence<Ids...>) noexcept
{
    return std::tuple{
        std::pair<std::size_t, std::tuple_element_t<Ids, Tuple>>{
                                                                 Ids, std::get<Ids>(tuple)}
          ...
    };
}

template <std::size_t Idx, typename Visitor, typename Tuple, typename Default>
constexpr decltype(auto)
visit_impl(const std::size_t case_,
           Visitor&&         visitor,
           Tuple&&           tuple,
           Default&&         def)
{
    if(Idx == case_)
        return std::invoke(std::forward<Visitor>(visitor),
                           std::get<Idx>(tuple));

    if constexpr(Idx < std::tuple_size_v<std::decay_t<Tuple>> - 1)
        return visit_impl<Idx + 1>(case_,
                                   std::forward<Visitor>(visitor),
                                   std::forward<Tuple>(tuple),
                                   std::forward<Default>(def));
    else
        return std::invoke(std::forward<Default>(def));
}

template <std::size_t I, typename T, std::size_t N1, std::size_t N2>
constexpr decltype(auto)
array_cat_idx(const std::array<T, N1>& first, const std::array<T, N2>& second)
{
    if constexpr(I < N1)
        return std::get<I>(first);
    else
        return std::get<I - N1>(second);
}

template <typename T, std::size_t N1, std::size_t N2, std::size_t... Is>
constexpr auto
array_cat_impl(const std::array<T, N1>& first,
               const std::array<T, N2>& second,
               std::index_sequence<Is...>) noexcept
{
    return std::array<T, N1 + N2>{array_cat_idx<Is>(first, second)...};
}

template <typename F, typename Tuple, std::size_t... I>
constexpr decltype(auto)
apply_impl(F&& f, Tuple&& tuple, std::index_sequence<I...>) noexcept
{
    return std::invoke(f, tuple.template get<I>()...);
}

template <typename F, typename Tuple, std::size_t... I>
constexpr decltype(auto)
apply_impl(F&& f, Tuple& tuple, std::index_sequence<I...>) noexcept
{
    return std::invoke(f, tuple.template get<I>()...);
}

template <typename F, typename Tuple, std::size_t... I>
constexpr void
for_each_impl(F&& f, Tuple&& tuple, std::index_sequence<I...>)
{
    (..., std::invoke(std::forward<F>(f), std::get<I>(tuple)));
}
} // namespace detail

/// @brief Left-assotiative fold of a @p tuple.
template <typename BinaryOp, typename T, typename... Ts>
constexpr decltype(auto)
fold_tuple(BinaryOp&&               op,
           const T&                 init,
           const std::tuple<Ts...>& tuple) noexcept
{
    return std::apply([&](const Ts&... args) noexcept
                      { return fold(op, init, args...); },
                      tuple);
}

/// @brief Left-assotiative fold of a non-empty @p tuple.
template <typename BinaryOp, typename Tuple>
constexpr decltype(auto)
fold_tuple(BinaryOp&& op, const Tuple& tuple) noexcept
{
    return std::apply([&](const auto&... args) noexcept
                      { return fold(op, args...); },
                      tuple);
}

/// @brief Make tuple of pairs (i, elem).
template <typename... Ts>
constexpr decltype(auto)
enumerate(const std::tuple<Ts...>& tuple) noexcept
{
    return detail::enumerate_impl(tuple, std::index_sequence_for<Ts...>{});
}

/// @brief Invoke @p visitor on the @p case_'th element of @p tuple.
///
/// @return Value of either std::variant comprising of all possible
/// return types of @p Visitor(Ts)... or a singular type if all the resulting
/// types of @p Visitor(Ts)... are the same.
///
/// @param case_ which @p tuple element to invoke @p visitor on.
/// @param visitor callable to invoke. Must be able to take @p tuple elements
/// as arguments.
/// @param tuple contains elements to invoke @p visitor on.
/// @param def is invoked if @p case_ is out of range.
template <typename Visitor, typename Tuple, typename Default>
constexpr decltype(auto)
visit(const std::size_t case_,
      Visitor&&         visitor,
      Tuple&&           tuple,
      Default&&         defval)
{
    return detail::visit_impl<0>(case_,
                                 std::forward<Visitor>(visitor),
                                 std::forward<Tuple>(tuple),
                                 std::forward<Default>(defval));
}

/// @brief Concatenate arrays.
template <typename T, std::size_t N1, std::size_t N2, typename... Rest>
constexpr auto
array_cat(const std::array<T, N1>& first,
          const std::array<T, N2>& second,
          const Rest&... rest) noexcept
{
    const auto ret =
        detail::array_cat_impl(first,
                               second,
                               std::make_index_sequence<N1 + N2>{});

    if constexpr(sizeof...(rest)) return array_cat(ret, rest...);

    return ret;
}

/// @brief Create an std::tuple of size @p N consisting only of values @p value.
template <std::size_t N, typename T>
constexpr auto
repeat_to_tuple(const T& value) noexcept
{
    if constexpr(N == 1)
        return std::tuple{value};
    else
        return std::tuple_cat(std::tuple{value},
                              repeat_to_tuple<(N - 1)>(value));
}

/// @brief Create std::array of size @p N consisting only of values @p value.
template <std::size_t N, typename T>
constexpr auto
repeat_to_array(const T& value) noexcept
{
    if constexpr(N == 1)
        return std::array<T, 1>{value};
    else
        return array_cat(std::array<T, 1>{value},
                         repeat_to_array<(N - 1)>(value));
}

/// @brief Make an array of size @p N consisting of elements made by applying @p
/// g to @p args.
template <size_t N, typename G, typename... Args>
constexpr std::array<std::invoke_result_t<G, Args...>, N>
generate(G&& g, Args&&... args) noexcept
{
    std::array<std::invoke_result_t<G, Args...>, N> ret{};

    for(auto& v: ret) v = std::invoke(g, args...);

    return ret;
}

template <typename F, typename... Ts, template <typename...> typename Tuple>
constexpr decltype(auto)
apply(F&& f, Tuple<Ts...>&& tuple)
{
    return detail::apply_impl(f,
                              std::forward<Tuple<Ts...>>(tuple),
                              std::index_sequence_for<Ts...>{});
}

template <typename F, typename... Ts, template <typename...> typename Tuple>
constexpr decltype(auto)
apply(F&& f, Tuple<Ts...>& tuple) noexcept
{
    return detail::apply_impl(f, tuple, std::index_sequence_for<Ts...>{});
}

/// @brief Invoke @p F on every element of @p tuple.
template <typename F, typename Tuple>
constexpr void
for_each(F&& f, Tuple&& tuple)
{
    detail::for_each_impl(
        std::forward<F>(f),
        std::forward<Tuple>(tuple),
        std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
}
} // namespace lucid
