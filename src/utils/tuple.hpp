// -*- C++ -*-
/// @file tuple.hpp
/// @brief Auxiliary utilities for tuples.

#pragma once

#include "functional.hpp"
#include "tmp.hpp"

#include <concepts>
#include <tuple>
#include <variant>

namespace lucid
{
template <typename... Ts>
class unique_tuple;
}

namespace std
{
template <typename... Ts>
class tuple_size<lucid::unique_tuple<Ts...>> :
    public integral_constant<size_t, sizeof...(Ts)>
{
};

template <size_t I, typename... Ts>
class tuple_element<I, lucid::unique_tuple<Ts...>>
{
  public:
    using type = lucid::typelist::at<I, lucid::type_sequence<Ts...>>;
};

template <size_t I, typename... Ts>
constexpr decltype(auto)
get(const lucid::unique_tuple<Ts...>& t) noexcept
{
    return t.template get<I>();
}

template <size_t I, typename... Ts>
constexpr decltype(auto)
get(lucid::unique_tuple<Ts...>& t) noexcept
{
    return t.template get<I>();
}
} // namespace std

namespace lucid
{
/// @brief Empty class tag type to indicate unique_tuple
/// to build all the containing objects using the same provided arguments.
struct uniform_init_t
{
};

/// @brief Instance of uniform_init_t.
inline constexpr uniform_init_t uniform_init;

namespace detail
{
template <std::size_t I, typename T>
class unique_tuple_leaf
{
    T value;

  public:
    unique_tuple_leaf()                         = delete;
    unique_tuple_leaf(const unique_tuple_leaf&) = delete;
    unique_tuple_leaf&
    operator=(const unique_tuple_leaf&) = delete;

    constexpr unique_tuple_leaf(unique_tuple_leaf&& other) noexcept :
        value(std::move(other.value))
    {
    }

    constexpr unique_tuple_leaf&
    operator=(unique_tuple_leaf&& other) noexcept
    {
        value = std::move(other.value);
        return *this;
    }

    template <typename ArgsTuple>
    constexpr unique_tuple_leaf(std::piecewise_construct_t,
                                ArgsTuple&& args) noexcept :
        value(std::make_from_tuple<T>(std::forward<ArgsTuple>(args)))
    {
    }

    template <typename... Args>
    constexpr unique_tuple_leaf(Args&&... args) noexcept :
        value(std::forward<Args>(args)...)
    {
    }

    constexpr T&
    get() noexcept
    {
        return value;
    }

