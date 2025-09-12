// -*- C++ -*-
/// @file bve.hpp
/// @brief Basic vector evaluator

#pragma once

#include "core.hpp"

#include <utils/common.hpp>
#include <utils/debug.hpp>
#include <utils/functional.hpp>
#include <utils/tuple.hpp>

#include <range/v3/range/operations.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/zip_with.hpp>

#include <algorithm>
#include <compare>
#include <ranges>
#include <span>
#include <vector>

namespace lucid::bve
{
// division and modulo for 32 bit unsigned integer can be faster
using fsize_t = std::uint32_t;

constexpr inline fsize_t dynamic_extent = -1;

template <typename T,
          template <typename, typename> typename Vector,
          template <typename> typename Allocator>
struct vector_union
{
    using type = Vector<T, Allocator<T>>;
};

template <typename... Ts,
          template <typename, typename> typename Vector,
          template <typename> typename Allocator>
struct vector_union<Union<Ts...>, Vector, Allocator>
{
    using type = UnionVector<type_sequence<Ts...>, Allocator<std::byte>>;
};

template <typename T,
          template <typename, typename> typename Vector,
          template <typename> typename Allocator>
using vector_union_t = vector_union<T, Vector, Allocator>::type;

template <typename Data, typename Helper, typename _BP>
class ProxyRow
{
    Data&   m_data;
    fsize_t m_pos;

    void
    copy_values(const ProxyRow& rhs) noexcept
    {
        intlist::for_each(
            []<std::size_t I>(ProxyRow& lhs, const ProxyRow& rhs) noexcept
            {
                std::get<I>(lhs.m_data)[lhs.m_pos] =
                    std::get<I>(rhs.m_data)[rhs.m_pos];
            },
            std::make_index_sequence<std::tuple_size_v<Data>>{},
            *this,
            rhs);
    }

  public:
    FORBID_DEFAULT(ProxyRow);

    ProxyRow(const ProxyRow& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos)
    {
    }

    ProxyRow(ProxyRow&& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos)
    {
    }

    ProxyRow(Data& data, const fsize_t pos) noexcept : m_data(data), m_pos(pos)
    {
    }

    ProxyRow&
    operator=(const ProxyRow& rhs) noexcept
    {
        copy_values(rhs);

        return *this;
    }

    ProxyRow&
    operator=(ProxyRow&& rhs) noexcept
    {
        copy_values(rhs);

        return *this;
    }

    bool
    operator==(const ProxyRow rhs) const noexcept
    {
        return bp() == rhs.bp();
    }

    std::strong_ordering
    operator<=>(const ProxyRow rhs) const noexcept
    {
        return bp() <=> rhs.bp();
    }

    template <typename Var>
    std::add_const_t<std::add_lvalue_reference_t<vartype<Var>>>
    get() const noexcept
    {
        return Helper::template get<Var>(m_data)[m_pos];
    }

    std::add_const_t<std::add_lvalue_reference_t<vartype<_BP>>>
    bp() const noexcept
    {
        return get<_BP>();
    }

    template <typename Var>
    std::add_lvalue_reference_t<vartype<Var>>
    get() noexcept
    {
        return Helper::template get<Var>(m_data)[m_pos];
    }

    std::add_lvalue_reference_t<vartype<_BP>>
    bp() noexcept
    {
        return get<_BP>();
    }

    friend void
    swap(ProxyRow a, ProxyRow b) noexcept
    {
        lucid::for_each(a.m_data,
                        [&](auto& c) noexcept
                        { std::swap(c[a.m_pos], c[b.m_pos]); });
    }
};

template <typename Data, typename Helper, typename _BP, fsize_t N>
class ProxyView
{
    Data&   m_data;
    fsize_t m_pos;

    fsize_t
    shift(fsize_t p) const noexcept
    {
        CHECK_INDEX(p, N);
        return m_pos + p;
    }

  public:
    FORBID_DEFAULT(ProxyView);

    ProxyView&
    operator=(const ProxyView& rhs) = delete;
    ProxyView&
    operator=(ProxyView&&) = delete;

    ProxyView(const ProxyView& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos)
    {
    }
    ProxyView(ProxyView&& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos) {};

    ProxyView(Data& data, fsize_t pos) noexcept : m_data(data), m_pos(pos) {}

    ProxyRow<Data, Helper, _BP>
    operator[](const fsize_t n) const noexcept
    {
        return {m_data, shift(n)};
    }

