// -*- C++ -*-
/// @file bve.hpp
/// @brief Basic vector evaluator

#pragma once

#include "lang.hpp"

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
          template <typename, typename>
          typename Vector,
          template <typename>
          typename Allocator>
struct vector_union
{
    using type = Vector<T, Allocator<T>>;
};

template <typename... Ts,
          template <typename, typename>
          typename Vector,
          template <typename>
          typename Allocator>
struct vector_union<Union<Ts...>, Vector, Allocator>
{
    using type = UnionVector<type_sequence<Ts...>, Allocator<std::byte>>;
};

template <typename T,
          template <typename, typename>
          typename Vector,
          template <typename>
          typename Allocator>
using vector_union_t = vector_union<T, Vector, Allocator>::type;

template <typename Data, typename Helper, typename _CP>
class ProxyRow
{
    Data&   m_data;
    fsize_t m_pos;

    void
    copy_values(const ProxyRow& rhs) noexcept
    {
        intlist::for_each(
            []<std::size_t I>(ProxyRow& lhs, const ProxyRow& rhs) noexcept {
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
        return cp() == rhs.cp();
    }

    std::strong_ordering
    operator<=>(const ProxyRow rhs) const noexcept
    {
        return cp() <=> rhs.cp();
    }

    template <typename Var>
    std::add_const_t<std::add_lvalue_reference_t<vartype<Var>>>
    get() const noexcept
    {
        return Helper::template get<Var>(m_data)[m_pos];
    }

    std::add_const_t<std::add_lvalue_reference_t<vartype<_CP>>>
    cp() const noexcept
    {
        return get<_CP>();
    }

    template <typename Var>
    std::add_lvalue_reference_t<vartype<Var>>
    get() noexcept
    {
        return Helper::template get<Var>(m_data)[m_pos];
    }

    std::add_lvalue_reference_t<vartype<_CP>>
    cp() noexcept
    {
        return get<_CP>();
    }

    friend void
    swap(ProxyRow a, ProxyRow b) noexcept
    {
        lucid::for_each([&](auto& c) noexcept
                        { std::swap(c[a.m_pos], c[b.m_pos]); },
                        a.m_data);
    }
};

template <typename Data, typename Helper, typename _CP, fsize_t N>
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
        m_data(other.m_data), m_pos(other.m_pos){};

    ProxyView(Data& data, fsize_t pos) noexcept : m_data(data), m_pos(pos) {}

    ProxyRow<Data, Helper, _CP>
    operator[](const fsize_t n) const noexcept
    {
        return {m_data, shift(n)};
    }

    template <fsize_t N2>
        requires(N2 <= N)
    ProxyView<Data, Helper, _CP, N2>
    view(const fsize_t pos = 0) const noexcept
    {
        ASSERT(N2 <= N - pos, "Extent too big");
        return {m_data, shift(pos)};
    }

    ProxyView<Data, Helper, _CP, dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        ASSERT(ext <= N - pos, "Extent too big");
        return {m_data, shift(pos), ext};
    }

    ProxyView<Data, Helper, _CP, dynamic_extent>
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

    std::span<vartype<_CP>, N>
    cp() const noexcept
    {
        return get<_CP>();
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

template <typename Data, typename Helper, typename _CP>
class ProxyView<Data, Helper, _CP, dynamic_extent>
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
        m_data(other.m_data), m_pos(other.m_pos), m_ext(other.m_ext){};

    ProxyView(Data& data, fsize_t pos, fsize_t ext) noexcept :
        m_data(data), m_pos(pos), m_ext(ext)
    {
    }

    ProxyRow<Data, Helper, _CP>
    operator[](const fsize_t n) const noexcept
    {
        return {m_data, shift(n)};
    }

    template <fsize_t N>
    ProxyView<Data, Helper, _CP, N>
    view(const fsize_t pos) const noexcept
    {
        ASSERT(N <= size() - pos, "Extent too big");
        return {m_data, shift(pos)};
    }

    ProxyView<Data, Helper, _CP, dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) const noexcept
    {
        ASSERT(ext <= size() - pos, "Extent too big");
        return {m_data, shift(pos), ext};
    }

    ProxyView<Data, Helper, _CP, dynamic_extent>
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

