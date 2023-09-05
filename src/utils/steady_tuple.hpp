// -*- C++ -*-
/// @file steady_tuple.hpp
/// @brief Tuple for non-default-constructible objects.

#pragma once

#include "tmp.hpp"

#include <tuple>

namespace lucid
{
/// @brief Empty class tag type to indicate steady_tuple
/// to build all the containing objects using the same provided arguments.
struct uniform_init_t
{
};

/// @brief Instance of uniform_init_t.
inline constexpr uniform_init_t uniform_init;

namespace detail
{
template <std::size_t I, typename T>
class steady_tuple_leaf
{
    T value;

  public:
    steady_tuple_leaf()                         = delete;
    steady_tuple_leaf(const steady_tuple_leaf&) = delete;
    steady_tuple_leaf(steady_tuple_leaf&&)      = delete;
    steady_tuple_leaf&
    operator=(const steady_tuple_leaf&) = delete;
    steady_tuple_leaf&
    operator=(steady_tuple_leaf&&) = delete;

    template <typename ArgsTuple>
    constexpr steady_tuple_leaf(std::piecewise_construct_t, ArgsTuple&& args) :
        value(std::make_from_tuple<T>(args))
    {
    }

    template <typename... Args>
    constexpr steady_tuple_leaf(Args&&... args) : value(args...)
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
class steady_tuple_impl;

template <std::size_t... Idxs, typename... Ts>
class steady_tuple_impl<std::index_sequence<Idxs...>, Ts...> :
    public steady_tuple_leaf<Idxs, Ts>...
{
  public:
    steady_tuple_impl()                         = delete;
    steady_tuple_impl(const steady_tuple_impl&) = delete;
    steady_tuple_impl(steady_tuple_impl&&)      = delete;
    steady_tuple_impl&
    operator=(const steady_tuple_impl&) = delete;
    steady_tuple_impl&
    operator=(steady_tuple_impl&&) = delete;

    template <std::size_t... LeafIdxs, typename... LeafTs, typename... ValueTs>
    constexpr steady_tuple_impl(std::index_sequence<LeafIdxs...>,
                                type_sequence<LeafTs...>,
                                ValueTs&&... values) :
        steady_tuple_leaf<LeafIdxs, LeafTs>(values)...
    {
    }

    template <std::size_t... LeafIdxs, typename... LeafTs, typename... ValueTs>
    constexpr steady_tuple_impl(std::index_sequence<LeafIdxs...>,
                                type_sequence<LeafTs...>,
                                std::piecewise_construct_t _pc,
                                ValueTs&&... values) :
        steady_tuple_leaf<LeafIdxs, LeafTs>(_pc, values)...
    {
    }

    template <std::size_t... LeafIdxs, typename... LeafTs, typename... ValueTs>
    constexpr steady_tuple_impl(std::index_sequence<LeafIdxs...>,
                                type_sequence<LeafTs...>,
                                uniform_init_t,
                                ValueTs&&... values) :
        steady_tuple_leaf<LeafIdxs, LeafTs>(values...)...
    {
    }
};

template <typename F, typename Tuple, std::size_t... I>
constexpr decltype(auto)
apply_impl(F&& f, Tuple&& tuple, std::index_sequence<I...>) noexcept(
    noexcept(std::invoke(std::forward<F>(f), tuple.template get<I>()...)))
{
    return std::invoke(std::forward<F>(f), tuple.template get<I>()...);
}
} // namespace detail

/// @brief Tuple class template
/// for objects that aren't default-, copy- or move-constructible.
template <typename... Ts>
class steady_tuple
{
    using Types = type_sequence<Ts...>;
    using Idxs  = typelist::indicies<Types>;
    using Data  = detail::steady_tuple_impl<Idxs, Ts...>;

    static constexpr Types types{};
    static constexpr Idxs  idxs{};

    Data data;

  public:
    steady_tuple()                    = delete;
    steady_tuple(const steady_tuple&) = delete;
    steady_tuple(steady_tuple&&)      = delete;
    steady_tuple&
    operator=(const steady_tuple&) = delete;
    steady_tuple&
    operator=(steady_tuple&&) = delete;

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
    constexpr steady_tuple(Args&&... args) : data(idxs, types, args...)
    {
    }

    /// @brief Access an underlying object by index @p I.
    template <std::size_t I>
    constexpr decltype(auto)
    get() const noexcept
    {
        return static_cast<
                   const detail::steady_tuple_leaf<I, typelist::at<I, Types>>&>(
                   data)
            .get();
    }

    /// @brief Access an underlying object by index @p I.
    template <std::size_t I>
    constexpr decltype(auto)
    get() noexcept
    {
        return static_cast<
                   detail::steady_tuple_leaf<I, typelist::at<I, Types>>&>(data)
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
                   detail::steady_tuple_leaf<typelist::index<T, Types>, T>&>(
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
                   detail::steady_tuple_leaf<typelist::index<T, Types>, T>&>(
                   data)
            .get();
    }
};

template <typename Tuple>
struct tuple_size;

template <typename... Ts>
struct tuple_size<steady_tuple<Ts...>> :
    std::integral_constant<std::size_t, sizeof...(Ts)>
{
};

/// @brief The number of elements in steady_tuple.
template <typename Tuple>
inline constexpr std::size_t tuple_size_v = tuple_size<Tuple>::value;

/// @brief std::apply analogue for steady_tuple.
template <typename F, typename Tuple>
constexpr decltype(auto)
apply(F&& f, Tuple&& tuple) noexcept(noexcept(detail::apply_impl(
    std::forward<F>(f),
    std::forward<Tuple>(tuple),
    std::make_index_sequence<tuple_size_v<std::decay_t<Tuple>>>{})))
{
    return detail::apply_impl(
        std::forward<F>(f),
        std::forward<Tuple>(tuple),
        std::make_index_sequence<tuple_size_v<std::decay_t<Tuple>>>{});
}

} // namespace lucid