    template <fsize_t N2>
        requires(N2 <= N)
    ProxyView<Data, Helper, _BP, N2>
    view(const fsize_t pos = 0) const noexcept
    {
        ASSERT(N2 <= N - pos, "Extent too big");
        return {m_data, shift(pos)};
    }

    ProxyView<Data, Helper, _BP, dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        ASSERT(ext <= N - pos, "Extent too big");
        return {m_data, shift(pos), ext};
    }

    ProxyView<Data, Helper, _BP, dynamic_extent>
    view(const fsize_t pos = 0) const noexcept
    {
        return view(pos, size() - pos);
    }

    template <typename Var>
    std::span<vartype<Var>, N>
    get() const noexcept
    {
        return std::span<vartype<Var>, N>(
            std::addressof(Helper::template get<Var>(m_data)[m_pos]),
            N);
    }

    std::span<vartype<_BP>, N>
    bp() const noexcept
    {
        return get<_BP>();
    }

    constexpr fsize_t
    size() const noexcept
    {
        return N;
    }

    fsize_t
    offset() const noexcept
    {
        return m_pos;
    }
};

template <typename Data, typename Helper, typename _BP>
class ProxyView<Data, Helper, _BP, dynamic_extent>
{
    Data&   m_data;
    fsize_t m_pos;
    fsize_t m_ext;

    fsize_t
    shift(fsize_t p) const noexcept
    {
        CHECK_INDEX(p, size());
        return m_pos + p;
    }

  public:
    FORBID_DEFAULT(ProxyView);
    ProxyView&
    operator=(const ProxyView& rhs) = delete;
    ProxyView&
    operator=(ProxyView&&) = delete;

    ProxyView(const ProxyView& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext)
    {
    }
    ProxyView(ProxyView&& other) noexcept :
        m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext) {};

    ProxyView(Data& data, fsize_t pos, fsize_t ext) noexcept :
        m_data(data), m_pos(pos), m_ext(ext)
    {
    }

    ProxyRow<Data, Helper, _BP>
    operator[](const fsize_t n) const noexcept
    {
        return {m_data, shift(n)};
    }

    template <fsize_t N>
    ProxyView<Data, Helper, _BP, N>
    view(const fsize_t pos) const noexcept
    {
        ASSERT(N <= size() - pos, "Extent too big");
        return {m_data, shift(pos)};
    }

    ProxyView<Data, Helper, _BP, dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, shift(pos), ext};
    }

    ProxyView<Data, Helper, _BP, dynamic_extent>
    view(const fsize_t pos = 0) const noexcept
    {
        return view(pos, size() - pos);
    }

    template <typename Var>
    std::span<vartype<Var>, std::dynamic_extent>
    get() const noexcept
    {
        return {std::addressof(Helper::template get<Var>(m_data)[m_pos]),
                m_ext};
    }

    std::span<vartype<_BP>, std::dynamic_extent>
    bp() const noexcept
    {
        return get<_BP>();
    }

    fsize_t
    size() const noexcept
    {
        return m_ext;
    }

    fsize_t
    offset() const noexcept
    {
        return m_pos;
    }
};

template <typename Data, typename Helper, typename _BP>
class MemoryIterator : public std::contiguous_iterator_tag
{
    Data*   p_data = nullptr;
    fsize_t m_pos{};
    fsize_t m_size{};

  public:
    using difference_type = std::ptrdiff_t;
    using value_type      = ProxyRow<Data, Helper, _BP>;

    MemoryIterator() noexcept = default;

    MemoryIterator(Data* data, fsize_t pos, fsize_t size) noexcept :
        p_data(data), m_pos(pos), m_size(size)
    {
    }
    MemoryIterator(const MemoryIterator& other) noexcept :
        p_data(other.p_data), m_pos(other.m_pos), m_size(other.m_size)
    {
    }
    MemoryIterator(MemoryIterator&& other) noexcept :
        p_data(other.p_data), m_pos(other.m_pos), m_size(other.m_size)
    {
    }

    MemoryIterator&
    operator=(const MemoryIterator& rhs) noexcept
    {
        p_data = rhs.p_data;
        m_pos  = rhs.m_pos;
        m_size = rhs.m_size;

        return *this;
    }

    MemoryIterator&
    operator=(MemoryIterator&& rhs) noexcept
    {
        p_data = rhs.p_data;
        m_pos  = rhs.m_pos;
        m_size = rhs.m_size;

        return *this;
    }

