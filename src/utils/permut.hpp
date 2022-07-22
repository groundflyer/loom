// -*- C++ -*-
// permut.hpp
//

#pragma once

#include "intlist.hpp"
#include "math.hpp"

namespace lucid
{
namespace il = intlist;

/// @brief Factorial representation of number @p N.
///
/// See https://en.wikipedia.org/wiki/Factorial_number_system
/// and https://en.wikipedia.org/wiki/Lehmer_code
template <std::size_t N, std::size_t I>
struct lehmer_code
{
  private:
    static constexpr std::size_t quot = N / I;
    static constexpr std::size_t rem  = N % I;

  public:
    using type = il::append_t<typename lehmer_code<quot, I + 1>::type, rem>;
};

template <std::size_t I>
struct lehmer_code<0, I>
{
    using type = std::index_sequence<>;
};

template <std::size_t N>
using lehmer_code_t = typename lehmer_code<N, 1>::type;

/// @brief Generates a permutation of index array @p Idxs from
/// its factorial representation @p FacNum.
template <typename Idxs, typename FacNum>
struct lehmer_perm;

template <typename Idxs>
struct lehmer_perm<Idxs, std::index_sequence<>>
{
    using type = std::index_sequence<>;
};

template <typename Idxs, std::size_t Head, std::size_t... Rest>
struct lehmer_perm<Idxs, std::index_sequence<Head, Rest...>>
{
  private:
    static constexpr std::size_t _elem = il::elem_v<Head, Idxs>;

  public:
    using type = il::cons_t<
        _elem,
        typename lehmer_perm<il::del_t<Idxs, _elem>, std::index_sequence<Rest...>>::type>;
};

template <typename Idxs, typename FacNum>
using lehmer_perm_t = typename lehmer_perm<Idxs, FacNum>::type;

/// @brief @p Nth permutation in lexicographical order
/// of an index sequence of length @p L.
template <std::size_t N, std::size_t L>
struct nlperm
{
    static_assert(N < fac(L), "N should be less than L!");

  private:
    using _lehmer_code                = lehmer_code_t<N>;
    static constexpr std::size_t diff = L - il::length_v<_lehmer_code>;

  public:
    using type = lehmer_perm_t<std::make_index_sequence<L>,
                               il::join_t<il::repeat_t<diff, std::size_t, 0ul>, _lehmer_code>>;
};

template <std::size_t N, std::size_t L>
using nlperm_t = typename nlperm<N, L>::type;
} // namespace lucid
