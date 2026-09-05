// -*- C++ -*-
// utils.hpp
//

#pragma once

#include <common/argparse.hpp>
#include <common/logging.hpp>
#include <common/math.hpp>
#include <common/random.hpp>
#include <common/string.hpp>
#include <common/timer.hpp>
#include <common/typeinfo.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

#include <range/v3/view/chunk.hpp>
#include <range/v3/view/zip.hpp>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace loom
{
template <vm::simd::ReconvMethod rcnv>
constexpr std::string_view
rcnv_string() noexcept
{
    switch(rcnv)
    {
    case(vm::simd::WARP_FAIR): return "warp-fair";
    case(vm::simd::HYBRID_MAJOR): return "hybrid-major";
    case(vm::simd::WARP_MAJOR): return "warp-major";
    case(vm::simd::HYBRID_FAIR): return "hybrid-fair";
    case(vm::simd::SUCCESSOR_FIRST): return "successor-first";
    case(vm::simd::SUCCESSOR_MAJOR): return "successor-major";
    }
}

inline constexpr std::size_t def_num_runs    = 10;
inline constexpr std::size_t def_sample_size = 1000000;

template <std::size_t ss = def_sample_size>
inline constexpr auto sample_size_opt =
    argparse::option<'s', "sample-size">("sample size",
                                         "N",
                                         ss,
                                         read_number<std::size_t, false>);

template <typename... Opts>
auto
make_opts(Opts... opts) noexcept
{
    return std::tuple{
        argparse::flag<'v', "verbose">("verbose logging"),
        argparse::option<'l', "log">("path to benchmark logs", "FILE", ""),
        argparse::option<'n', "num-runs">(
            "number of runs for each benchmark. Default: 10",
            "N",
            def_num_runs,
            read_number<std::size_t, false>),
        argparse::option<'S', "seed">("random seed",
                                      "S",
                                      0,
                                      read_number<unsigned, false>),
        opts...};
}

template <typename PR>
std::size_t
get_num_runs(Logger& logger, const PR& parse_results) noexcept
{
    const std::optional<std::size_t> onr =
        parse_results.template get_opt<'n'>();

    if(!onr)
        logger.warning("Failed to parse num-runs. Falling to default {}",
                       def_num_runs);

    return onr.value_or(def_num_runs);
}

template <typename PR>
std::size_t
get_sample_size(Logger& logger, const PR& parse_results) noexcept
{
    const std::optional<std::size_t> onr =
        parse_results.template get_opt<'s'>();

    if(!onr)
        logger.warning("Failed to parse sample-size. Falling to default {}",
                       def_sample_size);

    return align_up(onr.value_or(def_sample_size), 64ul);
}

template <typename PR>
std::FILE*
get_file(Logger& logger, const PR& parse_results) noexcept
{
    std::FILE* file = nullptr;

    const std::string_view path = parse_results.template get_opt<'l'>();

    if(!path.empty())
    {
        file = std::fopen(path.data(), "a+");
        if(!file)
            logger.error("Failed to open {} for read/write: {}",
                         path,
                         strerror(errno));
        else
            logger.info("Logging results into {}", path);
    }

    return file;
}

template <typename PR>
Logger
get_logger(const PR& pr) noexcept
{
    return Logger(pr.template get_opt<'v'>() ? Logger::DEBUG : Logger::INFO);
}

void
log_info(Logger& logger, std::size_t num_runs) noexcept
{
    logger.info("CPU: {}; SIMD: {}", get_cpu_model(), vm::simd::info::value);
    logger.info("clang-{}; fast-math: {}", rtrim(__clang_version__), fast_math);
    logger.info("Running each benchmark {} times", num_runs);
}

template <typename... BMs>
static void
run(Logger&     logger,
    std::FILE*  file,
    std::size_t num_runs,
    BMs&&... bms) noexcept
{
    log_info(logger, num_runs);
    print_results(run_benchmarks(logger, file, num_runs, bms...));
}

template <typename T, typename R>
static constexpr double
ratio2percent() noexcept
{
    constexpr std::size_t N = vm::simd::native_vector_extent_v<T>;
    return static_cast<double>(vm::simd::get_threshold_v<N, R>) /
           static_cast<double>(N) * 100;
}

template <typename T>
using AV = std::vector<T, vm::simd::SIMDAlignedAllocator<T>>;

template <typename T>
using add_const_ref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

template <template <typename> typename KT,
          template <typename> typename GM,
          typename R>
struct StandardBenchmarkSuite
{
    struct zipper_t
    {
        template <typename MT>
            requires(!is_tuple_v<std::remove_cvref_t<MT>>)
        decltype(auto)
        operator()(MT&& mt) const noexcept
        {
            return std::forward<MT>(mt);
        }

        template <typename... Vs>
        auto
        operator()(Vs&... vs) const noexcept
        {
            return ranges::views::zip(vs...);
        }

        template <typename... Vs>
        auto
        operator()(const Vs&... vs) const noexcept
        {
            return ranges::views::zip(vs...);
        }
    };

    static constexpr zipper_t zipper{};

    template <typename T>
    struct Base
    {
        using K         = KT<T>;
        using in_types  = vm::input_types<K>;
        using out_types = vm::output_types<K>;

        using invs_t =
            typelist::maybe_to<typelist::map<AV, in_types>, std::tuple>;
        using outs_t = typelist::maybe_to<
            typelist::map<std::add_volatile_t,
                          typelist::map<std::add_pointer_t, out_types>>,
            std::tuple>;

        using inrefs_t =
            typelist::maybe_to<typelist::map<add_const_ref_t, in_types>,
                               std::tuple>;

        const std::size_t sample_size;

        const invs_t& inputs;
        outs_t        sink;

        decltype(auto)
        deref_sink() const noexcept
        {
            return invomap([](volatile auto* p) noexcept -> decltype(auto)
                           { return *p; },
                           sink);
        }

        template <typename F, typename RngProj = std::identity>
        std::chrono::steady_clock::duration
        scalar(F&& f, RngProj&& proj = RngProj{}) const noexcept
        {
            ElapsedTimer<> timer;

            std::ranges::for_each(proj(invopply(zipper, Base::inputs)),
                                  [&](const auto& input) noexcept
                                  {
                                      Base::deref_sink() =
                                          invopply(f, vm::input_type<K>{input});
                                  });

            return timer.elapsed();
        }
    };

    template <typename T>
    struct Ref : Base<T>
    {
        using base = Base<T>;

        std::string
        info() const noexcept
        {
            return fmt::format("reference;{};{}",
                               TypeInfo<T>::chars,
                               base::sample_size);
        }

        std::chrono::steady_clock::duration
        operator()() const noexcept
        {
            return base::scalar(R{});
        }
    };

    template <typename T>
    struct BSE : Base<T>
    {
        using base = Base<T>;
        using K    = base::K;

        std::string
        info() const noexcept
        {
            return fmt::format("scalar;{};{}",
                               TypeInfo<T>::chars,
                               base::sample_size);
        }

        std::chrono::steady_clock::duration
        operator()() const noexcept
        {
            vm::bse::function<K> f;

            return base::scalar(f);
        }
    };

    template <typename T>
    struct SIMDCompact : Base<T>
    {
        using base     = Base<T>;
        using K        = base::K;
        using outs_t   = base::outs_t;
        using inrefs_t = base::inrefs_t;

        static constexpr std::size_t N = vm::simd::native_vector_extent_v<T>;

        std::string
        info() const noexcept
        {
            return fmt::format("predicated;{};{}",
                               TypeInfo<T>::chars,
                               base::sample_size);
        }

        std::chrono::steady_clock::duration
        operator()() const noexcept
        {
            ElapsedTimer<> timer;

            vm::simd::compact_function<K> f;

            std::ranges::for_each(
                ranges::views::chunk(invopply(zipper, base::inputs), N),
                [&f, sink = base::sink](auto chunk) noexcept
                {
                    const auto r = invopply(
                        f,
                        invomap(
                            [](const auto& i) noexcept
                            { return vm::simd::load<N>(std::addressof(i)); },
                            inrefs_t{chunk[0]}));
                    if constexpr(is_tuple_v<outs_t>)
                        zip_with_for_each(
                            []<typename V>(
                                volatile V*                      s,
                                const vm::simd::native_vector<V> r) noexcept
                            { vm::simd::store<N>(s, r); },
                            sink,
                            r);
                    else
                        vm::simd::store<N>(sink, r);
                });

            return timer.elapsed();
        }
    };

    template <typename T, bool ts, typename UTR>
    struct SIMDFullRange : Base<T>
    {
        using base   = Base<T>;
        using K      = base::K;
        using outs_t = base::outs_t;

        std::string
        info() const noexcept
        {
            return fmt::format("hybrid-full-range;ts:{};UTR:{:.0f}%;{};{}",
                               ts,
                               ratio2percent<T, UTR>(),
                               TypeInfo<T>::chars,
                               base::sample_size);
        }

        std::chrono::steady_clock::duration
        operator()() const noexcept
        {
            vm::simd::
                hybrid_function<K, ts, true, vm::simd::SUCCESSOR_MAJOR, UTR>
                    f(base::sample_size);

            ElapsedTimer<> timer;

            const auto r = invopply(f, base::inputs);

            const auto ret = timer.elapsed();

            if constexpr(is_tuple_v<outs_t>)
                zip_with_for_each(
                    [](volatile auto* s, auto&& v) noexcept
                    {
                        for(const auto i: v) *s = i;
                    },
                    base::sink,
                    r);
            else
                for(const auto i: r) *base::sink = i;

            return ret;
        }
    };

    template <typename T, bool ts, typename UTR>
    struct SIMDStream : Base<T>
    {
        using base = Base<T>;
        using K    = base::K;

        static constexpr std::size_t N = vm::simd::native_vector_extent_v<T>;

        std::size_t stream_size = 64;

        std::string
        info() const noexcept
        {
            return fmt::format("hybrid-stream;ts:{};UTR:{:.0f}%;size:{};{};{}",
                               ts,
                               ratio2percent<T, UTR>(),
                               stream_size,
                               TypeInfo<T>::chars,
                               base::sample_size);
        }

        std::chrono::steady_clock::duration
        operator()() const noexcept
        {
            vm::simd::hybrid_stream<K, ts, true, vm::simd::SUCCESSOR_MAJOR, UTR>
                s(stream_size);

            auto view = std::views::transform(invopply(zipper, base::inputs),
                                              [](const auto& v) noexcept
                                              { return vm::input_type<K>{v}; });

            auto it = s.set_inputs(view);

            ElapsedTimer<> timer;

            s();
            s.sort();

            while(it != std::ranges::end(view))
            {
                auto exc =
                    s.exchange(ranges::subrange(it, std::ranges::end(view)));
                it += std::ranges::size(exc);
                for(const auto e: exc) base::deref_sink() = e;
                s();
                s.sort();
            }

            while(!s.done())
            {
                s();
                s.sort();
            }

            for(const auto e: s.get_output_zipped()) base::deref_sink() = e;

            return timer.elapsed();
        }
    };

    template <typename T, typename... Args>
    auto
    operator()(const std::size_t sample_size,
               RandomState&      rs,
               Args&&... args) const noexcept
    {
        using bt        = Base<T>;
        using in_types  = bt::in_types;
        using out_types = bt::out_types;
        using invs_t    = bt::invs_t;
        using outs_t    = bt::outs_t;

        constexpr GM<T> gm{};
        auto            g = gm(rs);

        invs_t inputs = typelist::maybe_collect(
            in_types{},
            []<typename I>(const std::size_t sample_size) noexcept
            { return AV<I>(sample_size); },
            sample_size);

        std::ranges::for_each(invopply(zipper, inputs),
                              [&g](auto&& input) noexcept { input = g(); });

        outs_t sink = typelist::maybe_collect(
            out_types{},
            []<typename O>() noexcept
            {
                return static_cast<O*>(__builtin_alloca_with_align(
                    vm::simd::native_vector_extent_v<O> * sizeof(O),
                    vm::simd::simd_width * 8));
            });

        if constexpr(KT<T>::is_straightline)
            return run_benchmarks(std::forward<Args>(args)...,
                                  Ref<T>{sample_size, inputs, sink},
                                  BSE<T>{sample_size, inputs, sink},
                                  SIMDCompact<T>{sample_size, inputs, sink});
        else
        {
            using utr1 = std::ratio<1, 2>;
            using utr2 = std::ratio<1, 16>;

            using ss1 = SIMDStream<T, true, utr1>;
            using ss2 = SIMDStream<T, false, utr1>;
            using ss3 = SIMDStream<T, true, utr2>;
            using ss4 = SIMDStream<T, false, utr2>;

            return run_benchmarks(
                std::forward<Args>(args)...,

                // clang-format off
                Ref<T>{sample_size, inputs, sink},
                // clang-format on

                BSE<T>{sample_size, inputs, sink},
                SIMDCompact<T>{sample_size, inputs, sink},

                SIMDFullRange<T, true, utr1>{sample_size, inputs, sink},
                SIMDFullRange<T, false, utr1>{sample_size, inputs, sink},
                SIMDFullRange<T, true, utr2>{sample_size, inputs, sink},
                SIMDFullRange<T, false, utr2>{sample_size, inputs, sink},

                ss1{{sample_size, inputs, sink}, 32},
                ss1{{sample_size, inputs, sink}, 64},
                ss1{{sample_size, inputs, sink}, 128},

                ss2{{sample_size, inputs, sink}, 32},
                ss2{{sample_size, inputs, sink}, 64},
                ss2{{sample_size, inputs, sink}, 128},

                ss3{{sample_size, inputs, sink}, 32},
                ss3{{sample_size, inputs, sink}, 64},
                ss3{{sample_size, inputs, sink}, 128},

                ss4{{sample_size, inputs, sink}, 32},
                ss4{{sample_size, inputs, sink}, 64},
                ss4{{sample_size, inputs, sink}, 128});
        }
    }
};

template <typelist::List Types, typename Suite, typename... Args>
void
bm_suite_types(Types types, Suite suite, Args&&... args) noexcept
{
    for_each(typelist::materialize_to_tuple(types,
                                            suite,
                                            std::forward<Args>(args)...),
             print_results);
}
} // namespace loom