    bool
    operator==(std::default_sentinel_t) const noexcept
    {
        return m_pos == m_size;
    }

    bool
    operator==(const MemoryIterator& rhs) const noexcept
    {
        return m_pos == rhs.m_pos;
    }

    std::strong_ordering
    operator<=>(const MemoryIterator& rhs) const noexcept
    {
        return m_pos <=> rhs.m_pos;
    }

    value_type
    operator*() const noexcept
    {
        CHECK_INDEX(m_pos, m_size);
        return {*p_data, m_pos};
    }

    MemoryIterator&
    operator++() noexcept
    {
        ++m_pos;
        return *this;
    }

    MemoryIterator
    operator++(int) noexcept
    {
        MemoryIterator ret(*this);
        ++m_pos;
        return ret;
    }

    MemoryIterator&
    operator--() noexcept
    {
        --m_pos;
        return *this;
    }

    MemoryIterator
    operator--(int) noexcept
    {
        MemoryIterator ret(*this);
        --m_pos;
        return ret;
    }

    MemoryIterator
    operator+(difference_type rhs) const noexcept
    {
        return {p_data, m_pos + static_cast<fsize_t>(rhs), m_size};
    }

    difference_type
    operator+(MemoryIterator rhs) const noexcept
    {
        return m_pos + rhs.m_pos;
    }

    MemoryIterator&
    operator+=(difference_type rhs) noexcept
    {
        m_pos += rhs;
        return *this;
    }

    MemoryIterator
    operator-(difference_type rhs) const noexcept
    {
        return {p_data, static_cast<fsize_t>(m_pos - rhs), m_size};
    }

    difference_type
    operator-(MemoryIterator rhs) const noexcept
    {
        return m_pos - rhs.m_pos;
    }

    MemoryIterator&
    operator-=(difference_type rhs) noexcept
    {
        m_pos -= rhs;
        return *this;
    }

    value_type
    operator[](difference_type n) const noexcept
    {
        CHECK_INDEX(n, count());
        return {*p_data, static_cast<fsize_t>(m_pos + n)};
    }

    fsize_t
    count() const noexcept
    {
        return m_size - m_pos;
    }

    friend MemoryIterator
    operator+(difference_type n, MemoryIterator j) noexcept
    {
        return {j.p_data, n + j.m_pos, j.m_size};
    }
};

template <typename RegTypes,
          typename RegNames,
          typename _BP,
          template <typename, typename> typename _Vector = std::vector,
          template <typename> typename Allocator         = std::allocator>
class Memory
{
    template <typename T>
    using Vector = _Vector<T, Allocator<T>>;

    template <typename T>
    using f = vector_union_t<T, _Vector, Allocator>;

    using helper = MemoryHelper<RegTypes, RegNames, std::tuple, f>;

    using Data = helper::data;

    Data m_data;

    template <fsize_t N>
    using proxy_view = ProxyView<Data, helper, _BP, N>;

  public:
    using value_type = bve::ProxyRow<Data, helper, _BP>;
    using view_type  = proxy_view<dynamic_extent>;
    using iterator   = bve::MemoryIterator<Data, helper, _BP>;

    Memory() noexcept = default;
    explicit Memory(fsize_t n) { resize(n); }

    value_type
    operator[](const fsize_t n) noexcept
    {
        CHECK_INDEX(n, size());
        return {m_data, n};
    }

    value_type
    operator[](const fsize_t n) const noexcept
    {
        CHECK_INDEX(n, size());
        return {m_data, n};
    }

    template <fsize_t N>
    proxy_view<N>
    view(const fsize_t pos = 0) noexcept
    {
        CHECK_INDEX(pos, size());
        ASSERT(N <= size() - pos, "Extent too big");
        return {m_data, pos};
    }

