// -*- C++ -*-
/// @file Linear algebra tests

#include "property_test.hpp"

#include <base/linal.hpp>
#include <utils/tuple.hpp>
#include <utils/typeinfo.hpp>

#include <fmt/ranges.h>

#include <cstdlib>
#include <random>

using namespace std;
using namespace lucid;

namespace vec_test
{
static const constexpr size_t MaxN = 4;
using Indicies                     = make_index_sequence<MaxN - 1>;
using ArithmeticTypes              = type_sequence<int, long, float, double>;

template <size_t I, typename A, typename B>
constexpr bool
not_equal(A&& a, B&& b) noexcept
{
    return std::get<I>(std::forward<A>(a)) != std::get<I>(std::forward<B>(b));
}

template <typename A, typename B, size_t... I>
constexpr bool
assert_tuples(A&& a, B&& b, std::index_sequence<I...>) noexcept
{
    return (... || not_equal<I>(std::forward<A>(a), std::forward<B>(b)));
}

/// @brief Test lucid::Vector.
/// @tparam T Value type.
/// @tparam N Dimensionality.
template <typename T, size_t N, typename RandomEngine>
unsigned
vector_test(RandomEngine& g, const size_t num_tests) noexcept
{
    using Dist                = std::conditional_t<std::is_integral_v<T>,
                                    std::uniform_int_distribution<T>,
                                    std::uniform_real_distribution<T>>;
    using Vec                 = Vector<T, N, array>;
    const auto t_typestring   = get_typeinfo_string(T{});
    const auto vec_typestring = get_typeinfo_string(Vec{});
    const auto arr_typestring = get_typeinfo_string(Vec().data());
    Dist       dist(T{-10000}, T{10000});
    unsigned   ret = 0;

    const constexpr double threshold = is_floating_point_v<T> ? 0.01 : 0.0;
    const auto test_property_n       = [num_tests, threshold](auto&&... args)
    {
        return test_property(num_tests,
                             threshold,
                             std::forward<decltype(args)>(args)...);
    };

    constexpr auto vmaker = maker<Vec>;

    /// @test Construct @p Vector with a single scalar.
    /// @f$\mathrm{V}(a) = \mathrm{V}(a, a, a\dots)@f$
    ret += test_property_n(
        fmt::format("{}({})", vec_typestring, t_typestring),
        [&]() noexcept { return dist(g); },
        vmaker,
        [](const auto& testing, const auto& feed) noexcept
        { return any(testing != feed); });

    // array generator
    auto argen = [&]() noexcept { return generate<N>(dist, g); };

    // array assertion
    const auto arass = [](const Vec& testing, const array<T, N>& feed) noexcept
    {
        return assert_tuples(testing, feed, make_index_sequence<N>{}) &&
               !all(lucid::isfinite(testing));
    };

    /// @test Standard vector constructor.
    /// @f$\mathrm{V}(a,b,c\dots) = \mathrm{V}(a, b, c\dots)@f$
    ret +=
        test_property_n(fmt::format("{}({})", vec_typestring, arr_typestring),
                        argen,
                        tuple_maker<Vec>,
                        arass);

    /// @test Array vector constructor.
    /// @f$\mathrm{V}([a,b,c\dots]) = \mathrm{V}(a, b, c\dots)@f$
    ret +=
        test_property_n(fmt::format("{}({})", vec_typestring, arr_typestring),
                        argen,
                        vmaker,
                        arass);

    if constexpr(N > 2)
    {
        const constexpr std::size_t N1 = N - 1;
        /// @test Construct vector from scalar and smaller vector.
        /// @f$\mathrm{V}_N(a, \mathrm{V}_{N-1}(b, c\dots)) = \mathrm{V}_{N}(a,
        /// b, c\dots)@f$
        ret += test_property_n(
            fmt::format("{0}({1}, Vector<{1}, {2}>)",
                        vec_typestring,
                        t_typestring,
                        N1),
            [&]() noexcept {
                return pair(dist(g),
                            Vector<T, N1, array>(generate<N1>(dist, g)));
            },
            vmaker,
            [](const Vec& testing, const auto& feed) noexcept
            {
                const auto& [v0, vv] = feed;
                bool ret             = testing[0] != v0;
                for(size_t i = 0; i < N1; ++i) ret |= vv[i] != testing[i + 1];
                return ret || !all(lucid::isfinite(testing));
            });
    }

    auto vgen = [&]() noexcept { return Vec(argen()); };

    // vector and scalar value generator
    auto vsgen = [&]() noexcept { return pair(vgen(), dist(g)); };

    const auto assertion = [](const auto& a, const auto& b) noexcept
    {
        if constexpr(is_floating_point_v<T>)
        {
            return any(!almost_equal_vec(a, b, 200)) ||
                   !all(lucid::isfinite(a)) || !all(lucid::isfinite(b));
        }
        else
            return any(a != b);
    };

    /// @test Vector-scalar addition and subtraction.
    /// @f$\mathbf{v} + s = \mathbf{v} + s - s@f$
    ret +=
        test_property_n(fmt::format("{} +- {}", vec_typestring, t_typestring),
                        vsgen,
                        lucid::plus,
                        [&](const auto& testing, const auto feed) noexcept
                        {
                            const auto& [vec, val] = feed;
                            return assertion(testing - val, vec);
                        });

    /// @test Inplace vector-scalar addition and subtraction.
    /// @f$\mathbf{v} + s = \mathbf{v} + s - s@f$
    ret += test_property_n(
        fmt::format("{} +-= {}", vec_typestring, t_typestring),
        vsgen,
        [](Vec vec, const T& val) noexcept
        {
            vec += val;
            return vec;
        },
        [&](auto testing, const auto& feed) noexcept
        {
            auto [vec, val] = feed;
            testing -= val;
            return assertion(testing, vec);
        });

    // distribution to generate divizor that is guaranteed to be greater than
    // zero
    Dist divdist(T{1}, T{10000});

    // random sign generator
    auto signgen =
        [&, bdist = std::bernoulli_distribution(0.5)]() mutable noexcept
    { return static_cast<T>(minus_one_pow(bdist(g))); };

    // vector and divisor generator
    auto vdgen = [&]() noexcept
    { return pair(vgen(), divdist(g) * signgen()); };

    /// @test Vector-scalar multiplication and division.
    /// @f$\mathbf{v} s = \frac{\mathbf{v} s}{s}@f$
    ret +=
        test_property_n(fmt::format("{} */ {}", vec_typestring, t_typestring),
                        vdgen,
                        lucid::multiplies,
                        [&](const auto& testing, const auto& feed) noexcept
                        {
                            const auto& [vec, val] = feed;
                            return assertion(testing / val, vec);
                        });

    /// @test Inplace vector-scalar multiplication and division.
    /// @f$\mathbf{v} s = \frac{\mathbf{v} s}{s}@f$
    ret += test_property_n(
        fmt::format("{} */= {}", vec_typestring, t_typestring),
        vdgen,
        [](Vec vec, const T& val) noexcept
        {
            vec *= val;
            return vec;
        },
        [&](auto testing, const auto& feed) noexcept
        {
            const auto& [vec, val] = feed;
            testing /= val;
            return assertion(testing, vec);
        });

    // pair of vectors generator
    auto vvgen = [&]() noexcept { return pair(vgen(), vgen()); };

    /// @test Vector-vector addition and subtraction.
    /// @f$\mathbf{v}_0 + \mathbf{v}_1 = \mathbf{v}_0 + \mathbf{v}_1 -
    /// \mathbf{v}_1@f$
    ret += test_property_n(fmt::format("{0} +- {0}", vec_typestring),
                           vvgen,
                           lucid::plus,
                           [&](const auto& testing, const auto& feed) noexcept
                           {
                               const auto& [vec1, vec2] = feed;
                               return assertion(testing - vec2, vec1);
                           });

    /// @test Inplace vector-vector addition and subtraction.
    /// @f$\mathbf{v}_0 + \mathbf{v}_1 = \mathbf{v}_0 + \mathbf{v}_1 -
    /// \mathbf{v}_1@f$
    ret += test_property_n(
        fmt::format("{0} +-= {0}", vec_typestring),
        vvgen,
        [](Vec vec1, const Vec& vec2) noexcept
        {
            vec1 += vec2;
            return vec1;
        },
        [&](auto testing, const auto& feed) noexcept
        {
            auto [vec1, vec2] = feed;
            testing -= vec2;
            return assertion(testing, vec1);
        });

    /// @test Vector minmax.
    /// @f$\mathrm{min}(\mathbf{v}_0, \mathbf{v}_1) < \mathrm{max}(\mathbf{v}_0,
    /// \mathbf{v}_1)@f$
    ret += test_property_n(
        fmt::format("min({0}, {0}), max({0}, {0})", vec_typestring),
        vvgen,
        lucid::minmax,
        // [](const auto& feed) noexcept {
        //     const auto& [vec1, vec2] = feed;
        //     return pair{lucid::min(vec1, vec2), lucid::max(vec1, vec2)};
        // },
        [&](const auto& testing, const auto&) noexcept
        {
            const auto& [vmin, vmax] = testing;
            return any(vmin > vmax) || !all(lucid::isfinite(vmin)) ||
                   !all(lucid::isfinite(vmax));
        });

    // vector and vector-divizor generator
    auto vvdgen = [&]() noexcept
    { return pair(vgen(), Vec(generate<N>(divdist, g)) * signgen()); };

    /// @test Vector-vector multiplication and division.
    /// @f$\mathbf{v}_0 \mathbf{v}_1 = \frac{\mathbf{v}_0
    /// \mathbf{v}_1}{\mathbf{v}_1}@f$
    ret += test_property_n(fmt::format("{0} */ {0}", vec_typestring),
                           vvdgen,
                           lucid::multiplies,
                           [&](const auto& testing, const auto& feed) noexcept
                           {
                               const auto& [vec1, vec2] = feed;
                               return assertion(testing / vec2, vec1);
                           });

    /// @test Inplace vector-vector multiplication and division.
    /// @f$\mathbf{v}_0 \mathbf{v}_1 = \frac{\mathbf{v}_0
    /// \mathbf{v}_1}{\mathbf{v}_1}@f$
    ret += test_property_n(
        fmt::format("{0} */= {0}", vec_typestring),
        vvdgen,
        [](Vec vec1, const Vec& vec2) noexcept
        {
            vec1 *= vec2;
            return vec1;
        },
        [&](auto testing, const auto& feed) noexcept
        {
            const auto& [vec1, vec2] = feed;
            testing /= vec2;
            return assertion(testing, vec1);
        });

    if constexpr(is_floating_point_v<T>)
    {
        /// @test Dot product of the same vector is its length.
        /// @f$\mathbf{v} \cdot \mathbf{v} = \mathrm{length}(\mathbf{v})@f$
        ret += test_property_n(
            fmt::format("{}: A dot A = length A", vec_typestring),
            vgen,
            [](const Vec& feed) noexcept { return dot(feed, feed); },
            [](const T testing, const Vec& feed) noexcept
            {
                return !almost_equal(lucid::sqrt(testing), length(feed), 5) ||
                       !std::isfinite(testing);
            });

        /// @test Length of normalized vector is one.
        /// @f$\mathrm{length}(\mathrm{normalize}(\mathbf{v})) = 1@f$
        ret += test_property_n(
            fmt::format("normalize({}) = 1", vec_typestring),
            vgen,
            normalize,
            [](const Vec& testing, Vec) noexcept
            {
                return !almost_equal(length(testing), T{1}, 5) ||
                       !all(lucid::isfinite(testing));
            });

        if constexpr(N > 2)
            /// @test Cross product of two vectors is vector perpendicular
            /// to both of them.
            /// @f$(\mathbf{v}_0 \times \mathbf{v}_1) \bot \mathbf{v}_0 \bot
            /// \mathbf{v}_1@f$
            ret += test_property(
                num_tests,
                0.01,
                fmt::format("{}: C <- cross A B | A dot C = B dot C = 0",
                            vec_typestring),
                [&]() noexcept
                { return pair(normalize(vgen()), normalize(vgen())); },
                cross,
                [](const Vec& testing, const auto& feed) noexcept
                {
                    const auto& [a, b]       = feed;
                    const T              at  = dot(a, testing);
                    const T              bt  = dot(b, testing);
                    const constexpr auto ulp = static_pow<sizeof(T)>(
                        is_same_v<T, double> ? 100ul : 55u);
                    return !(almost_equal(at, T{0}, ulp) ||
                             almost_equal(bt, T{0}, ulp)) ||
                           !all(lucid::isfinite(testing));
                });
    }

    // structure binding test
    if constexpr(N == 4)
    {
        const Vec vec            = vgen();
        const auto& [x, y, z, w] = vec;
        const auto asrt = &x != &std::get<0>(vec) || &y != &std::get<1>(vec) ||
                          &z != &std::get<2>(vec) || &w != &std::get<3>(vec);

        ret += asrt;
    }

    return ret;
}

template <size_t N, typename RandomEngine, typename... Ts>
unsigned
test_t(RandomEngine& g, const size_t num_tests, type_sequence<Ts...>) noexcept
{
    return (0u + ... + vector_test<Ts, N>(g, num_tests));
}

template <typename RandomEngine, size_t... Ns>
unsigned
test_n(RandomEngine& g, const size_t num_tests, index_sequence<Ns...>) noexcept
{
    return (0u + ... + test_t<Ns + 2>(g, num_tests, ArithmeticTypes{}));
}

/// @brief Test lucid::Vector with boolean value type.
/// @tparam Vector dimensionality.
template <size_t N, typename RandomEngine>
unsigned
vector_bool_test(RandomEngine& g, const size_t num_tests) noexcept
{
    std::bernoulli_distribution dist(0.5);
    unsigned                    ret = 0;
    auto vec_typestring = get_typeinfo_string(Vector<bool, N, array>{});

    static const constexpr double threshold       = 0.0;
    auto                          test_property_n = [num_tests](auto&&... args)
    {
        return test_property(num_tests,
                             threshold,
                             std::forward<decltype(args)>(args)...);
    };

    /// @test @f$\mathrm{any}(\mathbf{v}(a,b,c\dots)) = a \vee b \vee \dots \vee
    /// c@f$
    ret += test_property_n(
        fmt::format("any({})", vec_typestring),
        [&]() noexcept { return Vector(generate<N>(dist, g)); },
        [](const Vector<bool, N, array>& feed) noexcept { return any(feed); },
        [](const bool testing, const Vector<bool, N, array>& feed) noexcept
        {
            return testing != apply([](auto... vals) { return (... || vals); },
                                    feed.data());
        });

    /// @test @f$\mathrm{all}(\mathbf{v}(a,b,c\dots)) = a \wedge b \wedge \dots
    /// \wedge c@f$
    ret += test_property_n(
        fmt::format("all({})", vec_typestring),
        [&]() noexcept { return Vector(generate<N>(dist, g)); },
        [](const Vector<bool, N, array>& feed) noexcept { return all(feed); },
        [](const bool testing, const Vector<bool, N, array>& feed) noexcept
        {
            return testing != apply([](auto... vals) { return (... && vals); },
                                    feed.data());
        });

    return ret;
}

template <typename RandomEngine, size_t... Ns>
unsigned
vector_bool_test(RandomEngine& g,
                 const size_t  num_tests,
                 index_sequence<Ns...>) noexcept
{
    return (0u + ... + vector_bool_test<Ns + 2>(g, num_tests));
}
} // namespace vec_test