    const constexpr T&
    get() const noexcept
    {
        return value;
    }
};

template <typename Idxs, typename... Ts>
class unique_tuple_impl;

template <std::size_t... Idxs, typename... Ts>
class unique_tuple_impl<std::index_sequence<Idxs...>, Ts...> :
    public unique_tuple_leaf<Idxs, Ts>...
{
    template <std::size_t I, typename T>
    constexpr void
    assign(unique_tuple_leaf<I, T>&& other) noexcept
    {
        static_cast<unique_tuple_leaf<I, T>&>(*this) = std::move(other);
    }

  public:
    unique_tuple_impl()                         = delete;
    unique_tuple_impl(const unique_tuple_impl&) = delete;
    unique_tuple_impl&
    operator=(const unique_tuple_impl&) = delete;

    constexpr unique_tuple_impl(unique_tuple_impl&& other) noexcept :
        unique_tuple_leaf<Idxs, Ts>(
            std::move(static_cast<unique_tuple_leaf<Idxs, Ts>&&>(other)))...
    {
    }

    constexpr unique_tuple_impl&
    operator=(unique_tuple_impl&& other) noexcept
    {
        (..., assign<Idxs, Ts>(std::move(other)));
        return *this;
    }

    template <typename... ValueTs>
    constexpr unique_tuple_impl(ValueTs&&... values) noexcept :
        unique_tuple_leaf<Idxs, Ts>(std::forward<ValueTs>(values))...
    {
    }

    template <typename... ValueTs>
    constexpr unique_tuple_impl(std::piecewise_construct_t _pc,
                                ValueTs&&... values) noexcept :
        unique_tuple_leaf<Idxs, Ts>(_pc, std::forward<ValueTs>(values))...
    {
    }

    template <typename... ValueTs>
    constexpr unique_tuple_impl(uniform_init_t, ValueTs&&... values) noexcept :
        unique_tuple_leaf<Idxs, Ts>(std::forward<ValueTs>(values)...)...
    {
    }
};

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

template <std::unsigned_integral I,
          I                      idx,
          typename Visitor,
          typename Tuple,
          typename Default>
constexpr decltype(auto)
visit_at_impl(const I i, Visitor&& visitor, Tuple&& tuple, Default&& def)
{
    if(idx == i)
        return std::invoke(std::forward<Visitor>(visitor),
                           std::get<idx>(std::forward<Tuple>(tuple)));

    if constexpr(idx < std::tuple_size_v<std::remove_cvref_t<Tuple>> - 1)
        return visit_at_impl<I, idx + 1>(i,
                                         std::forward<Visitor>(visitor),
                                         std::forward<Tuple>(tuple),
                                         std::forward<Default>(def));
    else
        return std::invoke(std::forward<Default>(def));
}

template <std::size_t I, typename T, std::size_t N1, std::size_t N2>
constexpr decltype(auto)
array_cat_idx(const std::array<T, N1>& first,
              const std::array<T, N2>& second) noexcept
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

template <typename F, typename Tuple, typename... Args, std::size_t... I>
constexpr void
for_each_impl(F&&     f,
              Tuple&& tuple,
              Args&&... args,
              std::index_sequence<
                  I...>) noexcept(std::
                                      conjunction_v<std::is_nothrow_invocable<
                                          F,
                                          std::tuple_element_t<
                                              I,
                                              std::remove_cvref_t<Tuple>>,
                                          std::remove_cvref_t<Args>...>...>)
{
    (...,
     std::invoke(std::forward<F>(f),
                 std::get<I>(tuple),
                 std::forward<Args>(args)...));
}

template <std::size_t I, typename F, typename... Ts>
constexpr decltype(auto)
multi_invoke(F&& f, Ts&&... tuples) noexcept(
    std::is_nothrow_invocable_v<
        F,
        std::tuple_element_t<I, std::remove_cvref_t<Ts>>...>)
{
    return std::invoke(std::forward<F>(f), std::get<I>(tuples)...);
}

template <typename F, typename... Ts, std::size_t... I>
constexpr void
zip_with_for_each_impl(
    std::index_sequence<I...>,
    F&& f,
    Ts&&... tuples) noexcept((...&& noexcept(multi_invoke<I>(std::forward<F>(f),
                                                             std::forward<Ts>(
                                                                 tuples)...))))
{
    (..., multi_invoke<I>(std::forward<F>(f), std::forward<Ts>(tuples)...));
}

template <typename F, typename T, template <typename...> typename M>
struct is_applicable_mod
{
    static constexpr bool value = false;
};

template <typename F, template <typename...> typename M, typename... Ts>
struct is_applicable_mod<F, unique_tuple<Ts...>, M>
{
    static constexpr bool value = M<F, Ts...>::value;
};

template <typename F, template <typename...> typename M, typename... Ts>
struct is_applicable_mod<F, std::tuple<Ts...>, M>
{
    static constexpr bool value = M<F, Ts...>::value;
};

template <typename F,
          template <typename...>
          typename M,
          typename T1,
          typename T2>
struct is_applicable_mod<F, std::pair<T1, T2>, M>
{
    static constexpr bool value = M<F, T1, T2>::value;
};

template <typename F, typename R, typename T, template <typename...> typename M>
struct is_applicable_mod_r
{
    static constexpr bool value = false;
};

template <typename F,
          typename R,
          template <typename...>
          typename M,
          typename... Ts>
struct is_applicable_mod_r<F, R, unique_tuple<Ts...>, M>
{
    static constexpr bool value = M<F, R, Ts...>::value;
};

template <typename F,
          typename R,
          template <typename...>
          typename M,
          typename... Ts>
struct is_applicable_mod_r<F, R, std::tuple<Ts...>, M>
{
    static constexpr bool value = M<F, R, Ts...>::value;
};

template <typename F,
          typename R,
          template <typename...>
          typename M,
          typename T1,
          typename T2>
struct is_applicable_mod_r<F, R, std::pair<T1, T2>, M>
{
    static constexpr bool value = M<F, R, T1, T2>::value;
};

template <typename T>
struct is_tuple
{
    static constexpr bool value = false;
};

template <typename... Ts>
struct is_tuple<std::pair<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>>
{
    static constexpr bool value = true;
};

template <typename... Ts>
struct is_tuple<unique_tuple<Ts...>>
{
    static constexpr bool value = true;
};
} // namespace detail

/// @brief Check if tuple.
template <typename T>
inline constexpr bool is_tuple_v =
    detail::is_tuple<std::remove_cvref_t<T>>::value;

template <typename F, typename T>
inline constexpr bool is_applicable_v = detail::
    is_applicable_mod<F, std::remove_cvref_t<T>, std::is_invocable>::value;

template <typename F, typename T>
inline constexpr bool is_nothrow_applicable_v =
    detail::is_applicable_mod<F,
                              std::remove_cvref_t<T>,
                              std::is_nothrow_invocable>::value;

template <typename F, typename T>
inline constexpr bool is_maybe_nothrow_applicable_v =
    is_nothrow_applicable_v<F, T> || std::is_nothrow_invocable_v<F, T>;

template <typename F, typename R, typename T>
inline constexpr bool is_applicable_r_v =
    detail::is_applicable_mod_r<F,
                                R,
                                std::remove_cvref_t<T>,
                                std::is_invocable_r>::value;

template <typename F, typename R, typename T>
inline constexpr bool is_nothrow_applicable_r_v =
    detail::is_applicable_mod_r<F,
                                R,
                                std::remove_cvref_t<T>,
                                std::is_nothrow_invocable_r>::value;

template <typename F, typename R, typename T>
inline constexpr bool is_maybe_nothrow_applicable_r_v =
    is_nothrow_applicable_r_v<F, R, T> ||
    std::is_nothrow_invocable_r_v<F, R, T>;

template <typename F, typename T>
concept maybe_applicable = is_applicable_v<F, T> || std::is_invocable_v<F, T>;

template <typename F, typename R, typename T>
concept maybe_applicable_r =
    is_applicable_r_v<F, R, T> || std::is_invocable_r_v<F, R, T>;

/// @brief Tuple class template
/// for objects that aren't default- or copy-constructible.
template <typename... Ts>
class unique_tuple
{
    using Types = type_sequence<Ts...>;
    using Idxs  = typelist::indices<Types>;
    using Data  = detail::unique_tuple_impl<Idxs, Ts...>;