    template <fsize_t N>
    proxy_view<N>
    view(const fsize_t pos = 0) const noexcept
    {
        CHECK_INDEX(pos, size());
        ASSERT(N <= size() - pos, "Extent too big");
        return {m_data, pos};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) noexcept
    {
        CHECK_INDEX(pos, size());
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, pos, ext};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        CHECK_INDEX(pos, size());
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, pos, ext};
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos = 0) noexcept
    {
        CHECK_INDEX(pos, size());
        return view(pos, size() - pos);
    }

    proxy_view<dynamic_extent>
    view(const fsize_t pos = 0) const noexcept
    {
        CHECK_INDEX(pos, size());
        return view(pos, size() - pos);
    }

    iterator
    begin() noexcept
    {
        return {&m_data, 0ul, size()};
    }

    iterator
    end() noexcept
    {
        return {&m_data, size(), size()};
    }

    iterator
    begin() const noexcept
    {
        return {&m_data, 0ul, size()};
    }

    iterator
    end() const noexcept
    {
        return {&m_data, size(), size()};
    }

    fsize_t
    size() const noexcept
    {
        return std::get<0>(m_data).size();
    }

    template <typename Var>
    std::span<vartype<Var>>
    get() noexcept
    {
        return {helper::template get<Var>(m_data).data(), size()};
    }

    template <typename Var>
    std::span<vartype<Var>>
    get() const noexcept
    {
        return {const_cast<std::add_pointer_t<vartype<Var>>>(
                    helper::template get<Var>(m_data).data()),
                size()};
    }

    std::span<vartype<_BP>>
    bp() noexcept
    {
        return get<_BP>();
    }

    std::span<vartype<_BP>>
    bp() const noexcept
    {
        return get<_BP>();
    }

    void
    resize(fsize_t new_size)
    {
        lucid::for_each(m_data,
                        [new_size](auto& c) noexcept { c.resize(new_size); });
    }

    fsize_t
    offset() const noexcept
    {
        return 0;
    }

    friend iterator
    begin(Memory& m) noexcept
    {
        return m.begin();
    }

    friend std::default_sentinel_t
    end(Memory&) noexcept
    {
        return std::default_sentinel;
    }

    friend fsize_t
    size(Memory& m) noexcept
    {
        return m.size();
    }

    friend iterator
    begin(const Memory& m) noexcept
    {
        return m.begin();
    }

    friend std::default_sentinel_t
    end(const Memory&) noexcept
    {
        return std::default_sentinel;
    }

    friend fsize_t
    size(const Memory& m) noexcept
    {
        return m.size();
    }
};

template <std::input_iterator                                          I,
          std::predicate<typename std::iterator_traits<I>::value_type> P>
I
swap_only_partition(I first, I last, P p) noexcept
{
    first = std::find_if_not(first, last, p);
    if(first == last) return first;

    for(auto i = std::next(first); i != last; ++i)
        if(p(*i)) std::iter_swap(i, first++);

    return first;
}

template <std::random_access_iterator                                  I,
          std::predicate<typename std::iterator_traits<I>::value_type,
                         typename std::iterator_traits<I>::value_type> Cmp =
              std::less<typename std::iterator_traits<I>::value_type>>
void
swap_only_make_heap(I first, I last, Cmp cmp = Cmp{}) noexcept
{
    using difference_type = std::iterator_traits<I>::difference_type;

    difference_type n = last - first;

    for(difference_type i = 1; i < n; ++i)
    {
        I child_i  = first + i;
        I parent_i = first + (i - 1) / 2;

        if(cmp(*parent_i, *child_i))
        {
            difference_type j        = i;
            I               child_j  = first + j;
            I               parent_j = first + (j - 1) / 2;

            while(cmp(*parent_j, *child_j))
            {
                std::iter_swap(child_j, parent_j);
                j        = (j - 1) / 2;
                child_j  = first + j;
                parent_j = first + (j - 1) / 2;
            }
        }
    }
}

template <std::random_access_iterator                                  I,
          std::predicate<typename std::iterator_traits<I>::value_type,
                         typename std::iterator_traits<I>::value_type> Cmp =
              std::less<typename std::iterator_traits<I>::value_type>>
void
swap_only_heap_sort(I first, I last, Cmp cmp = Cmp{}) noexcept
{
    using difference_type = std::iterator_traits<I>::difference_type;

    difference_type n = last - first;

    for(difference_type i = n; i > 1; --i)
    {
        std::iter_swap(first, first + i - 1);

        difference_type j   = 0;
        difference_type idx = 1;

        while(idx < i - 1)
        {
            if(idx + 1 < i - 1 && cmp(*(first + idx), *(first + idx + 1)))
                ++idx;

            if(!cmp(*(first + j), *(first + idx))) break;

            std::iter_swap(first + j, first + idx);
            j   = idx;
            idx = 2 * j + 1;
        }
    }
}

