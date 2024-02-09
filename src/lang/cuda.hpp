// -*- C++ -*-
// cuda.hpp
//

#ifndef __CUDA__
#error "This header requires CUDA"
#endif

#pragma once

#include "bve.hpp"

// stupid cmake adds -NDEBUG to RelWithDebInfo for CUDA
// thus we use #ifdef DEBUG instead ifndef NDEBUG
#ifdef DEBUG
#include <cassert>
#include <cstdio>
void
cuda_check(cudaError_t r) noexcept
{
    if(r != cudaSuccess)
        std::fprintf(stderr, "CUDA Error: %s", cudaGetErrorString(r));
    assert(r == cudaSuccess);
}
#else
void
cuda_check(cudaError_t) noexcept
{
}
#endif

#define DEF_CUDA_ALLOC(name, a, f)              \
    template <typename T>                       \
    struct name                                 \
    {                                           \
        using value_type = T;                   \
        using pointer    = T*;                  \
        name() noexcept  = default;             \
        pointer                                 \
        allocate(unsigned n)                    \
        {                                       \
            pointer ret;                        \
            cuda_check(a(&ret, n * sizeof(T))); \
            return ret;                         \
        }                                       \
        void                                    \
        deallocate(pointer p, unsigned)         \
        {                                       \
            cuda_check(f(p));                   \
        }                                       \
    };

namespace lucid::cuda
{
DEF_CUDA_ALLOC(DeviceAllocator, cudaMalloc, cudaFree)
DEF_CUDA_ALLOC(HostAllocator, cudaMallocHost, cudaFreeHost)

#undef DEF_CUDA_ALLOC

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
struct remap_comp<choice>
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

#undef DEF_REMAP_UN
#undef DEF_REMAP_BIN

template <typename Op>
using remap_comp_t = remap_comp<Op>::type;

template <unsigned I, typename Head, typename... Args>
__device__ decltype(auto)
select_arg(Head& head, Args&... args)
{
    static_assert(I < sizeof...(args) + 1, "Index out of range");

    if constexpr(I == 0)
        return (head);
    else if constexpr(sizeof...(Args))
        return select_arg<I - 1>(args...);
}

template <typename F, typename... Args, unsigned... Is>
__device__ auto
call_multiselect(F f, std::index_sequence<Is...>, Args&... args)
{
    return f(select_arg<Is>(args...)...);
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

    using idxs = typelist::to_integer_sequence<
        typelist::map<var_idx, type_sequence<VarArgs...>>>;
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
    using vars = cell_vars_t<Cell>;

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

/// @brief Current chunk size.
/// @param size Size of @p cps.
/// @param cps Cell pointers.
template <std::integer I>
__device__ unsigned
chunk_size(unsigned size, I* cps)
{
    const I  cp = *cps;
    unsigned i  = 0;
    while(i < size && cps[i] == cp) ++i;
    return i;
}

/// @brief Fills in ranges of chunks: @p bgns and @p ends.
/// @param size size of @p cps.
/// @param cc cell count, size of @p bgns and @p ends.
/// @param bgns begin points of ranges.
/// @param ends end points of ranges.
/// @param cps cell pointers.
/// @param ttor terminator constant.
template <std::integer I>
__global__ void
chunk_counter(unsigned size, unsigned cc, I* bgns, I* ends, I* cps, I ttor)
{
    unsigned pos = 0;
    I        cp  = cps[pos];
    do {
        const unsigned offset = chunk_size(cps + pos, size - pos);
        bgns[cp]              = pos;
        ends[cp]              = pos + offset;
        pos += offset;
        cp = cps[pos];
    } while(pos < size && cp != ttor);

    bgns[cc] = pos;
    ends[cc] = size;
}

template <typename CompiledCell, typename... Args>
__global__ void
kernel(unsigned size, CompiledCell cc, Args... args)
{
    const auto index  = blockIdx.x * blockDim.x + threadIdx.x;
    const auto stride = blockDim.x * gridDim.x;
    for(auto i = index; i < size; i += stride) cc(args[i]...);
}

template <typename CompiledCell, typename _Memory>
void
launch_kernel(CompiledCell cc, _Memory& mem, unsigned bs = 256) noexcept
{
    auto data = typelist::maybe_collect(memory_accessor{},
                                        typename CompiledCell::vars{},
                                        mem);

    unsigned nb = mem.size() / bs;
    if(mem.size() % bs) ++nb;

    auto launcher = [&](auto... args) noexcept
    {
        if(nb) { kernel<<<nb, bs>>>(mem.size(), cc, args...); }
        cudaDeviceSynchronize();
    };

    maybe_apply(launcher, data);
}

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
                cuda_check(cudaMemcpy(column.data(),
                                      buffer.data(),
                                      column.size() * sizeof(t),
                                      cudaMemcpyHostToDevice));
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
                cuda_check(cudaMemcpy(buffer.data(),
                                      column.data(),
                                      column.size() * sizeof(t),
                                      cudaMemcpyDeviceToHost));
                std::vector<t> ret(buffer.size());
                std::ranges::copy(buffer, ret.begin());
                return ret;
            },
            OutputVars{});
    }
};

template <typename _Program>
struct evaluator
{
    using labelmap  = typename _Program::labelmap;
    using cells     = typename _Program::cells;
    using reg_names = typename _Program::reg_names;
    using reg_types = typename _Program::reg_types;
    using cp        = typename _Program::cp;

    static constexpr unsigned num_views = _Program::num_cells + 1;
    static constexpr auto     end_pos   = _Program::end_pos;
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
        operator()(const unsigned n, Args&&... args) noexcept
        {
            mem.resize(n);
            setter(mem, std::forward<Args>(args)...);
            launch_kernel(cell, mem);
            return getter(mem);
        }
    };
};
} // namespace lucid::cuda
