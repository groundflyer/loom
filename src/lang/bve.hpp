// -*- C++ -*-
/// @file bve.hpp
/// @brief Basic vector evaluator

#pragma once

#include "lang.hpp"

#include <utils/debug.hpp>
#include <utils/functional.hpp>
#include <utils/tuple.hpp>

#include <range/v3/range/operations.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/filter.hpp>

#include <algorithm>
#include <compare>
#include <ranges>
#include <span>
#include <vector>

namespace lucid::bve
{
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
    copy_values(ProxyRow rhs) noexcept
    {
        lucid::for_each([&](auto& c) noexcept { c[m_pos] = c[rhs.m_pos]; },
                        m_data);
    }

  public:
    ProxyRow() = delete;

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
        m_pos = rhs.m_pos;

        return *this;
    }

    ProxyRow&
    operator=(ProxyRow&& rhs) noexcept
    {
        copy_values(rhs);
        m_pos = rhs.m_pos;

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
    std::add_lvalue_reference_t<vartype<Var>>
    get() const noexcept
    {
        return Helper::template get<Var>(m_data)[m_pos];
    }

    std::add_lvalue_reference_t<vartype<_CP>>
    cp() const noexcept
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
    ProxyView() = delete;
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
    ProxyView() = delete;
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

    Memory(const Memory&) = delete;
    Memory(Memory&&)      = delete;

    Memory&
    operator=(const Memory&) = delete;
    Memory&
    operator=(Memory&&) = delete;

    value_type
    operator[](const fsize_t n) noexcept
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

    proxy_view<dynamic_extent>
    view(const fsize_t pos, const fsize_t ext) noexcept
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

    std::span<vartype<_CP>>
    cp() noexcept
    {
        return get<_CP>();
    }

    void
    resize(fsize_t new_size)
    {
        lucid::for_each([&](auto& c) noexcept { c.resize(new_size); }, m_data);
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

template <typename InputVars>
struct input_setter
{
    template <typename _Memory, typename... Args>
    constexpr void
    operator()(_Memory& mem, Args&&... args) const noexcept
    {
        typelist::for_each_arg(
            [&mem]<typename Var, typename T>(const T& value) constexpr noexcept
            {
                std::ranges::range decltype(auto) column =
                    mem.template get<Var>();
                if constexpr(std::ranges::input_range<T>)
                    std::ranges::copy(value, column.begin());
                else
                    std::ranges::fill(column, value);
            },
            InputVars{},
            std::forward<Args>(args)...);
    }
};

template <typename OutputVars>
struct output_getter
{
    template <typename _Memory>
    constexpr decltype(auto)
    operator()(_Memory& mem) const noexcept
    {
        return typelist::maybe_collect<std::tuple>(
            [&mem]<typename Var>() constexpr noexcept
            { return std::views::all(mem.template get<Var>()); },
            OutputVars{});
    }
};

template <typename _Program>
struct basic_vector_evaluator
{
    using labelmap  = _Program::labelmap;
    using cells     = _Program::cells;
    using vars      = _Program::variables;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using cp        = _Program::cp;

    static constexpr auto halt_pos = _Program::halt_pos;

    template <typename C>
    using compile_cell = standard_compiler<C, labelmap, vector_eval>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    static constexpr input_setter<typename _Program::input_vars>   setter{};
    static constexpr output_getter<typename _Program::output_vars> getter{};

    using memory = Memory<reg_types, reg_names, cp>;

    struct single_cell_evaluator
    {
        static constexpr typelist::head<compiled_cells> cell{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const fsize_t n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);
            cell(mem);
            return getter(mem);
        }

        void
        reset() noexcept
        {
        }
    };

    struct evaluator
    {
        static constexpr typename typelist::to<compiled_cells, std::tuple>
            cells{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const fsize_t n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);

            while(mem.cp()[0] != halt_pos)
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
                            [&](auto& cell) constexpr noexcept { cell(grp); },
                            cells,
                            sink);
                    });

#ifdef _LIBCPP_VERSION
                std::sort(mem.begin(), mem.end());
#else
                // libstdc++ std::sort doesn't use swap internally
                // so we sort it manually
                for(fsize_t i = 1; i < mem.size(); ++i)
                    for(fsize_t j = i; j > 0 && mem[j - 1] > mem[j]; --j)
                        swap(mem[j], mem[j - 1]);
#endif
            }

            return getter(mem);
        }

        void
        reset() noexcept
        {
            std::ranges::fill(mem.cp(), 0);
        }
    };

  public:
    using type = std::
        conditional_t<_Program::singular, single_cell_evaluator, evaluator>;
};

template <typename _Program>
using evaluator = basic_vector_evaluator<_Program>::type;
} // namespace lucid::bve