template <std::random_access_iterator                                  I,
          std::predicate<typename std::iterator_traits<I>::value_type,
                         typename std::iterator_traits<I>::value_type> Cmp =
              std::less<typename std::iterator_traits<I>::value_type>>
void
swap_only_insertion_sort(I first, I last, Cmp cmp = Cmp{}) noexcept
{
    for(auto i = std::next(first); i != last; ++i)
        for(auto j = i; (j != first) && cmp(*j, *(std::prev(j))); --j)
            std::iter_swap(j, std::prev(j));
}

template <std::random_access_iterator I>
    requires(
        std::three_way_comparable<typename std::iterator_traits<I>::value_type>)
I
median_of_3(I a, I b, I c) noexcept
{
    const bool alb = *a < *b;
    const bool blc = *b < *c;
    if(alb && blc) return b;

    const bool alc  = *a < *c;
    const bool cleb = *c <= *b;
    if(alc && cleb) return c;

    const bool blea = *b <= *a;
    if(alc && blea) return a;

    const bool clea = *c <= *a;
    if(blc && clea) return c;

    if(clea && alb) return a;

    // if(cleb && blea)
    return b;
}

template <std::random_access_iterator I>
void
swap_only_introsort_impl(I first, I last, const unsigned depth) noexcept
{
    using difference_type = std::iterator_traits<I>::difference_type;
    if(first == last) return;
    const difference_type     len       = last - first;
    constexpr difference_type ins_limit = 24;

    if(len <= ins_limit) return swap_only_insertion_sort(first, last);

    if(depth == 0u)
    {
        swap_only_make_heap(first, last);
        return swap_only_heap_sort(first, last);
    }

    I pivot = median_of_3(first, first + len / 2, last - 1);
    I m     = swap_only_partition(first,
                              last,
                              [p = *pivot](auto val) { return val < p; });

    swap_only_introsort_impl(first, m, depth - 1);
    swap_only_introsort_impl(m, last, depth - 1);
}

template <std::random_access_iterator I>
    requires(
        std::three_way_comparable<typename std::iterator_traits<I>::value_type>)
void
swap_only_introsort(I first, I last) noexcept
{
    swap_only_introsort_impl(
        first,
        last,
        2u * static_cast<unsigned>(std::floor(std::log(last - first))));
}

template <auto halt_pos, auto num_bins, std::integral T>
constexpr T
fit_in_bins(T bp) noexcept
{
    return bp != halt_pos ? bp : num_bins - 1;
}

template <fsize_t num_bins,
          auto    halt_pos,
          typename RegTypes,
          typename RegNames,
          typename _BP,
          template <typename, typename> typename _Vector,
          template <typename> typename Allocator>
    requires(std::is_same_v<decltype(halt_pos), vartype<_BP>>)
void
counting_sort(Memory<RegTypes, RegNames, _BP, _Vector, Allocator>& in,
              Memory<RegTypes, RegNames, _BP, _Vector, Allocator>& out) noexcept
{
    ASSERT(in.size() == out.size(), "Sizes must match");

    using index_t = vartype<_BP>;

    index_t bins[num_bins] = {};

    // histogram
    for(index_t bp: in.bp()) ++bins[fit_in_bins<halt_pos, num_bins>(bp)];

    // prefix sum
    for(index_t i = 1; i < num_bins; ++i) bins[i] += bins[i - 1];

    // fill out
    for(int i = in.size() - 1; i >= 0; --i)
        out[--bins[fit_in_bins<halt_pos, num_bins>(in.bp()[i])]] = in[i];
}

template <typename _Program, typename _Memory>
class SimpleSorter
{
    static constexpr bool singular = _Program::singular;

    _Memory m_mem;

  public:
    SimpleSorter() noexcept = default;
    explicit SimpleSorter(fsize_t n) : m_mem(n) {};

    _Memory&
    mem() noexcept
    {
        return m_mem;
    }

    const _Memory&
    mem() const noexcept
    {
        return m_mem;
    }

    void
    sort() noexcept
    {
        if constexpr(!singular) swap_only_introsort(m_mem.begin(), m_mem.end());
    }
};

template <typename _Program, typename _Memory>
class CountingSorter
{
    static constexpr auto    halt_pos = _Program::halt_pos;
    static constexpr fsize_t num_bins = _Program::num_blocks + 1;

    _Memory  m_buf0;
    _Memory  m_buf1;
    _Memory* p_main = &m_buf0;
    _Memory* p_aux  = &m_buf1;

