// -*- C++ -*-
// cuda.hpp
//

#ifndef __CUDA__
#error "This header requires CUDA"
#endif

#pragma once

#include "bve.hpp"

#include <bit>

// stupid cmake adds -NDEBUG to RelWithDebInfo for CUDA
// thus we use #ifdef DEBUG instead ifndef NDEBUG
#ifdef DEBUG
#include <cassert>
#include <cstdio>
void
cuda_check(cudaError_t r, const char* func, int line) noexcept
{
    if(r != cudaSuccess)
        std::fprintf(stderr,
                     "CUDA Error in %s: %d: %s\n",
                     func,
                     line,
                     cudaGetErrorString(r));
    assert(r == cudaSuccess);
}

#define CUDA_CHECK(...) cuda_check(__VA_ARGS__, __PRETTY_FUNCTION__, __LINE__)

#define LAUNCH_KERNEL(KERNEL, NUM_THREADS, ...)                    \
    {                                                              \
        cudaGetLastError();                                        \
        KERNEL<<<ceil_div(NUM_THREADS, BLOCK_SIZE), BLOCK_SIZE>>>( \
            __VA_ARGS__);                                          \
        cuda_check(cudaPeekAtLastError(), #KERNEL, __LINE__ - 1);  \
    }

#define ASSERT_POW2(x) ASSERT(std::has_single_bit(x), "Must be power of 2\n")
#else
void
cuda_check(cudaError_t) noexcept
{
}

#define CUDA_CHECK(...) cuda_check(__VA_ARGS__)

#define LAUNCH_KERNEL(KERNEL, NUM_THREADS, ...) \
    KERNEL<<<ceil_div(NUM_THREADS, BLOCK_SIZE), BLOCK_SIZE>>>(__VA_ARGS__)

#define ASSERT_POW2(x)
#endif

#define HOST_TO_DEVICE(DST, SRC, N)                                         \
    CUDA_CHECK(cudaMemcpy(DST,                                              \
                          SRC,                                              \
                          N * sizeof(std::remove_pointer_t<decltype(DST)>), \
                          cudaMemcpyHostToDevice))

#define DEVICE_TO_HOST(DST, SRC, N)                                         \
    CUDA_CHECK(cudaMemcpy(DST,                                              \
                          SRC,                                              \
                          N * sizeof(std::remove_pointer_t<decltype(SRC)>), \
                          cudaMemcpyDeviceToHost))

#define DEF_CUDA_ALLOC(NAME, MALLOC, FREE)           \
    template <typename T>                            \
    struct NAME                                      \
    {                                                \
        using value_type = T;                        \
        using pointer    = T*;                       \
        NAME() noexcept  = default;                  \
        pointer                                      \
        allocate(gsize_t n)                          \
        {                                            \
            pointer ret;                             \
            CUDA_CHECK(MALLOC(&ret, n * sizeof(T))); \
            return ret;                              \
        }                                            \
        void                                         \
        deallocate(pointer p, gsize_t)               \
        {                                            \
            CUDA_CHECK(FREE(p));                     \
        }                                            \
    };

namespace lucid::cuda
{
/// @brief Index type that goes to GPU.
using gsize_t = std::uint32_t;

/// @brief Indicates invalid position;
constexpr inline gsize_t n_pos = static_cast<gsize_t>(-1);

DEF_CUDA_ALLOC(DeviceAllocator, cudaMalloc, cudaFree)
DEF_CUDA_ALLOC(HostAllocator, cudaMallocHost, cudaFreeHost)

#undef DEF_CUDA_ALLOC

#ifdef CUDA_BLOCK_SIZE
static constexpr unsigned BLOCK_SIZE = CUDA_BLOCK_SIZE;
static_assert(BLOCK_SIZE % 32 == 0 && BLOCK_SIZE >= 32);
#else
static constexpr unsigned BLOCK_SIZE = 256;
#endif

// std::vector doesn't work with CUDA. We replace it with this.
template <typename T, typename A>
class SimpleVector
{
    T*       p_data;
    unsigned m_n = 0;
    A        m_alloc;

    void
    alloc() noexcept
    {
        p_data = std::allocator_traits<A>::allocate(m_alloc, size());
    }

    void
    dealloc() noexcept
    {
        if(size())
            std::allocator_traits<A>::deallocate(m_alloc, p_data, size());
    }

  public:
    using value_type = T;
    using iterator   = T*;

    SimpleVector(const SimpleVector&) = delete;
    SimpleVector(SimpleVector&&)      = delete;
    SimpleVector&
    operator=(const SimpleVector&) = delete;
    SimpleVector&
    operator=(SimpleVector&&) = delete;

    SimpleVector() = default;
    explicit SimpleVector(const A& a) noexcept : m_alloc(a) {}
    SimpleVector(unsigned n, const A& a = A()) : m_n(n), m_alloc(a) { alloc(); }

    ~SimpleVector() { dealloc(); }

    void
    resize(unsigned new_size)
    {
        if(new_size != m_n)
        {
            dealloc();
            m_n = new_size;
            alloc();
        }
    }

    __host__ __device__ unsigned
    size() const noexcept
    {
        return m_n;
    }

    __host__ __device__ T*
    data() noexcept
    {
        return p_data;
    }

    __host__ __device__ iterator
    begin() noexcept
    {
        return p_data;
    }

    __host__ __device__ iterator
    end() noexcept
    {
        return p_data + size();
    }

    __host__ __device__ T&
    operator[](const unsigned i) noexcept
    {
        CHECK_INDEX(i, size());
        return p_data[i];
    }

    __host__ __device__ friend iterator
    begin(SimpleVector& v) noexcept
    {
        return v.begin();
    }

    __host__ __device__ friend iterator
    end(SimpleVector& v) noexcept
    {
        return v.end();
    }

    __host__ __device__ friend unsigned
    size(SimpleVector& v) noexcept
    {
        return v.size();
    }
};

/// @brief Invoke every functional object in the typelist with @p args.
template <typename... Fs, typename... Args>
__device__ void
unroll(type_sequence<Fs...> /*list*/, Args&... args)
{
    (..., std::invoke(Fs{}, args...));
}

template <typename Op>
struct remap_comp;

template <>
struct remap_comp<std::identity>
{
    struct type
    {
        template <typename T>
        __device__ T
        operator()(T v) const
        {
            return v;
        }
    };
};

template <>
struct remap_comp<choice_op>
{
    struct type
    {
        template <typename C, typename T>
        __device__ T
        operator()(const C c, const T a, const T b) const
        {
            return c ? a : b;
        }
    };
};

#define DEF_REMAP_BIN(func, op)                    \
    template <typename T>                          \
    struct remap_comp<func<T>>                     \
    {                                              \
        struct type                                \
        {                                          \
            __device__ T                           \
            operator()(const T a, const T b) const \
            {                                      \
                return a op b;                     \
            }                                      \
        };                                         \
    };

#define DEF_REMAP_UN(func, op)          \
    template <typename T>               \
    struct remap_comp<func<T>>          \
    {                                   \
        struct type                     \
        {                               \
            __device__ T                \
            operator()(const T a) const \
            {                           \
                return op a;            \
            }                           \
        };                              \
    };

DEF_REMAP_BIN(std::plus, +)
DEF_REMAP_BIN(std::minus, -)
DEF_REMAP_BIN(std::multiplies, *)
DEF_REMAP_BIN(std::divides, /)
DEF_REMAP_BIN(std::modulus, %)
DEF_REMAP_UN(std::negate, -)

DEF_REMAP_BIN(std::equal_to, ==)
DEF_REMAP_BIN(std::not_equal_to, !=)
DEF_REMAP_BIN(std::greater, >)
DEF_REMAP_BIN(std::less, <)
DEF_REMAP_BIN(std::greater_equal, >=)
DEF_REMAP_BIN(std::less_equal, <=)

DEF_REMAP_BIN(std::logical_and, &&)
DEF_REMAP_BIN(std::logical_or, ||)
DEF_REMAP_UN(std::logical_not, !)

DEF_REMAP_BIN(lshift_op, <<)
DEF_REMAP_BIN(rshift_op, >>)

#undef DEF_REMAP_UN
#undef DEF_REMAP_BIN

template <typename Op>
using remap_comp_t = remap_comp<Op>::type;

/// @brief Initializes ranges with @p n_pos.
/// @param bgns contains ranges' starts, the index of the first range element in
/// @p cps.
/// @param ends contains ranges' ends, the index after the last range element in
/// @p cps.
/// @tparam N size of arrays @p bgns and @p ends.
template <gsize_t N>
void
pre_counter(gsize_t* bgns, gsize_t* ends) noexcept
{
    for(gsize_t i = 0; i < N; ++i) bgns[i] = ends[i] = n_pos;
}

/// @brief Ensures that the ranges set correctly.
/// @copydetails cuda::pre_counter
/// @param cps_size maximum possible value for a range.
/// @param dcp default cell pointer.
template <gsize_t N>
void
post_counter(gsize_t*      bgns,
             gsize_t*      ends,
             const gsize_t cps_size,
             const gsize_t dcp) noexcept
{
    // none of ranges are set
    bool none = true;
    for(gsize_t i = 0; i < N; ++i)
    {
        gsize_t& b = bgns[i];
        gsize_t& e = ends[i];

        none = none && b == n_pos && e == n_pos;

        // beginning is not set, but end is set
        if(b == n_pos && e != n_pos) b = 0;
        // beginning is set, but end is not set
        else if(b != n_pos && e == n_pos)
            e = cps_size;

#ifdef DEBUG
        // both points must be either n_pos or of a valid range
        assert(b == n_pos || b < cps_size);
        assert(e == n_pos || e < cps_size);
        assert(b <= e);
#endif
    }

    if(none)
    {
        bgns[dcp] = 0;
        ends[dcp] = cps_size;
    }
}

/// @brief Computes ranges of chunks containing equal values saving them to @p
/// bgns and @p ends.
/// @param bgns contains ranges' starts, the index of the first range element in
/// @p cps.
/// @param ends contains ranges' ends, the index after the last range element in
/// @p cps.
/// @param N size of @p bgns and @p ends.
/// @param cps cell pointers.
/// @param cps_size size of @p cps.
/// @tparam cp_t cell pointer type.
///
/// Basically parallel version of @p groupby / @p chunkby(==) with known number
/// of chunks which is @f$N = C + 1@f$ where @p C is number of cells. The last
/// range is for terminated cells.
///
/// The beginning of the first and end of the last ranges are no set here.
template <std::unsigned_integral cp_t>
__global__ void
chunk_counter(gsize_t* bgns,
              gsize_t* ends,
              gsize_t  N,
              cp_t*    cps,
              gsize_t  cps_size,
              cp_t     halt_pos)
{
    const gsize_t idxl = blockIdx.x * blockDim.x + threadIdx.x;

    const cp_t cpl = cps[idxl];

    auto get_pos = [&](cp_t p) -> gsize_t
    { return p == halt_pos ? (N - 1) : p; };

    if(idxl < cps_size - 1)
    {
        const gsize_t idxr = idxl + 1;
        const cp_t    cpr  = cps[idxr];
        if(cpl != cpr) ends[get_pos(cpl)] = bgns[get_pos(cpr)] = idxr;
    }
}

template <std::size_t I, typename Head, typename... Args>
__device__ decltype(auto)
select_arg(Head& head, Args&... args)
{
    static_assert(I < sizeof...(args) + 1, "Index out of range");

    if constexpr(I == 0)
        return (head);
    else if constexpr(sizeof...(Args))
        return select_arg<I - 1>(args...);
}

template <typename F, typename... Args, std::size_t... Is>
__device__ auto
call_multiselect(F f, std::index_sequence<Is...>, Args&... args)
{
    return f(select_arg<Is>(args...)...);
}

/// @brief Device swap.
template <typename T>
__device__ void
swap(T& a, T& b)
{
    T tmp = a;
    a     = b;
    b     = tmp;
}

/// @brief Compare and swap rows in bitonic merge.
template <std::size_t... Idxs, typename... Ts>
    requires(sizeof...(Idxs) == sizeof...(Ts) / 2)
__device__ void
cmp_swap_rows(std::index_sequence<Idxs...>, const bool ascending, Ts&... rows)
{
    constexpr std::size_t offset = sizeof...(Idxs);
    /// cell pointer should be at 0
    const std::unsigned_integral auto cpl = select_arg<0>(rows...);
    const std::unsigned_integral auto cpr = select_arg<offset>(rows...);
    if(ascending == (cpl > cpr))
        (...,
         swap(select_arg<Idxs>(rows...), select_arg<Idxs + offset>(rows...)));
}

/// @brief Bitonic sorter merge.
/// @param msl monotonic sequence length.
/// @param bl bitonic length.
template <typename... Ts>
__global__ void
bitonic_merge(const unsigned msl,
              const unsigned bl,
              const unsigned size,
              Ts... columns)
{
    const unsigned i   = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned bl2 = bl / 2;
    const unsigned p   = i / bl2;
    const unsigned s   = p * bl + (i & (bl2 - 1));
    const unsigned e   = s + bl2;
    if(e > size) return;

    const unsigned pis = msl / bl; // parts in sequence
    const unsigned msi = p / pis;  // monotonic sequence index

    const bool ascending = (msi & 1) || (msi + 1) * msl >= size;

    cmp_swap_rows(std::index_sequence_for<Ts...>{},
                  ascending,
                  columns[s]...,
                  columns[e]...);
}

template <typename C, typename LabelMap, typename VarMap, typename CPVar>
struct eval;

template <typename Op,
          typename Dst,
          typename... VarArgs,
          typename LabelMap,
          typename VarMap,
          typename CPVar>
struct eval<Compute<Op, Dst, VarArgs...>, LabelMap, VarMap, CPVar>
{
    template <typename Var>
        requires(typelist::elem<Var, typelist::map<fst_t, VarMap>>)
    using var_idx = typelist::lookup<Var, VarMap>;

    using idxs      = typelist::map<var_idx, type_sequence<VarArgs...>>;
    using device_op = remap_comp_t<Op>;

    template <typename... Args>
    __device__ void
    operator()(Args&... args) const
    {
        select_arg<var_idx<Dst>::value>(args...) =
            call_multiselect(device_op{}, idxs{}, args...);
    }
};

template <typename Target, typename LabelMap, typename VarMap, typename CPVar>
struct eval<Jump<Target>, LabelMap, VarMap, CPVar>
{
    static constexpr auto value = typelist::lookup<Target, LabelMap>::value;
    static constexpr auto cpi   = typelist::lookup<CPVar, VarMap>::value;

    template <typename... Args>
    __device__ void
    operator()(Args&... args) const
    {
        select_arg<cpi>(args...) = value;
    }
};

template <typename Var,
          typename Labels,
          typename LabelMap,
          typename VarMap,
          typename CPVar>
struct eval<ParmJump<Var, Labels>, LabelMap, VarMap, CPVar>
{
    static constexpr auto vari  = typelist::lookup<Var, VarMap>::value;
    static constexpr auto cpi   = typelist::lookup<CPVar, VarMap>::value;
    static constexpr auto table = pjmp_arr<ParmJump<Var, Labels>, LabelMap>;

    template <typename... Args>
    __device__ void
    operator()(Args&... args) const
    {
        select_arg<cpi>(args...) = table[select_arg<vari>(args...)];
    }
};

template <typename Cell, typename LabelMap, typename CPVar>
struct compiler
{
    using vars = cell_vars_t<Cell, CPVar>;

    template <typename Command>
    using cuda_eval =
        cuda::eval<Command, LabelMap, typelist::enumerate<vars>, CPVar>;

    using instructions = typelist::map<cuda_eval, Cell>;

    template <typename... Args>
    __device__ void
    operator()(Args&... args) const
    {
        cuda::unroll(instructions{}, args...);
    }
};

template <typename T>
struct fake_array
{
    T val;

    __host__ __device__ const T&
    operator[](unsigned) const noexcept
    {
        return val;
    }

    __host__ __device__ T&
    operator[](unsigned) noexcept
    {
        return val;
    }
};

struct memory_accessor
{
    template <typename Var, typename _Memory>
    auto
    operator()(_Memory& mem) const noexcept
    {
        constexpr auto ov = parse_literal<Var>();
        if constexpr(ov.has_value())
            return fake_array<vartype<Var>>{ov.value()};
        else
            return mem.template get<Var>().data();
    }
};

template <typename T>
__global__ void
set_constant(gsize_t begin, gsize_t end, T* arr, T def_val)
{
    const auto i = blockIdx.x * blockDim.x + threadIdx.x + begin;
    if(i < end) arr[i] = def_val;
}

template <typename CompiledCell, typename... Args>
__global__ void
cell_kernel(gsize_t begin, gsize_t end, CompiledCell cc, Args... args)
{
    const auto i = blockIdx.x * blockDim.x + threadIdx.x + begin;
    if(i < end) cc(args[i]...);
}

/// @brief Runs kernel of cell @p cc.
template <typename CompiledCell, typename _Memory>
void
run_cell_kernel(CompiledCell  cc,
                _Memory&      mem,
                const gsize_t begin,
                const gsize_t end) noexcept
{
    const gsize_t size = end - begin;

    auto launcher = [&](auto... columns) noexcept
    {
        LAUNCH_KERNEL(cell_kernel, size, begin, end, cc, columns...);
        cudaDeviceSynchronize();
    };

    maybe_apply(launcher,
                typelist::maybe_collect(memory_accessor{},
                                        typename CompiledCell::vars{},
                                        mem));
}

/// @brief Sorts memory.
///
/// Memory size must be power of two.
template <typename Vars, typename _Memory>
void
sort(Vars vars, _Memory& mem) noexcept
{
    ASSERT_POW2(mem.size());

    auto launcher = [&](auto... columns) noexcept
    {
        // msl - monotonic sequence length
        // bl - bitonic length
        for(gsize_t msl = 2; msl <= mem.size(); msl *= 2)
            for(gsize_t bl = msl; bl > 1; bl /= 2)
                LAUNCH_KERNEL(bitonic_merge,
                              mem.size() / 2u,
                              msl,
                              bl,
                              mem.size(),
                              columns...);
        cudaDeviceSynchronize();
    };

    maybe_apply(launcher,
                typelist::maybe_collect(memory_accessor{}, vars, mem));
}

template <gsize_t N, std::unsigned_integral cp_t, cp_t halt_pos>
class chunk_ranges :
    public std::ranges::view_interface<chunk_ranges<N, cp_t, halt_pos>>
{
    struct range
    {
        cp_t    cp;
        gsize_t bgn;
        gsize_t end;
    };

    class _iterator : public std::input_iterator_tag
    {
        gsize_t* p_bgns;
        gsize_t* p_ends;
        cp_t     m_cp = 0;

        /// @brief Checks if current range is empty.
        bool
        empty() const noexcept
        {
            return p_ends[m_cp] - p_bgns[m_cp] == 0;
        }

        /// @brief Checks if current range can be jumped to and executed.
        ///
        /// The last range contains terminated cells. We ignore it.
        bool
        executable() const noexcept
        {
            return m_cp < N - 1;
        }

        /// @brief Find next usefull cell.
        void
        next() noexcept
        {
            do ++m_cp;
            while(empty() && executable());
        }

      public:
        using difference_type = std::ptrdiff_t;
        using value_type      = range;

        _iterator() noexcept = default;

        _iterator(cp_t* bgns, cp_t* ends, cp_t cp = 0) noexcept :
            p_bgns(bgns), p_ends(ends), m_cp(cp)
        {
            // find first useful cell
            while(empty() && executable()) ++m_cp;
        }

        bool
        operator==(std::default_sentinel_t) const noexcept
        {
            return !executable();
        }

        bool
        operator==(const _iterator& rhs) const noexcept
        {
            return m_cp == rhs.m_cp;
        }

        std::strong_ordering
        operator<=>(const _iterator& rhs) const noexcept
        {
            return m_cp <=> rhs.m_cp;
        }

        value_type
        operator*() const noexcept
        {
            return {m_cp, p_bgns[m_cp], p_ends[m_cp]};
        }

        _iterator&
        operator++() noexcept
        {
            next();
            return *this;
        }

        _iterator&
        operator++(int) noexcept
        {
            _iterator ret(*this);
            next();
            return ret;
        }
    };

    // host data
    HostAllocator<gsize_t> m_ha;
    gsize_t*               ph_bgns;
    gsize_t*               ph_ends;

    // device data
    DeviceAllocator<gsize_t> m_da;
    gsize_t*                 pd_bgns;
    gsize_t*                 pd_ends;

    // cell pointers columns
    cp_t*   pd_cps;
    cp_t*   ph_cp0;
    gsize_t m_size;

    template <typename A>
    gsize_t*
    alloc(A a, gsize_t n = N)
    {
        return a.allocate(n);
    }

    template <typename A>
    void
    dealloc(gsize_t* data, A a, gsize_t n = N)
    {
        a.deallocate(data, n);
    }

    void
    count()
    {
        pre_counter<N>(ph_bgns, ph_ends);

        HOST_TO_DEVICE(pd_bgns, ph_bgns, N);
        HOST_TO_DEVICE(pd_ends, ph_ends, N);

        LAUNCH_KERNEL(chunk_counter,
                      m_size - 1,
                      pd_bgns,
                      pd_ends,
                      N,
                      pd_cps,
                      m_size,
                      halt_pos);
        cudaDeviceSynchronize();

        DEVICE_TO_HOST(ph_bgns, pd_bgns, N);
        DEVICE_TO_HOST(ph_ends, pd_ends, N);
        DEVICE_TO_HOST(ph_cp0, pd_cps, 1);

        const gsize_t dcp = *ph_cp0 == halt_pos ? N - 1 : *ph_cp0;

        post_counter<N>(ph_bgns, ph_ends, m_size, dcp);
    }

  public:
    chunk_ranges() = default;

    chunk_ranges(cp_t* cps, gsize_t cps_size) :
        ph_bgns(alloc(m_ha)), ph_ends(alloc(m_ha)), pd_bgns(alloc(m_da)),
        pd_ends(alloc(m_da)), pd_cps(cps), ph_cp0(alloc(m_ha, 1)),
        m_size(cps_size)
    {
        ph_bgns[0]     = 0;
        ph_ends[0]     = cps_size;
        ph_bgns[N - 1] = ph_ends[N - 1] = n_pos;
    }

    _iterator
    begin() noexcept
    {
        count();
        return _iterator(ph_bgns, ph_ends, 0);
    }

    std::default_sentinel_t
    end() noexcept
    {
        return std::default_sentinel;
    }

    /// @brief Returns the left boundary of terminated cells.
    gsize_t
    terminated() const
    {
        return ph_bgns[N - 1];
    }

    ~chunk_ranges()
    {
        dealloc(ph_bgns, m_ha);
        dealloc(ph_ends, m_ha);
        dealloc(pd_bgns, m_da);
        dealloc(pd_ends, m_da);
        dealloc(ph_cp0, m_ha);
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
                using t     = vartype<Var>;
                auto column = mem.template get<Var>();
                // fill pinned memory buffer first
                SimpleVector<t, HostAllocator<t>> buffer(column.size());
                if constexpr(std::ranges::input_range<T>)
                    std::ranges::copy(value, buffer.begin());
                else
                    std::ranges::fill(buffer, value);

                // now copy to device
                HOST_TO_DEVICE(column.data(), buffer.data(), column.size());
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
            {
                using t     = vartype<Var>;
                auto column = mem.template get<Var>();
                SimpleVector<t, HostAllocator<t>> buffer(column.size());
                DEVICE_TO_HOST(buffer.data(), column.data(), column.size());
                std::vector<t> ret(buffer.size());
                std::ranges::copy(buffer, ret.begin());
                return ret;
            },
            OutputVars{});
    }
};

template <typename _Program>
class cuda_evaluator
{
    using labelmap  = _Program::labelmap;
    using cells     = _Program::cells;
    using reg_names = _Program::reg_names;
    using reg_types = _Program::reg_types;
    using cp        = _Program::cp;
    using cp_t      = vartype<cp>;
    using vars      = _Program::variables;

    static constexpr gsize_t num_chunks = _Program::num_cells + 1;
    static constexpr auto    halt_pos   = _Program::halt_pos;
    static constexpr input_setter<typename _Program::input_vars>   setter{};
    static constexpr output_getter<typename _Program::output_vars> getter{};

    template <typename C>
    using compile_cell = cuda::compiler<C, labelmap, cp>;

    using compiled_cells = typelist::map<compile_cell, cells>;

    using memory =
        bve::Memory<reg_types, reg_names, cp, SimpleVector, DeviceAllocator>;

    struct single_cell_evaluator
    {
        static constexpr typelist::head<compiled_cells> cell{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const gsize_t n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);
            run_cell_kernel(cell, mem, 0, mem.size());
            return getter(mem);
        }
    };

    struct evaluator
    {
        using rngs_t = chunk_ranges<num_chunks, cp_t, halt_pos>;
        static constexpr typename typelist::to<compiled_cells, std::tuple>
            cells{};

        memory mem;

        template <typename... Args>
        constexpr auto
        operator()(const gsize_t n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);

            cp_t* cps = mem.cp().data();

            LAUNCH_KERNEL(set_constant, n, 0, n, cps, static_cast<cp_t>(0));
            cudaDeviceSynchronize();

            rngs_t chunks(cps, n);

            while(chunks.terminated() != 0)
            {
                std::ranges::for_each(
                    chunks,
                    [&](auto rng) noexcept
                    {
                        auto [cp, bgn, end] = rng;
                        visit(
                            cp,
                            [&](auto cell) noexcept
                            { run_cell_kernel(cell, mem, bgn, end); },
                            cells,
                            sink);
                    });

                sort(vars{}, mem);
#if 0
                auto columns = typelist::maybe_collect<std::tuple>(
                    [&]<typename Var>() constexpr noexcept
                    {
                        using t     = vartype<Var>;
                        auto column = mem.template get<Var>();
                        SimpleVector<t, HostAllocator<t>> buffer(column.size());
                        DEVICE_TO_HOST(buffer.data(),
                                       column.data(),
                                       column.size());
                        std::vector<t> ret(buffer.size());
                        std::ranges::copy(buffer, ret.begin());
                        return ret;
                    },
                    vars{});

                typelist::for_each(
                    []<typename Var>()
                    {
                        using name                   = varname<Var>;
                        constexpr auto             l = intlist::length<name>;
                        constexpr auto             a = intlist::array<name>;
                        constexpr std::string_view s(a, l);
                        for(char c: s) putchar(c);
                        putchar('\t');
                        putchar('\t');
                    },
                    vars{});
                putchar('\n');
                for(gsize_t i = 0; i < n; ++i)
                {
                    lucid::for_each([&](const auto& c)
                                    { printf("%*u\t", 10, c[i]); },
                                    columns);
                    putchar('\n');
                }
#endif
            }
            return getter(mem);
        }
    };

  public:
    using type = std::
        conditional_t<_Program::singular, single_cell_evaluator, evaluator>;
};

template <typename _Program>
using evaluator = cuda_evaluator<_Program>::type;
} // namespace lucid::cuda
