// -*- C++ -*-
// root_finding.cpp
#include "bm.hpp"
#include "utils.hpp"

#include <common/random.hpp>
#include <common/timer.hpp>
#include <common/typeinfo.hpp>
#include <tests/root_finding.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/subrange.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <algorithm>
#include <vector>

using namespace loom;
using namespace vm;

template <typename T>
using AV = std::vector<T, simd::SIMDAlignedAllocator<T>>;

template <std::floating_point T>
struct Base
{
    AV<T>& a;
    AV<T>& b;
    AV<T>& c;
    AV<T>& d;

    volatile T*                         dst1;
    volatile uint_of_size_t<sizeof(T)>* dst2;
};

template <std::floating_point T>
struct Ref : Base<T>
{
    using base = Base<T>;

    std::string
    info() const noexcept
    {
        return fmt::format("reference|{}|{}",
                           TypeInfo<T>::chars,
                           base::a.size());
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        ElapsedTimer<> timer;

        for(std::size_t i = 0; i < base::a.size(); ++i)
            std::tie(*base::dst1, *base::dst2) =
                val_cubic_fz(base::a[i], base::b[i], base::c[i], base::d[i]);

        return timer.elapsed();
    }
};

template <std::floating_point T>
struct BSE : Base<T>
{
    using base  = Base<T>;
    using cnsts = constants<T>;
    using errt  = cnsts::errt;

    std::string
    info() const noexcept
    {
        return fmt::format("bse|{}|{}", TypeInfo<T>::chars, base::a.size());
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        vm::bse::function<bisection<T, errt>> f;

        ElapsedTimer<> timer;

        for(std::size_t i = 0; i < base::a.size(); ++i)
            std::tie(*base::dst1, *base::dst2) =
                f(base::a[i], base::b[i], base::c[i], base::d[i]);

        return timer.elapsed();
    }
};

template <std::floating_point T>
struct SIMDCompact : Base<T>
{
    using base  = Base<T>;
    using cnsts = constants<T>;
    using errt  = cnsts::errt;

    static constexpr std::size_t N = simd::native_vector_extent_v<T>;

    std::string
    info() const noexcept
    {
        return fmt::format("compact|{}|{}", TypeInfo<T>::chars, base::a.size());
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        ElapsedTimer<> timer;

        vm::simd::compact_function<bisection<T, errt>> f;

        for(std::size_t i = 0; i < base::a.size(); i += N)
        {
            const auto [s, c] = f(simd::load<N>(std::addressof(base::a[i])),
                                  simd::load<N>(std::addressof(base::b[i])),
                                  simd::load<N>(std::addressof(base::c[i])),
                                  simd::load<N>(std::addressof(base::d[i])));

            simd::store<N>(base::dst1, s);
            simd::store<N>(base::dst2, c);
        }

        return timer.elapsed();
    }
};

template <std::floating_point T, bool ts, typename UTR>
struct SIMDStream : Base<T>
{
    using base  = Base<T>;
    using cnsts = constants<T>;
    using errt  = cnsts::errt;

    static constexpr std::size_t N = simd::native_vector_extent_v<T>;

    std::size_t stream_size = 64;

    std::string
    info() const noexcept
    {
        return fmt::format("stream|ts:{}|UTR:{:.0f}%|size:{}|{}|{}",
                           ts,
                           ratio2percent<T, UTR>(),
                           stream_size,
                           TypeInfo<T>::chars,
                           base::a.size());
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        vm::simd::hybrid_stream<bisection<T, errt>,
                                ts,
                                true,
                                simd::SUCCESSOR_MAJOR,
                                UTR>
            s(stream_size);

        auto view =
            ranges::views::zip(base::a, base::b, base::c, base::d) |
            std::views::transform([](const auto& v)
                                  { return std::tuple<T, T, T, T>{v}; });

        auto it = s.set_inputs(view);

        ElapsedTimer<> timer;

        s();
        s.sort();

        while(it != std::ranges::end(view))
        {
            auto exc = s.exchange(ranges::subrange(it, std::ranges::end(view)));
            it += std::ranges::size(exc);
            for(const auto e: exc) std::tie(*base::dst1, *base::dst2) = e;
            s();
            s.sort();
        }

        while(!s.done())
        {
            s();
            s.sort();
        }

        return timer.elapsed();
    }
};

template <std::floating_point T, bool ts, typename UTR>
struct SIMDFullRange : Base<T>
{
    using base  = Base<T>;
    using cnsts = constants<T>;
    using errt  = cnsts::errt;