  public:
    CountingSorter() noexcept = default;
    CountingSorter(fsize_t n) : m_buf0(n), m_buf1(n) {}

    _Memory&
    mem() noexcept
    {
        return *p_main;
    }

    const _Memory&
    mem() const noexcept
    {
        return *p_main;
    }

    void
    sort() noexcept
    {
        counting_sort<num_bins, halt_pos>(*p_main, *p_aux);
        std::swap(p_main, p_aux);
    }
};

template <typename _Program,
          template <typename, typename> typename _Vector = std::vector,
          template <typename> typename Allocator         = std::allocator,
          typename Sorter = SimpleSorter<_Program,
                                         Memory<typename _Program::reg_types,
                                                typename _Program::reg_names,
                                                typename _Program::bp,
                                                _Vector,
                                                Allocator>>>
class MemoryFacade : public Sorter
{
    using reg_names   = _Program::reg_names;
    using reg_types   = _Program::reg_types;
    using bp_var      = _Program::bp;
    using input_vars  = _Program::input_vars;
    using output_vars = _Program::output_vars;
    using index_t     = _Program::index_t;

    static constexpr index_t halt_pos = _Program::halt_pos;

    template <typename _Memory>
    struct var_setter
    {
        _Memory* mem;

        explicit var_setter(_Memory* _mem) noexcept : mem(_mem) {}

        template <typename Var, std::ranges::input_range Rng>
        std::ranges::iterator_t<Rng>
        operator()(Rng&& rng) const noexcept
        {
            decltype(auto) column = mem->template get<Var>();
            return std::ranges::copy_n(std::ranges::begin(rng),
                                       column.size(),
                                       column.begin())
                .in;
        }

        template <typename Var, typename Val>
            requires(std::is_nothrow_convertible_v<Val, vartype<Var>>)
        auto
        operator()(Val val) const noexcept
        {
            return std::ranges::fill(mem->template get<Var>(), val);
        }
    };

    template <typename _Memory>
    var_setter(_Memory*) -> var_setter<_Memory>;

  public:
    using Sorter::Sorter;

    template <typename _ProxyRow, typename... Args>
    static void
    set_row_input(_ProxyRow row, Args&&... args) noexcept
    {
        typelist::for_each_arg(
            input_vars{},
            [row]<typename Var>(auto&& arg) constexpr noexcept
            { row.template get<Var>() = arg; },
            std::forward<Args>(args)...);

        // reset
        if constexpr(!_Program::singular) row.bp() = 0;
    }

    template <typename _ProxyRow>
    static auto
    get_row_output(_ProxyRow row) noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            output_vars{},
            [row]<typename Var>() constexpr noexcept
            { return row.template get<Var>(); });
    }

    template <std::ranges::input_range Input>
        requires(
            is_tuple_v<std::ranges::range_value_t<Input>> &&
            std::is_nothrow_convertible_v<std::ranges::range_value_t<Input>,
                                          input_type<_Program>>)
    std::ranges::iterator_t<Input>
    set_inputs(Input&& input) noexcept
    {
        std::ranges::iterator_t<Input> i = std::ranges::begin(input);
        std::ranges::for_each(Sorter::mem(),
                              [&i](auto row) noexcept
                              {
                                  std::apply([row](const auto... args) noexcept
                                             { set_row_input(row, args...); },
                                             *i++);
                              });

        return i;
    }

    template <typename... Args>
    auto
    set_inputs(Args&&... args) noexcept
    {
        return typelist::maybe_collect_arg<std::tuple>(
            input_vars{},
            var_setter(&Sorter::mem()),
            std::forward<Args>(args)...);
    }

    auto
    get_outputs() const noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            output_vars{},
            [&mem = Sorter::mem()]<typename Var>() noexcept
            { return std::views::all(mem.template get<Var>()); });
    }

    auto
    get_outputs() noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            output_vars{},
            [&mem = Sorter::mem()]<typename Var>() noexcept
            { return std::views::all(mem.template get<Var>()); });
    }

    auto
    get_output_zipped() const noexcept
    {
        if constexpr(typelist::length<output_vars> > 1)
            return std::views::transform(Sorter::mem(),
                                         [](auto row) noexcept
                                         { return get_row_output(row); });
        else
            return get_outputs();
    }

    auto
    get_output_zipped() noexcept
    {
        if constexpr(typelist::length<output_vars> > 1)
            return std::views::transform(Sorter::mem(),
                                         [](auto row) noexcept
                                         { return get_row_output(row); });
        else
            return get_outputs();
    }

    template <typename F>
    auto
    visit(F&& f) noexcept
    {
        return std::invoke(std::forward<F>(f), Sorter::mem());
    }

    void
    reset() noexcept
    {
        if constexpr(!_Program::singular)
            std::ranges::fill(Sorter::mem().bp(), 0);
    }

    auto
    done() noexcept
    {
        if constexpr(!_Program::singular)
            return Sorter::mem().bp().front() == halt_pos;
    }

    fsize_t
    size() const noexcept
    {
        return Sorter::mem().size();
    }

    void
    resize(fsize_t n) noexcept
    {
        Sorter::mem().resize(n);
    }
};