    Data data;

  public:
    unique_tuple()                    = delete;
    unique_tuple(const unique_tuple&) = delete;
    unique_tuple&
    operator=(const unique_tuple&) = delete;

    constexpr unique_tuple(unique_tuple&& other) noexcept :
        data(std::move(other.data))
    {
    }

    /// @brief Constructs all the containing objects in place from @p args.
    ///
    /// By default, each containing object is constructed from
    /// the corresponding argument.
    ///
    /// Pass lucid::uniform_init as the first argument to indicate
    /// that all the containing elements should be constructed
    /// from the same remaining arguments.
    ///
    /// Pass std::piecewise_construct as the first argument
    /// or after lucid::uniform_init to indicate that
    /// the provided arguments are
    /// tuples and should be unpacked for constructing corresponding objects,
    /// i. e. uses std::make_from_tuple to construct the objects.
    template <typename... Args>
    constexpr unique_tuple(Args&&... args) noexcept :
        data(std::forward<Args>(args)...)
    {
    }

    constexpr unique_tuple&
    operator=(unique_tuple&& other) noexcept
    {
        data = std::move(other.data);
        return *this;
    }

    /// @brief Access an underlying object by index @p I.
    template <std::size_t I>
    constexpr decltype(auto)
    get() const noexcept
    {
        return static_cast<
                   const detail::unique_tuple_leaf<I, typelist::at<I, Types>>&>(
                   data)
            .get();
    }

    /// @brief Access an underlying object by index @p I.
    template <std::size_t I>
    constexpr decltype(auto)
    get() noexcept
    {
        return static_cast<
                   detail::unique_tuple_leaf<I, typelist::at<I, Types>>&>(data)
            .get();
    }

    /// @brief Access an underlying object of type @p T.
    ///
    /// @p T should be unique.
    template <typename T>
    constexpr decltype(auto)
    get() const noexcept
    {
        return static_cast<
                   detail::unique_tuple_leaf<typelist::index<T, Types>, T>&>(
                   data)
            .get();
    }