    static constexpr std::size_t N = simd::native_vector_extent_v<T>;

    std::string
    info() const noexcept
    {
        return fmt::format("full-range|ts:{}|UTR:{:.0f}%|{}|{}",
                           ts,
                           ratio2percent<T, UTR>(),
                           TypeInfo<T>::chars,
                           base::a.size());
    }

    std::chrono::steady_clock::duration
    operator()() const noexcept
    {
        vm::simd::hybrid_function<bisection<T, errt>,
                                  ts,
                                  true,
                                  simd::SUCCESSOR_MAJOR,
                                  UTR>
            f(base::a.size());

        ElapsedTimer<> timer;

        const auto [s, i] = f(base::a, base::b, base::c, base::d);

        const auto ret = timer.elapsed();

        for(const auto v: s) *base::dst1 = v;
        for(const auto v: i) *base::dst2 = v;

        return ret;
    }
};

template <std::floating_point T, typename PR>
void
run_root_finding(const PR& pr, const std::size_t sample_size, RandomState& rs)
{
    using cnsts                  = constants<T>;
    constexpr T           lbound = cnsts::lbound;
    constexpr T           rbound = cnsts::rbound;
    constexpr std::size_t N      = simd::native_vector_extent_v<T>;

    auto g_f  = rs.make_gen<T>(lbound, rbound);
    auto g_b  = rs.make_gen<bool>(0.5);
    auto g_a  = rs.make_gen<T>(1, rbound);
    auto g_a1 = [&]() noexcept { return g_a() * (g_b() ? T(-1) : T(1)); };

    AV<T> a(sample_size), b(sample_size), c(sample_size), d(sample_size);

    for(std::size_t i = 0; i < sample_size; ++i)
    {
        a[i] = g_a1();
        b[i] = g_f();
        c[i] = g_f();
        d[i] = g_f();
    }

    volatile T* dst1 = static_cast<T*>(
        __builtin_alloca_with_align(N * sizeof(T), simd::simd_width * 8));
    volatile uint_of_size_t<sizeof(T)>* dst2 =
        static_cast<uint_of_size_t<sizeof(T)>*>(
            __builtin_alloca_with_align(N * sizeof(T), simd::simd_width * 8));

    using utr1 = std::ratio<1, 16>;
    using utr2 = std::ratio<1, 2>;

    run(pr,
        Ref<T>{
            a,
            b,
            c,
            d,
            dst1,
            dst2
    },
        BSE<T>{a, b, c, d, dst1, dst2},
        SIMDCompact<T>{a, b, c, d, dst1, dst2},
        SIMDStream<T, false, utr1>{{a, b, c, d, dst1, dst2}, 64},
        SIMDStream<T, true, utr1>{{a, b, c, d, dst1, dst2}, 64},
        SIMDStream<T, false, utr1>{{a, b, c, d, dst1, dst2}, 32},
        SIMDStream<T, true, utr1>{{a, b, c, d, dst1, dst2}, 32},
        SIMDStream<T, false, utr1>{{a, b, c, d, dst1, dst2}, 128},
        SIMDStream<T, true, utr1>{{a, b, c, d, dst1, dst2}, 128},
        SIMDStream<T, false, utr2>{{a, b, c, d, dst1, dst2}, 64},
        SIMDStream<T, true, utr2>{{a, b, c, d, dst1, dst2}, 64},
        SIMDStream<T, false, utr2>{{a, b, c, d, dst1, dst2}, 32},
        SIMDStream<T, true, utr2>{{a, b, c, d, dst1, dst2}, 32},
        SIMDStream<T, false, utr2>{{a, b, c, d, dst1, dst2}, 128},
        SIMDStream<T, true, utr2>{{a, b, c, d, dst1, dst2}, 128},
        SIMDFullRange<T, false, utr1>{a, b, c, d, dst1, dst2},
        SIMDFullRange<T, true, utr1>{a, b, c, d, dst1, dst2},
        SIMDFullRange<T, false, utr2>{a, b, c, d, dst1, dst2},
        SIMDFullRange<T, true, utr2>{a, b, c, d, dst1, dst2});
}

int
main(int argc, char* argv[])
{
    const auto parse_results =
        argparse::parse(make_opts(sample_size_opt), {argc, argv});
    const std::size_t sample_size = align_up(
        parse_results.template get_opt<'s'>().value_or(def_sample_size),
        64ul);
    RandomState rs;

    run_root_finding<double>(parse_results, sample_size, rs);
    run_root_finding<float>(parse_results, sample_size, rs);

    return 0;
}