template <typename _MemoryFacade, typename Input, typename Iter>
auto
exchange_from(_MemoryFacade& facade, Input&& input, Iter bgn) noexcept
{
    return ranges::views::zip_with(
        [](auto row, auto inputs)
        {
            auto val = _MemoryFacade::get_row_output(row);

            invopply([row](auto... args) constexpr noexcept
                        { _MemoryFacade::set_row_input(row, args...); },
                        inputs);

            return val;
        },
        ranges::subrange(bgn, std::ranges::end(facade.mem())),
        std::forward<Input>(input));
}

template <typename _Program,
          template <typename, typename> typename _Vector = std::vector,
          template <typename> typename Allocator         = std::allocator,
          typename Sorter = SimpleSorter<_Program,
                                         Memory<typename _Program::reg_types,
                                                typename _Program::reg_names,
                                                typename _Program::bp,
                                                _Vector,
                                                Allocator>>>
class StreamFacade : public MemoryFacade<_Program, _Vector, Allocator, Sorter>
{
    using reg_names   = _Program::reg_names;
    using reg_types   = _Program::reg_types;
    using bp_var      = _Program::bp;
    using input_vars  = _Program::input_vars;
    using output_vars = _Program::output_vars;
    using index_t     = _Program::index_t;

    static constexpr index_t halt_pos = _Program::halt_pos;

    using facade = MemoryFacade<_Program, _Vector, Allocator, Sorter>;

  public:
    using facade::facade;

    template <std::ranges::input_range Input>
        requires(
            std::is_nothrow_convertible_v<std::ranges::range_value_t<Input>,
                                          input_type<_Program>>)
    auto
    exchange(Input&& input) noexcept
    {
        return exchange_from(
            static_cast<facade&>(*this),
            std::forward<Input>(input),
            std::ranges::find_if(facade::mem(),
                                 [](auto row) constexpr noexcept
                                 { return row.bp() == halt_pos; }));
    }
};

template <typename Command, typename LabelMap>
struct vector_eval;

template <typename Op, typename Dst, typename... Args, typename LabelMap>
struct vector_eval<Compute<Op, Dst, Args...>, LabelMap>
{
    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::ranges::for_each(std::forward<_Memory>(mem),
                              [](auto row) constexpr noexcept
                              {
                                  row.template get<Dst>() =
                                      std::invoke(Op{},
                                                  get_value<Args>(row)...);
                              });
    }
};

template <typename Target, typename LabelMap>
struct vector_eval<Jump<Target>, LabelMap>
{
    using constant = typelist::lookup<Target, LabelMap>;
    static constexpr constant f{};

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::ranges::for_each(std::forward<_Memory>(mem),
                              [](auto row) constexpr noexcept
                              { row.bp() = std::invoke(f); });
    }
};

template <typename Var, typename Labels, typename LabelMap>
struct vector_eval<ParmJump<Var, Labels>, LabelMap>
{
    static constexpr auto table = pjmp_arr<ParmJump<Var, Labels>, LabelMap>;

    template <typename _Memory>
    constexpr void
    operator()(_Memory&& mem) const noexcept
    {
        std::ranges::for_each(std::forward<_Memory>(mem),
                              [](auto row) noexcept
                              { row.bp() = table[row.template get<Var>()]; });
    }
};

enum SORT
{
    INTROSORT,
    COUNTING_SORT
};

template <SORT sort_method>
struct select_sorter;

template <>
struct select_sorter<INTROSORT>
{
    template <typename _Program, typename _Memory>
    using type = SimpleSorter<_Program, _Memory>;
};