    /// @brief Access an underlying object of type @p T.
    ///
    /// @p T should be unique.
    template <typename T>
    constexpr decltype(auto)
    get() noexcept
    {
        return static_cast<
                   detail::unique_tuple_leaf<typelist::index<T, Types>, T>&>(
                   data)
            .get();
    }
};

// @brief Apply @p op to every element of @p tuple and return result as a tuple.
template <template <typename...> typename ResultTuple = std::tuple,
          typename UnaryOp,
          typename InputTuple>
constexpr decltype(auto)
map_tuple(InputTuple&& tuple, UnaryOp&& op)
{
    return std::apply([op = std::move(op)]<typename... Ts>(Ts&&... args)
                          -> ResultTuple<std::invoke_result_t<UnaryOp, Ts>...>
                      { return {std::invoke(op, std::forward<Ts>(args))...}; },
                      std::forward<InputTuple>(tuple));
}

/// @brief Left-assotiative fold of a @p tuple.
template <typename BinaryOp, typename T, typename... Ts>
constexpr decltype(auto)
fold_tuple(const std::tuple<Ts...>& tuple, BinaryOp&& op, const T& init)
{
    return std::apply([&](const Ts&... args)
                      { return fold(op, init, args...); },
                      tuple);
}

/// @brief Left-assotiative fold of a non-empty @p tuple.
template <typename BinaryOp, typename Tuple>
constexpr decltype(auto)
fold_tuple(const Tuple& tuple, BinaryOp&& op)
{
    return std::apply([&](const auto&... args) { return fold(op, args...); },
                      tuple);
}

/// @brief Make tuple of pairs (i, elem).
template <typename... Ts>
constexpr decltype(auto)
enumerate(const std::tuple<Ts...>& tuple) noexcept
{
    return detail::enumerate_impl(tuple, std::index_sequence_for<Ts...>{});
}

/// @brief Invoke @p visitor on the @p i'th element of @p tuple.
///
/// @param i which @p tuple element to invoke @p visitor on.
/// @param visitor callable to invoke. Must be able to take @p tuple elements
/// as arguments.
/// @param tuple contains elements to invoke @p visitor on.
/// @param def function that invoked without arguments if @p i out of range.
template <std::unsigned_integral I,
          typename Visitor,
          typename Tuple,
          typename Default>
constexpr decltype(auto)
visit_at(const I i, Visitor&& visitor, Tuple&& tuple, Default&& def)
{
    return detail::visit_at_impl<I, 0>(i,
                                       std::forward<Visitor>(visitor),
                                       std::forward<Tuple>(tuple),
                                       std::forward<Default>(def));
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

    if constexpr(sizeof...(rest))
        return array_cat(ret, rest...);
    else
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

/// @brief Make an array of size @p N consisting of elements
/// made by applying @p g to @p args.
template <size_t N, typename G, typename... Args>
constexpr std::array<std::invoke_result_t<G, Args...>, N>
generate(G&& g, Args&&... args) noexcept
{
    std::array<std::invoke_result_t<G, Args...>, N> ret{};

    for(auto& v: ret)
        v = std::invoke(std::forward<G>(g), std::forward<Args>(args)...);

    return ret;
}

#define _FOR_EACH_EXPR               \
    detail::for_each_impl(           \
        std::forward<F>(f),          \
        std::forward<Tuple>(tuple),  \
        std::forward<Args>(args)..., \
        std::make_index_sequence<    \
            std::tuple_size_v<std::remove_cvref_t<Tuple>>>{})

#define _ZIP_WITH_FOR_EACH_EXPR                               \
    detail::zip_with_for_each_impl(                           \
        std::make_index_sequence<                             \
            std::tuple_size_v<std::remove_cvref_t<Tuple>>>{}, \
        std::forward<F>(f),                                   \
        std::forward<Tuple>(tuple),                           \
        std::forward<Rest>(tuples)...)

/// @brief Invoke @p F on every element of @p tuple.
template <typename F, typename Tuple, typename... Args>
constexpr void
for_each(Tuple&& tuple,
         F&&     f,
         Args&&... args) noexcept(noexcept(_FOR_EACH_EXPR))
{
    _FOR_EACH_EXPR;
}

/// @brief Invoke @p F on every correspondent element of @p tuple and @p tuples.
template <typename F, typename Tuple, typename... Rest>
    requires(... && (std::tuple_size_v<std::remove_cvref_t<Tuple>> ==
                     std::tuple_size_v<std::remove_cvref_t<Rest>>))
constexpr void
zip_with_for_each(Tuple&& tuple,
                  F&&     f,
                  Rest&&... tuples) noexcept(noexcept(_ZIP_WITH_FOR_EACH_EXPR))
{
    _ZIP_WITH_FOR_EACH_EXPR;
}

#undef _FOR_EACH_EXPR
#undef _ZIP_WITH_FOR_EACH_EXPR

/// @brief Invoke @p f with elements of @p mt as arguments if
/// possible otherwise just pass @p mt directly to @p f.
template <typename F, typename MaybeTuple>
constexpr decltype(auto)
maybe_apply(F&& f, MaybeTuple&& mt)
{
    if constexpr(is_applicable_v<F, std::remove_cvref_t<MaybeTuple>>)
        return std::apply(std::forward<F>(f), std::forward<MaybeTuple>(mt));
    else
        return std::invoke(std::forward<F>(f), std::forward<MaybeTuple>(mt));
}
} // namespace lucid