    std::span<vartype<_CP>, std::dynamic_extent>
    cp() const noexcept
    {
        return get<_CP>();
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

template <typename Data, typename Helper, typename _CP>
class MemoryIterator : public std::contiguous_iterator_tag
{
    Data*   p_data = nullptr;
    fsize_t m_pos{};
    fsize_t m_size{};

  public:
    using difference_type = std::ptrdiff_t;
    using value_type      = ProxyRow<Data, Helper, _CP>;

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
        return {p_data, m_pos - rhs, m_size};
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
        return {*p_data, m_pos + n};
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
          typename _CP,
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
    using proxy_view = ProxyView<Data, helper, _CP, N>;

  public:
    using value_type = bve::ProxyRow<Data, helper, _CP>;
    using view_type  = proxy_view<dynamic_extent>;
    using iterator   = bve::MemoryIterator<Data, helper, _CP>;

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

    std::span<vartype<_CP>>
    cp() noexcept
    {
        return get<_CP>();
    }

    std::span<vartype<_CP>>
    cp() const noexcept
    {
        return get<_CP>();
    }

    void
    resize(fsize_t new_size)
    {
        lucid::for_each([new_size](auto& c) noexcept { c.resize(new_size); },
                        m_data);
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

template <fsize_t num_bins,
          auto    halt_pos,
          typename RegTypes,
          typename RegNames,
          typename _CP,
          template <typename, typename>
          typename _Vector,
          template <typename>
          typename Allocator>
    requires(std::is_same_v<decltype(halt_pos), vartype<_CP>>)
void
counting_sort(Memory<RegTypes, RegNames, _CP, _Vector, Allocator>& in,
              Memory<RegTypes, RegNames, _CP, _Vector, Allocator>& out) noexcept
{
    ASSERT(in.size() == out.size(), "Sizes must match");

    using cp_t = vartype<_CP>;

    cp_t bins[num_bins] = {};

    auto fit = [](cp_t cp) noexcept
    { return cp != halt_pos ? cp : num_bins - 1; };

    // histogram
    for(cp_t cp: in.cp()) ++bins[fit(cp)];

    // prefix sum
    for(cp_t i = 1; i < num_bins; ++i) bins[i] += bins[i - 1];

    // fill out
    for(int i = in.size() - 1; i >= 0; --i)
        out[--bins[fit(in.cp()[i])]] = in[i];
}

template <typename _Memory, bool singular>
class SimpleSorter
{
    _Memory m_mem;

  protected:
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

  public:
    SimpleSorter() noexcept = default;
    explicit SimpleSorter(fsize_t n) : m_mem(n){};

    void
    sort() noexcept
    {
        if constexpr(!singular) std::sort(m_mem.begin(), m_mem.end());
    }
};

template <typename _Memory, fsize_t num_bins, auto halt_pos>
class CountingSorter
{
    _Memory  m_buf0;
    _Memory  m_buf1;
    _Memory* p_main = &m_buf0;
    _Memory* p_aux  = &m_buf1;

  protected:
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

  public:
    CountingSorter() noexcept = default;
    CountingSorter(fsize_t n) : m_buf0(n), m_buf1(n) {}

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
          typename Sorter = SimpleSorter<Memory<typename _Program::reg_types,
                                                typename _Program::reg_names,
                                                typename _Program::cp,
                                                _Vector,
                                                Allocator>,
                                         _Program::singular>>
class MemoryFacade : public Sorter
{
    using reg_names   = _Program::reg_names;
    using reg_types   = _Program::reg_types;
    using cp_var      = _Program::cp;
    using input_vars  = _Program::input_vars;
    using output_vars = _Program::output_vars;

    static constexpr auto halt_pos = _Program::halt_pos;

  public:
    using Sorter::Sorter;

    template <typename... Args>
    void
    set_input(Args&&... args) noexcept
    {
        typelist::for_each_arg(
            [&mem = Sorter::mem()]<typename Var, typename T>(
                const T&
                    value) constexpr noexcept requires(std::ranges::
                                                           sized_range<T> ||
                                                       std::
                                                           is_nothrow_convertible_v<
                                                               T,
                                                               vartype<Var>>) {
                std::ranges::range decltype(auto) column =
                    mem.template get<Var>();
                if constexpr(std::ranges::sized_range<T>)
                {
                    ASSERT(
                        std::ranges::size(value) == mem.size(),
                        "Input range size doesn't match memory buffer size.");
                    std::ranges::copy(value, column.begin());
                }
                else
                    std::ranges::fill(column, value);
            },
            input_vars{},
            std::forward<Args>(args)...);
    }

    decltype(auto)
    get_output() const noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            [&mem = Sorter::mem()]<typename Var>() constexpr noexcept
            { return std::views::all(mem.template get<Var>()); },
            output_vars{});
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
            std::ranges::fill(Sorter::mem().cp(), 0);
    }

    auto
    done() noexcept
    {
        if constexpr(!_Program::singular)
            return Sorter::mem().cp().front() == halt_pos;
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

template <typename _Program,
          template <typename, typename> typename _Vector = std::vector,
          template <typename> typename Allocator         = std::allocator,
          typename Sorter = SimpleSorter<Memory<typename _Program::reg_types,
                                                typename _Program::reg_names,
                                                typename _Program::cp,
                                                _Vector,
                                                Allocator>,
                                         _Program::singular>>
class StreamFacade : public MemoryFacade<_Program, _Vector, Allocator, Sorter>
{
    using reg_names   = _Program::reg_names;
    using reg_types   = _Program::reg_types;
    using cp_var      = _Program::cp;
    using input_vars  = _Program::input_vars;
    using output_vars = _Program::output_vars;

    static constexpr auto halt_pos = _Program::halt_pos;

    using facade = MemoryFacade<_Program, _Vector, Allocator, Sorter>;

    template <typename _ProxyRow, typename... Args>
    static void
    set_row_input(_ProxyRow row, Args&&... args) noexcept
    {
        typelist::for_each_arg(
            [row]<typename Var>(auto&& arg) constexpr noexcept
            { row.template get<Var>() = arg; },
            input_vars{},
            std::forward<Args>(args)...);

        // reset
        row.cp() = 0;
    }

    template <typename _ProxyRow>
    static auto
    get_row_output(_ProxyRow row) noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            [row]<typename Var>() constexpr noexcept
            { return row.template get<Var>(); },
            output_vars{});
    }

  public:
    using facade::facade;

    template <std::ranges::input_range Input>
        requires(
            std::is_nothrow_convertible_v<std::ranges::range_value_t<Input>,
                                          input_type<_Program>>)
    constexpr auto
    exchange(Input&& input) noexcept
    {
        return ranges::views::zip_with(
            [](auto row, auto inputs)
            {
                auto val = get_row_output(row);

                maybe_apply([row](auto... args) constexpr noexcept
                            { set_row_input(row, args...); },
                            inputs);

                return val;
            },
            ranges::subrange(
                std::ranges::find_if(facade::mem(),
                                     [](auto row) constexpr noexcept
                                     { return row.cp() == halt_pos; }),
                std::ranges::end(facade::mem())),
            std::forward<Input>(input));
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
                              [](auto row) constexpr noexcept {
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
                              { row.cp() = std::invoke(f); });
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
                              { row.cp() = table[row.template get<Var>()]; });
    }
};

template <typename _Program, bool use_counting_sort>
class basic_vector_compiler
{
    using labelmap  = _Program::labelmap;
    using cells     = _Program::cells;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using cp        = _Program::cp;

    static constexpr bool        singular = _Program::singular;
    static constexpr vartype<cp> halt_pos = _Program::halt_pos;

    template <typename C>
    using compile_cell = standard_compiler<C, labelmap, vector_eval>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    using memory        = Memory<reg_types, reg_names, cp>;
    using simple_sorter = SimpleSorter<memory, singular>;
    using sorter        = std::conditional_t<
        use_counting_sort,
        std::conditional_t<
            singular,
            simple_sorter,
            CountingSorter<memory, _Program::num_cells + 1, halt_pos>>,
        simple_sorter>;

    using memory_facade =
        MemoryFacade<_Program, std::vector, std::allocator, sorter>;
    using stream_facade =
        StreamFacade<_Program, std::vector, std::allocator, sorter>;

    class single_cell_evaluator : public memory_facade
    {
        static constexpr typelist::head<compiled_cells> cell{};

      public:
        using memory_facade::memory_facade;

        template <typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            memory_facade::set_input(std::forward<Args>(args)...);
            memory_facade::visit(cell);
            return memory_facade::get_output();
        }
    };

    struct visitor
    {
        static constexpr typename typelist::to<compiled_cells, std::tuple>
                              cells{};
        static constexpr auto halt_pos = _Program::halt_pos;

        void
        operator()(memory& mem) const noexcept
        {
            std::ranges::for_each(
                ranges::views::filter(mem,
                                      [](auto row) constexpr noexcept
                                      { return row.cp() != halt_pos; }) |
                    ranges::views::chunk_by(
                        [](auto a, auto b) constexpr noexcept
                        { return a.cp() == b.cp(); }),
                [](auto grp) constexpr noexcept
                {
                    visit_at(
                        ranges::front(grp).cp(),
                        [grp](auto cell) constexpr noexcept { cell(grp); },
                        cells,
                        sink);
                });
        }
    };

    class evaluator : public memory_facade
    {
      public:
        using memory_facade::memory_facade;

        template <typename... Args>
        auto
        operator()(Args&&... args) noexcept
        {
            memory_facade::set_input(std::forward<Args>(args)...);

            while(!memory_facade::done())
            {
                memory_facade::visit(visitor{});
                memory_facade::sort();
            }

            return memory_facade::get_output();
        }
    };

  public:
    using function = std::
        conditional_t<_Program::singular, single_cell_evaluator, evaluator>;

    class streamer : public stream_facade
    {
        static_assert(!_Program::singular);

      public:
        using stream_facade::stream_facade;

        void
        operator()() noexcept
        {
            stream_facade::visit(visitor{});
        }
    };
};

template <typename _Program, bool use_counting_sort = false>
using function = basic_vector_compiler<_Program, use_counting_sort>::function;

template <typename _Program, bool use_counting_sort = false>
    requires(!_Program::singular)
using streamer = basic_vector_compiler<_Program, use_counting_sort>::streamer;
} // namespace lucid::bve