template <>
struct select_sorter<COUNTING_SORT>
{
    template <typename _Program, typename _Memory>
    using type = CountingSorter<_Program, _Memory>;
};

template <typename _Program, SORT sort_method>
class basic_vector_compiler
{
    using labelmap  = _Program::labelmap;
    using blocks    = _Program::blocks;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using bp        = _Program::bp;
    using vars      = _Program::variables;
    using index_t   = _Program::index_t;

    static constexpr bool    singular = _Program::singular;
    static constexpr index_t halt_pos = _Program::halt_pos;

    template <typename C>
    using compile_block = standard_compiler<C, labelmap, vector_eval>;

    using compiled_blocks = typelist::map<compile_block, blocks>;

    using memory = Memory<reg_types, reg_names, bp>;

    static constexpr SORT final_sort_method =
        singular ? INTROSORT : sort_method;
    using sorter =
        select_sorter<final_sort_method>::template type<_Program, memory>;

    using memory_facade =
        MemoryFacade<_Program, std::vector, std::allocator, sorter>;
    using stream_facade =
        StreamFacade<_Program, std::vector, std::allocator, sorter>;

    class single_block_evaluator : public memory_facade
    {
        static constexpr typelist::head<compiled_blocks> block{};

      public:
        using program = _Program;
        using memory_facade::memory_facade;

        template <bool zip_outputs = false, typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            memory_facade::set_inputs(std::forward<Args>(args)...);
            memory_facade::visit(block);
            if constexpr(zip_outputs)
                return memory_facade::get_output_zipped();
            else
                return memory_facade::get_outputs();
        }
    };

    struct visitor
    {
        static constexpr typename typelist::to<compiled_blocks, std::tuple>
                              blocks{};
        static constexpr auto halt_pos = _Program::halt_pos;

        void
        operator()(memory& mem) const noexcept
        {
            std::ranges::for_each(
                ranges::views::filter(mem,
                                      [](auto row) constexpr noexcept
                                      { return row.bp() != halt_pos; }) |
                    ranges::views::chunk_by(
                        [](auto a, auto b) constexpr noexcept
                        { return a.bp() == b.bp(); }),
                [](auto grp) constexpr noexcept
                {
                    visit_at(
                        ranges::front(grp).bp(),
                        [grp](auto block) constexpr noexcept { block(grp); },
                        blocks,
                        sink);
                });
        }
    };

    class evaluator : public memory_facade
    {
#if 0
        void
        print() const noexcept
        {
            auto columns = typelist::maybe_collect<std::tuple>(
                vars{},
                [&]<typename Var>() constexpr noexcept
                {
                    return memory_facade::visit(
                        [](memory& mem) noexcept
                        { return std::views::all(mem.template get<Var>()); });
                });

            typelist::for_each(vars{},
                               []<typename Var>()
                               {
                                   using name       = varname<Var>;
                                   constexpr auto l = intlist::length<name>;
                                   constexpr auto a = intlist::array<name>;
                                   constexpr std::string_view s(a, l);
                                   fmt::print("{:>10}", s);
                               });
            putchar('\n');
            for(fsize_t i = 0; i < memory_facade::size(); ++i)
            {
                lucid::for_each(columns,
                                [i](const auto& c)
                                { fmt::print("{:>10}", c[i]); });
                putchar('\n');
            }
        }
#endif

      public:
        using program = _Program;
        using memory_facade::memory_facade;

        template <bool zip_outputs = false, typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            memory_facade::set_inputs(std::forward<Args>(args)...);

            while(!memory_facade::done())
            {
                memory_facade::visit(visitor{});
                memory_facade::sort();
            }

            if constexpr(zip_outputs)
                return memory_facade::get_output_zipped();
            else
                return memory_facade::get_outputs();
        }
    };

  public:
    using function = std::
        conditional_t<_Program::singular, single_block_evaluator, evaluator>;

    class stream : public stream_facade
    {
        static_assert(!_Program::singular);

      public:
        using program = _Program;
        using stream_facade::stream_facade;

        void
        operator()() noexcept
        {
            stream_facade::visit(visitor{});
        }
    };
};

template <typename _Program, SORT sort_method>
using function = basic_vector_compiler<_Program, sort_method>::function;

template <typename _Program, SORT sort_method>
    requires(!_Program::singular)
using stream = basic_vector_compiler<_Program, sort_method>::stream;
} // namespace lucid::bve