namespace mat_test
{
using Indicies        = index_sequence<3ul, 4ul>;
using ArithmeticTypes = type_sequence<float, double>;
// 5% error threshold, because + and - operations are quite inaccurate
static const constexpr double threshold = 0.05;

/// @brief Test lucid::Matrix.
/// @tparam M Number of rows.
/// @tparam N Number of columns.
template <typename T, size_t M, size_t N, typename RandomEngine>
unsigned
matrix_test(RandomEngine& g, const size_t num_tests) noexcept
{
    using Mat                           = Matrix<T, M, N, array>;
    using Vec                           = Vector<T, N, array>;
    const constexpr auto t_typestring   = get_typeinfo_string(T{});
    const auto           mat_typestring = get_typeinfo_string(Mat{});
    const auto           vec_typestring = get_typeinfo_string(Vec{});
    const auto           arr_typestring = get_typeinfo_string(Mat().data());
    std::uniform_real_distribution<T> dist(T{-10000}, T{10000});
    static const constexpr auto       MN = M * N;
    auto       array_mn_gen = [&]() noexcept { return generate<MN>(dist, g); };
    const auto array_assertion =
        [](const Mat& testing, const auto& feed) noexcept
    {
        bool ret = false;
        for(size_t i = 0; i < MN; ++i) ret |= testing.at(i) != feed[i];
        return ret || !all(lucid::isfinite(flat_ref(testing)));
    };
    auto array_n_gen = [&]() noexcept { return generate<N>(dist, g); };
    auto vgen        = [&]() noexcept { return Vec(array_n_gen()); };

    const auto test_property_n = [num_tests](auto&&... args) noexcept
    {
        return test_property(num_tests,
                             threshold,
                             std::forward<decltype(args)>(args)...);
    };

    const auto assertion = [](const auto& a, const auto& b) noexcept
    {
        if constexpr(is_floating_point_v<T>)
        {
            const constexpr unsigned ULP = 5;
            return any(!almost_equal_vec(flat_ref(a), flat_ref(b), ULP)) ||
                   !all(lucid::isfinite(flat_ref(a))) ||
                   !all(lucid::isfinite(flat_ref(b)));
        }
        else
            return any(flat_ref(a) != flat_ref(b));
    };

    auto mat_gen  = [&]() noexcept { return Mat(array_mn_gen()); };
    auto mat_gen1 = [&]() noexcept { return pair{mat_gen(), dist(g)}; };
    auto mat_gen2 = [&]() noexcept { return pair{mat_gen(), mat_gen()}; };

    constexpr auto mmaker = maker<Mat>;
    constexpr auto tmaker = tuple_maker<Mat>;

    unsigned ret = 0u;

    /// @test Construct Matrix with a single scalar.
    /// @f$\mathrm{M}(a) = \mathrm{M}(a, a, a\dots)@f$
    ret += test_property_n(
        fmt::format("flat_ref({}({}))", mat_typestring, t_typestring),
        [&]() noexcept { return dist(g); },
        mmaker,
        [](const auto& testing, const auto& feed) noexcept
        {
            return any(flat_ref(testing) != feed) ||
                   !all(lucid::isfinite(flat_ref(testing)));
        });

    /// @test Standard matrix constructor.
    /// @f$\mathrm{M}(a,b,c\dots) = \mathrm{M}(a, b, c\dots)@f$
    ret +=
        test_property_n(fmt::format("{}({})", mat_typestring, arr_typestring),
                        array_mn_gen,
                        tmaker,
                        array_assertion);

    /// @test Construct Matrix with an array.
    /// @f$\mathrm{M}([a,b,c\dots]) = \mathrm{M}(a, b, c\dots)@f$
    ret +=
        test_property_n(fmt::format("{}({})", mat_typestring, arr_typestring),
                        array_mn_gen,
                        mmaker,
                        array_assertion);

    /// @test Construct Matrix from few vectors.
    /// @f$\mathrm{M}_{m,n}(\mathrm{V}_n(a_{1,1},a_{1,2},\dotsc,a_{1,n}),
    /// \mathrm{V}_n(a_{2,1},a_{2,2},\dotsc,a_{2,n}),\dotsc,
    /// \mathrm{V}_n(a_{m,1},a_{m,2},\dotsc,a_{m,n})) =
    /// \begin{pmatrix}
    /// a_{1,1} & a_{1,2} & \cdots & a_{1,n} \\
    /// a_{2,1} & a_{2,2} & \cdots & a_{2,n} \\
    /// \vdots  & \vdots  & \ddots & \vdots  \\
    /// a_{m,1} & a_{m,2} & \cdots & a_{m,n}
    /// \end{pmatrix}@f$
    ret += test_property_n(
        fmt::format("{}({})",
                    mat_typestring,
                    get_typeinfo_string(array<Vec, M>{})),
        [&]() noexcept
        {
            array<Vec, M> ret;
            for(size_t i = 0; i < M; ++i) ret[i] = vgen();
            return ret;
        },
        tmaker,
        [](const Mat& testing, const array<Vec, M>& feed) noexcept
        {
            bool ret = false;
            for(size_t i = 0; i < M; ++i) ret |= any(testing[i] != feed[i]);
            return ret || !all(lucid::isfinite(flat_ref(testing)));
        });

    array<string, M> strs;
    std::fill(strs.begin(),
              strs.end(),
              fmt::format("{}", get_typeinfo_string(Vec().data())));

    /// @test Construct Matrix with few arrays.
    /// @f$\mathrm{M}_{m,n}([a_{1,1},a_{1,2},\dotsc,a_{1,n}],
    /// [a_{2,1},a_{2,2},\dotsc,a_{2,n}],\dotsc,
    /// [a_{m,1},a_{m,2},\dotsc,a_{m,n}]) =
    /// \begin{pmatrix}
    /// a_{1,1} & a_{1,2} & \cdots & a_{1,n} \\
    /// a_{2,1} & a_{2,2} & \cdots & a_{2,n} \\
    /// \vdots  & \vdots  & \ddots & \vdots  \\
    /// a_{m,1} & a_{m,2} & \cdots & a_{m,n}
    /// \end{pmatrix}@f$
    ret += test_property_n(
        fmt::format("{}({})", mat_typestring, strs),
        [&]() noexcept
        {
            array<array<T, N>, M> ret;
            for(size_t i = 0; i < M; ++i) ret[i] = array_n_gen();
            return ret;
        },
        tmaker,
        [](const Mat& testing, const array<array<T, N>, M>& feed) noexcept
        {
            bool ret = false;
            for(size_t i = 0; i < M; ++i)
                for(size_t j = 0; j < N; ++j)
                    ret |= testing[i][j] != feed[i][j];
            return ret || !all(lucid::isfinite(flat_ref(testing)));
        });

    if constexpr(M > 2 && N > 2)
    {
        const constexpr std::size_t M1  = M - 1;
        const constexpr std::size_t N1  = N - 1;
        const constexpr std::size_t MN1 = M1 * N1;
        using sMat                      = Matrix<T, M1, N1, array>;
        using sVec                      = Vector<T, N1, array>;
        auto smat_gen = [&]() noexcept { return sMat(generate<MN1>(dist, g)); };
        auto svecgen  = [&]() noexcept { return sVec(generate<N1>(dist, g)); };

        /// @test Construct Matrix with a smaller matrix.
        /// @f$\mathrm{M}_{m,n}\left(
        /// \mathrm{M}_{m-1,n-1}
        /// \begin{pmatrix}
        /// a_{1,1} & \cdots & a_{1,n-1} \\
        /// \vdots & \ddots & \vdots \\
        /// a_{m-1,1} & \cdots & a_{m-1,n-1}
        /// \end{pmatrix} \right) =
        /// \begin{pmatrix}
        /// a_{1,1} & a_{1,2} & \cdots & a_{1,n-1} & \cdots & 0 \\
        /// \vdots  & \vdots  & \vdots & \vdots & \ddots & \vdots \\
        /// a_{m-1,1} & a_{m-1,2} & \cdots & a_{m-1,n-1} & \cdots & 0 \\
        /// \vdots  & \vdots  & \vdots & \vdots & \ddots & \vdots \\
        /// 0 & 0 & \cdots & 0 & \cdots & 0
        /// \end{pmatrix}@f$
        ret += test_property_n(
            fmt::format("{}(Matrix<{}, {}, {}>)",
                        mat_typestring,
                        t_typestring,
                        M1,
                        N1),
            smat_gen,
            mmaker,
            [](const Mat& testing, const sMat& feed) noexcept
            {
                bool ret = false;
                for(size_t i = 0; i < M1; ++i)
                    for(size_t j = 0; j < N1; ++j)
                        ret |= feed.at(i, j) != testing.at(i, j);
                return ret || !all(lucid::isfinite(flat_ref(testing)));
            });

        /// @test Construct Matrix from a scalar, a vector and a smaller matrix.
        /// @f$\mathrm{M}_{m,n}\left(a, \mathrm{V}_{n-1}(b_1,\cdots,b_{n-1}),
        /// \mathrm{M}_{m-1,n-1}
        /// \begin{pmatrix}
        /// c_{1,1} & \cdots & c_{1,n-1} \\
        /// \vdots & \ddots & \vdots \\
        /// c_{m-1,1} & \cdots & c_{k,n-1}
        /// \end{pmatrix} \right) =
        /// \begin{pmatrix}
        /// a & b_1 & \cdots & b_{n-1} \\
        /// c_{1,1} & \cdots & c_{1,n-1} & 0 \\
        /// \vdots  & \ddots & \vdots & \vdots \\
        /// c_{m-1,1} & \cdots & c_{m-1,n-1} & 0
        /// \end{pmatrix}@f$
        ret += test_property_n(
            fmt::format("{0}({1}, Vector<{1}, {2}>, Matrix<{1}, {3}, {2}>)",
                        mat_typestring,
                        t_typestring,
                        N1,
                        M1),
            [&]() noexcept { return tuple(dist(g), svecgen(), smat_gen()); },
            mmaker,
            [](const Mat& testing, const auto& feed) noexcept
            {
                const auto& [scalar, vec, mat] = feed;
                bool ret                       = testing.at(0) != scalar;
                for(size_t i = 0; i < N1; ++i)
                    ret |= vec[i] != testing[0][i + 1];
                for(size_t i = 0; i < M1; ++i)
                    for(size_t j = 0; j < N1; ++j)
                        ret |= mat.at(i, j) != testing.at(i + 1, j);
                return ret;
            });

        /// @test Matrix inversion.
        /// @f$\mathbf{A}^{-1}\mathbf{A} = \mathbf{I}_n@f$
        if constexpr(M == N && is_floating_point_v<T>)
            ret += test_property_n(
                fmt::format("{0}: inverse(A) * A = {0}::identity()",
                            mat_typestring),
                [&]() noexcept { return mat_gen(); },
                inverse,
                [](const Mat& testing, const Mat& feed) noexcept
                {
                    // identity matrix elements have different magnitude
                    // equalizing them as ulp is same for all of them
                    const Mat zero = Mat::identity() - feed * testing;
                    const constexpr size_t ulp = static_pow<sizeof(T)>(100ul);
                    return any(!almost_equal_vec(flat_ref(zero), T{0}, ulp)) ||
                           !all(lucid::isfinite(flat_ref(testing)));
                });
    }

    /// @test Matrix-scalar addition and subtraction.
    /// @f$\mathbf{A} + b = \mathbf{A} + b - b@f$
    ret +=
        test_property_n(fmt::format("{} +- {}", mat_typestring, t_typestring),
                        mat_gen1,
                        lucid::plus,
                        [&](const Mat& testing, const auto& feed) noexcept
                        {
                            const auto& [mat, scalar] = feed;
                            const auto mm             = testing - scalar;
                            return assertion(mm, mat);
                        });

    /// @test Inplace matrix-scalar addition and subtraction.
    /// @f$\mathbf{A} + b = \mathbf{A} + b - b@f$
    ret += test_property_n(
        fmt::format("{} +-= {}", mat_typestring, t_typestring),
        mat_gen1,
        [](Mat mat, const T& scalar) noexcept
        {
            mat += scalar;
            return mat;
        },
        [&](Mat testing, const auto& feed) noexcept
        {
            const auto& [mat, scalar] = feed;
            testing -= scalar;
            return assertion(testing, mat);
        });

    /// @test Matrix-matrix addition and subtraction.
    /// @f$\mathbf{A} + \mathbf{B} = \mathbf{A} + \mathbf{B} - \mathbf{B}@f$
    ret += test_property_n(fmt::format("{0} +- {0}", mat_typestring),
                           mat_gen2,
                           lucid::plus,
                           [&](const Mat& testing, const auto& feed) noexcept
                           {
                               const auto& [mat1, mat2] = feed;
                               const auto mm            = testing - mat2;
                               return assertion(mm, mat1);
                           });

    /// @test Inplace matrix-matrix addition and subtraction.
    /// @f$\mathbf{A} + \mathbf{B} = \mathbf{A} + \mathbf{B} - \mathbf{B}@f$
    ret += test_property_n(
        fmt::format("{0} +-= {0}", mat_typestring),
        mat_gen2,
        [](Mat mat1, const Mat& mat2) noexcept
        {
            mat1 += mat2;
            return mat1;
        },
        [&](Mat testing, const auto& feed) noexcept
        {
            const auto& [mat1, mat2] = feed;
            testing -= mat2;
            return assertion(testing, mat1);
        });

    // distribution to generate divizor that is guaranteed to be greater than
    // zero
    std::uniform_real_distribution<T> divdist(T{1}, T{10000});

    auto sign_gen =
        [&, bdist = std::bernoulli_distribution(0.5)]() mutable noexcept
    { return static_cast<T>(minus_one_pow(bdist(g))); };

    auto div_vec_gen = [&]() noexcept
    { return Vec(generate<N>(divdist, g)) * sign_gen(); };

    std::uniform_int_distribution<size_t> rowdist(0, M - 1);

    /// @test Assignment to Matrix row from a vector.
    /// @f$\mathbf{M}_{m,n}[i] \gets \mathbf{V}_n(a_1,\cdots,a_n) =
    /// \begin{pmatrix}
    /// \cdots & \cdots & \cdots \\
    /// a_{i,1} & \cdots & a_{i,n} \\
    /// \cdots & \cdots & \cdots
    /// \end{pmatrix}@f$
    ret += test_property_n(
        fmt::format("{}[] = Vector<{}, {}>", mat_typestring, t_typestring, N),
        [&]() noexcept { return tuple(mat_gen(), vgen(), rowdist(g)); },
        [](Mat mat, const Vec& vec, const std::size_t idx) noexcept
        {
            mat[idx] = vec;
            return mat;
        },
        [](const Mat& testing, const auto& feed) noexcept
        {
            const auto& vec = get<1>(feed);
            const auto& idx = get<2>(feed);
            return any(testing[idx] != vec) ||
                   !all(lucid::isfinite(flat_ref(testing)));
        });

    /// @test Matrix row assignment-addition-subtraction
    /// @f$\mathbf{M}_{m,n}[i] \gets \mathbf{M}_{m,n}[i] \pm
    /// \mathbf{V}_n(a_1,\cdots,a_n)@f$
    ret += test_property_n(
        fmt::format("{}[] +-= {}", mat_typestring, vec_typestring),
        [&]() noexcept { return tuple(mat_gen(), vgen(), rowdist(g)); },
        [](Mat mat, const Vec& vec, const std::size_t idx) noexcept
        {
            mat[idx] += vec;
            return mat;
        },
        [&](Mat testing, const auto& feed) noexcept
        {
            const auto& [mat, vec, idx] = feed;
            testing[idx] -= vec;
            return assertion(testing, mat);
        });

    /// @test Matrix row assignment-multuply-subtraction
    /// @f$\mathbf{M}_{m,n}[i] \gets \mathbf{M}_{m,n}[i] \ast\div
    /// \mathbf{V}_n(a_1,\cdots,a_n)@f$
    ret += test_property_n(
        fmt::format("{}[] */= {}", mat_typestring, vec_typestring),
        [&]() noexcept { return tuple(mat_gen(), div_vec_gen(), rowdist(g)); },
        [](Mat mat, const Vec& vec, const std::size_t idx) noexcept
        {
            mat[idx] *= vec;
            return mat;
        },
        [&](Mat testing, const auto& feed) noexcept
        {
            const auto& [mat, vec, idx] = feed;
            testing[idx] /= vec;
            return assertion(testing, mat);
        });

    /// @test Matrix transposition and multiplication.
    /// @f$(\mathbf{AB})^T = \mathbf{B}^T\mathbf{A}@f$
    ret += test_property_n(
        fmt::format("{}: transpose(A dot B) = transpose(B) dot transpose(A)",
                    mat_typestring),
        [&]() noexcept
        {
            return pair(Mat(generate<MN>(divdist, g)) * sign_gen(),
                        Matrix<T, N, M, array>(generate<MN>(divdist, g)) *
                            sign_gen());
        },
        transpose | dot,
        [&](const auto& testing, const auto& feed) noexcept
        {
            const auto& ab_t   = testing;
            const auto& [a, b] = feed;
            const auto bt_at   = transpose(b) * transpose(a);
            return assertion(ab_t, bt_at);
        });

    if constexpr(M == 4)
    {
        const auto mat           = mat_gen();
        const auto& [a, b, c, d] = mat;
        const auto asrt          = &std::get<0>(a) != &mat.at(0, 0) ||
                          &std::get<0>(b) != &mat.at(1, 0) ||
                          &std::get<0>(c) != &mat.at(2, 0) ||
                          &std::get<0>(d) != &mat.at(3, 0);

        ret += asrt;
    }

    return ret;
}

template <size_t M, size_t N, typename RandomEngine, typename... Ts>
unsigned
test_t(RandomEngine& g, const size_t num_tests, type_sequence<Ts...>) noexcept
{
    return (0u + ... + matrix_test<Ts, M, N>(g, num_tests));
}

template <size_t N, typename RandomEngine, size_t... Ms>
unsigned
test_m(RandomEngine& g, const size_t num_tests, index_sequence<Ms...>) noexcept
{
    return (0u + ... + test_t<Ms, N>(g, num_tests, ArithmeticTypes{}));
}

template <typename RandomEngine, size_t... Ns>
unsigned
test_n(RandomEngine& g, const size_t num_tests, index_sequence<Ns...>) noexcept
{
    return (0u + ... + test_m<Ns>(g, num_tests, Indicies{}));
}
} // namespace mat_test

int
main(int argc, char* argv[])
{
    const size_t num_tests = argc == 2 ? atoi(argv[1]) : 1000000;

    int ret = 0;

    random_device         rd;
    default_random_engine g(rd());

    ret += vec_test::test_n(g, num_tests, vec_test::Indicies{});
    ret += vec_test::vector_bool_test(g, num_tests, vec_test::Indicies{});
    ret += mat_test::test_n(g, num_tests, mat_test::Indicies{});

    if(ret)
        fmt::print("{} tests failed.\n", ret);
    else
        fmt::print("All tests passed.\n");

    return ret;
}
