// -*- C++ -*-
// vm_mp.cpp
#include <common/test.hpp>
#include <vm/mp.hpp>
#include <vm/simd.hpp>

#include "generators.hpp"
#include "programs.hpp"
#include "root_finding.hpp"
#include "vm_test_suite_utils.hpp"

#include <algorithm>
#include <vector>

#include <fmt/ranges.h>

using namespace loom;
using namespace vm;
using namespace test;

using fac_p = fac_test<std::uint64_t>;
using fib_p = fib_test<std::uint64_t>;
using fz_p  = bisection<double, constants<double>::errt>;

using programs = type_sequence<fac_p, fib_p, fz_p>;

template <std::floating_point T>
auto
gm_fz(RandomState& rs) noexcept
{
    constexpr T lbound = constants<T>::lbound;
    constexpr T rbound = constants<T>::rbound;

    return [g_a1 = [g_a = rs.make_gen<T>(1, rbound),
                    g_b = rs.make_gen<bool>(0.5)]() mutable noexcept
            { return g_a() * (g_b() ? T(-1) : T(1)); },
            g_f = rs.make_gen<T>(lbound, rbound)]() mutable noexcept
    { return std::tuple{g_a1(), g_f(), g_f(), g_f()}; };
}

template <typename G>
auto
gen_input(unsigned num_runs, G& g) noexcept
{
    std::vector<decltype(g())> v;
    v.reserve(num_runs);
    std::ranges::generate_n(std::back_inserter(v), num_runs, g);
    return v;
}

template <typename T, typename V>
auto
mk_val(const std::vector<T>& i, V v) noexcept
{
    auto l = [v](const auto& in) noexcept { return invopply(v, in); };
    std::vector<decltype(l(i[0]))> ret;
    ret.reserve(i.size());
    std::ranges::transform(i, std::back_inserter(ret), l);
    std::ranges::sort(ret);
    return ret;
}

using v_in_fac = std::vector<input_type<fac_p>>;
using v_in_fib = std::vector<input_type<fib_p>>;
using v_in_fz  = std::vector<input_type<fz_p>>;

using v_out_fac = std::vector<output_type<fac_p>>;
using v_out_fib = std::vector<output_type<fib_p>>;
using v_out_fz  = std::vector<output_type<fz_p>>;

template <template <typename> typename _Context,
          std::size_t threads_number,
          bool        affine_threads>
void
test_stream(std::string_view compiler_name,
            Track&           track,
            const unsigned   stream_size,
            const unsigned   num_runs,
            const v_in_fac&  in_fac,
            const v_in_fib&  in_fib,
            const v_in_fz&   in_fz,
            const v_out_fac& out_val_fac,
            const v_out_fib& out_val_fib,
            const v_out_fz&  out_val_fz) noexcept
{
    v_out_fac out_fac;
    v_out_fib out_fib;
    v_out_fz  out_fz;

    auto sort_outs = [&]() noexcept
    {
        std::ranges::sort(out_fac);
        std::ranges::sort(out_fib);
        std::ranges::sort(out_fz);
    };
    auto reset_outs = [&]() noexcept
    {
        out_fac.clear();
        out_fib.clear();
        out_fz.clear();

        out_fac.reserve(num_runs);
        out_fib.reserve(num_runs);
        out_fz.reserve(num_runs);
    };

    auto oc = mp::make_output_collection(out_fac, out_fib, out_fz);

    auto test_shop = [&](auto& shop) noexcept
    {
        auto ic = shop.bind_inputs(in_fac, in_fib, in_fz);
        {
            auto tp = shop.get_thread_pool();
            if constexpr(affine_threads)
                tp.affine(threads_number <=
                                  std::thread::hardware_concurrency() / 2 ?
                              AFFINITY::PHYSICAL :
                              AFFINITY::FULL);

            while(!mp::is_complete(ic)) shop.exchange(ic, oc);
            shop.set();
        }
        shop.finalize(oc);
        ASSERT(out_fac.size() == out_val_fac.size(), "sizes don't match");
        ASSERT(out_fib.size() == out_val_fib.size(), "sizes don't match");
        ASSERT(out_fz.size() == out_val_fz.size(), "sizes don't match");

        sort_outs();

        constexpr test::range_cmp_estimator<> rng_cmp{};

        const double r_fac = rng_cmp(out_fac, out_val_fac);
        const double r_fib = rng_cmp(out_fib, out_val_fib);
        const double r_fz  = rng_cmp(out_fz, out_val_fz);

        return (r_fac + r_fib + r_fz) / 3.;
    };

    const bool really_affine =
        affine_threads && threads_number <= std::thread::hardware_concurrency();
    const std::string_view affine_s = really_affine ? "affine" : "";

    track.check(NamedFuzzyTest(
        [&]() noexcept
        {
            mp::Shop<programs, _Context, threads_number> shop(stream_size);
            return test_shop(shop);
        },
        fmt::format("Static {} {} {} threads",
                    compiler_name,
                    threads_number,
                    affine_s),
        0.01));

    reset_outs();

    track.check(NamedFuzzyTest(
        [&]() noexcept
        {
            mp::Shop<programs, _Context, dynamic_size> shop(threads_number,
                                                            stream_size);
            return test_shop(shop);
        },
        fmt::format("Dynamic {} {} {} threads",
                    compiler_name,
                    threads_number,
                    affine_s),
        0.01));
}

template <simd::STITCH stitch, simd::SORT sort_method>
struct make_simd_context
{
    static std::string
    get_name() noexcept
    {
        return fmt::format("SIMD {} {}",
                           stitch_name<stitch>::chars,
                           sort_name<sort_method>::chars);
    }

    template <Kernel K>
    using compiler =
        simd::stream<K, stitch, sort_method, true, std::ratio<1, 2>>;

    template <Kernel K>
    using context = mp::Context<K, compiler>;
};

template <simd::STITCH stitch,
          simd::SORT   sort_method,
          std::size_t  tn,
          typename... Args>
void
test_stream_affines(Args&&... args) noexcept
{
    using helper = make_simd_context<stitch, sort_method>;
    test_stream<helper::template context, tn, false>(
        helper::get_name(),
        std::forward<Args>(args)...);
    test_stream<helper::template context, tn, true>(
        helper::get_name(),
        std::forward<Args>(args)...);
}

template <simd::STITCH stitch,
          simd::SORT   sort_method,
          std::size_t... tns,
          typename... Args>
void
test_simd_stream(std::integer_sequence<std::size_t, tns...>,
                 Args&&... args) noexcept
{
    (...,
     test_stream_affines<stitch, sort_method, tns>(
         std::forward<Args>(args)...));
}

template <simd::STITCH stitch, auto... sorts, typename... Args>
void
test_1stitch(Args&&... args) noexcept
{
    (...,
     test_simd_stream<stitch, sorts>(
         std::integer_sequence<std::size_t, 2, 4, 8>{},
         std::forward<Args>(args)...));
}

template <auto... stitches, typename... Args>
void
test_simd_all(Args&&... args) noexcept
{
    (...,
     test_1stitch<stitches,
                  simd::bve_INTROSORT,
                  simd::bve_COUNTING_SORT,
                  simd::COUNTING_SORT>(std::forward<Args>(args)...));
}

int
main(int argc, char* argv[])
{
    simd::print_info();
    CPUTopology top;
#if FMT_VERSION < 110000
    fmt::print("CPU topology: {}\n", top);
#else
    fmt::println("CPU topology: {}", top);
#endif

    Track          track;
    RandomState    rs;
    const unsigned num_runs =
        align_top(parse_numruns<10000>(argc, argv), 32u * 8u);
    const unsigned stream_size = align_top(num_runs / 1000, 32u * 8u);
    fmt::print("Input size: {}\n", num_runs);
    fmt::print("Stream size: {}\n", stream_size);

    auto g_fac = GM_FAC<std::uint64_t>{}(rs);
    auto g_fib = GM_FIB<std::uint64_t>{}(rs);
    auto g_fz  = gm_fz<double>(rs);

    const v_in_fac in_fac = gen_input(num_runs, g_fac);
    const v_in_fib in_fib = gen_input(num_runs, g_fib);
    const v_in_fz  in_fz  = gen_input(num_runs, g_fz);

    const v_out_fac out_val_fac = mk_val(in_fac, val_fac);
    const v_out_fib out_val_fib = mk_val(in_fib, val_fib);
    const v_out_fz  out_val_fz  = mk_val(in_fz, val_cubic_fz);

    test_simd_all<simd::WAVEFRONT, simd::MINIKERNEL, simd::MASKED>(track,
                                                                   stream_size,
                                                                   num_runs,
                                                                   in_fac,
                                                                   in_fib,
                                                                   in_fz,
                                                                   out_val_fac,
                                                                   out_val_fib,
                                                                   out_val_fz);

    return track.ret_code();
}
