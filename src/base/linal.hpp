// -*- C++ -*-
/// @file linal.hpp
/// @brief Linar algebra classes and operators.
/// @note Functions defined in lucid::fn namespace are not for general use.
/// They provide implementations for functional objects with same names
/// in lucid namespace.

#pragma once

#include <utils/debug.hpp>
#include <utils/functional.hpp>
#include <utils/math.hpp>
#include <utils/tmp.hpp>

#include <array>
#include <functional>
#include <limits>
#include <numeric>
#include <span>
#include <type_traits>
#include <utility>

/// @brief Generate code for arithmetic binary operators of lucid::Vector and
/// lucid::Matrix.
#define DEF_BINARY_OP(FUNC, OP)                                           \
    namespace fn                                                          \
    {                                                                     \
    struct FUNC##_fn                                                      \
    {                                                                     \
        template <typename T1,                                            \
                  std::size_t N,                                          \
                  template <typename, std::size_t>                        \
                  typename Container,                                     \
                  typename T2>                                            \
        constexpr auto                                                    \
        operator()(const Vector<T1, N, Container>& lhs,                   \
                   const T2&                       rhs) const noexcept                          \
        {                                                                 \
            return transform(std::FUNC<T1>{}, lhs, rhs);                  \
        }                                                                 \
        template <typename T,                                             \
                  std::size_t M,                                          \
                  std::size_t N,                                          \
                  template <typename, std::size_t>                        \
                  typename Container1,                                    \
                  template <typename, std::size_t>                        \
                  typename Container2>                                    \
        constexpr Matrix<T, M, N, std::array>                             \
        operator()(const Matrix<T, M, N, Container1>& lhs,                \
                   const Matrix<T, M, N, Container2>& rhs) const noexcept \
        {                                                                 \
            constexpr std::FUNC<T>      op{};                             \
            Matrix<T, M, N, std::array> ret{};                            \
            for(std::size_t i = 0; i < (M * N); ++i)                      \
                ret.at(i) = op(lhs.at(i), rhs.at(i));                     \
            return ret;                                                   \
        }                                                                 \
        template <typename T,                                             \
                  std::size_t M,                                          \
                  std::size_t N,                                          \
                  template <typename, std::size_t>                        \
                  typename Container>                                     \
        constexpr Matrix<T, M, N, std::array>                             \
        operator()(const Matrix<T, M, N, Container>& lhs,                 \
                   const T&                          rhs) const noexcept                           \
        {                                                                 \
            constexpr std::FUNC<T>      op{};                             \
            Matrix<T, M, N, std::array> ret{};                            \
            for(std::size_t i = 0; i < (M * N); ++i)                      \
                ret.at(i) = op(lhs.at(i), rhs);                           \
            return ret;                                                   \
        }                                                                 \
        COMPOSE_OP                                                        \
    };                                                                    \
    }                                                                     \
    static constexpr fn::FUNC##_fn FUNC{};                                \
    template <typename T1,                                                \
              std::size_t N,                                              \
              template <typename, std::size_t>                            \
              typename Container,                                         \
              typename T2>                                                \
    constexpr auto OP(const Vector<T1, N, Container>& lhs,                \
                      const T2&                       rhs) noexcept                             \
    {                                                                     \
        return FUNC(lhs, rhs);                                            \
    }                                                                     \
    template <typename T1,                                                \
              std::size_t M,                                              \
              std::size_t N,                                              \
              template <typename, std::size_t>                            \
              typename Container,                                         \
              typename T2>                                                \
    constexpr auto OP(const Matrix<T1, M, N, Container>& lhs,             \
                      const T2&                          rhs) noexcept                             \
    {                                                                     \
        return FUNC(lhs, rhs);                                            \
    }

/// @brief Generate code for unary operators of lucid::Vector and lucid::Matrix.
#define DEF_UNARY_OP(FUNC, OP)                                           \
    namespace fn                                                         \
    {                                                                    \
    struct FUNC##_fn                                                     \
    {                                                                    \
        template <typename T,                                            \
                  std::size_t N,                                         \
                  template <typename, std::size_t>                       \
                  typename Container>                                    \
        constexpr Vector<T, N, std::array>                               \
        operator()(const Vector<T, N, Container>& lhs) const noexcept    \
        {                                                                \
            return transform(std::FUNC<T>(), lhs);                       \
        }                                                                \
        template <typename T,                                            \
                  std::size_t M,                                         \
                  std::size_t N,                                         \
                  template <typename, std::size_t>                       \
                  typename Container>                                    \
        constexpr Matrix<T, M, N, std::array>                            \
        operator()(const Matrix<T, M, N, Container>& lhs) const noexcept \
        {                                                                \
            return Matrix<T, M, N, std::array>(                          \
                transform(std::FUNC<T>(), flat_ref(lhs)));               \
        }                                                                \
        COMPOSE_OP                                                       \
    };                                                                   \
    }                                                                    \
    inline constexpr fn::FUNC##_fn FUNC{};                               \
    template <typename T,                                                \
              std::size_t N,                                             \
              template <typename, std::size_t>                           \
              typename Container>                                        \
    constexpr Vector<T, N, std::array> OP(                               \
        const Vector<T, N, Container>& lhs) noexcept                     \
    {                                                                    \
        return FUNC(lhs);                                                \
    }                                                                    \
    template <typename T,                                                \
              std::size_t M,                                             \
              std::size_t N,                                             \
              template <typename, std::size_t>                           \
              typename Container>                                        \
    constexpr Matrix<T, M, N, std::array> OP(                            \
        const Matrix<T, M, N, Container>& lhs) noexcept                  \
    {                                                                    \
        return FUNC(lhs);                                                \
    }

/// @brief Generate code for lucid::Vector'ised analogue of @p FUNC.
#define VEC_NAMED_FN_OBJ(NAME, FUNC)                   \
    namespace fn                                       \
    {                                                  \
    struct NAME##_fn                                   \
    {                                                  \
        template <typename T,                          \
                  std::size_t N,                       \
                  template <typename, std::size_t>     \
                  typename Container,                  \
                  typename... Args>                    \
        constexpr decltype(auto)                       \
        operator()(const Vector<T, N, Container>& v,   \
                   const Args&... args) const noexcept \
        {                                              \
            return transform(*this, v, args...);       \
        }                                              \
        template <typename... Args>                    \
        constexpr decltype(auto)                       \
        operator()(const Args&... args) const noexcept \
        {                                              \
            return FUNC(args...);                      \
        }                                              \
        COMPOSE_OP                                     \
    };                                                 \
    }                                                  \
    inline constexpr fn::NAME##_fn NAME{};

/// @brief Same as VEC_NAMED_FN_OBJ, but @p FUNC as the @p NAME.
#define VEC_FN_OBJ(FUNC) VEC_NAMED_FN_OBJ(FUNC, FUNC)

/// @brief Same as VEC_FN_OBJ, but when
/// static_math analogue of @p FUNC is also available.
#define VEC_NAMED_FN_OBJ_SMATH(FUNC)                   \
    namespace fn                                       \
    {                                                  \
    struct FUNC##_fn                                   \
    {                                                  \
        template <typename T,                          \
                  std::size_t N,                       \
                  template <typename, std::size_t>     \
                  typename Container,                  \
                  typename... Args>                    \
        constexpr decltype(auto)                       \
        operator()(const Vector<T, N, Container>& v,   \
                   const Args&... args) const noexcept \
        {                                              \
            return transform(*this, v, args...);       \
        }                                              \
        template <typename... Args>                    \
        constexpr decltype(auto)                       \
        operator()(const Args&... args) const noexcept \
        {                                              \
            if(std::is_constant_evaluated())           \
                return smath::FUNC(args...);           \
            else                                       \
                return std::FUNC(args...);             \
        }                                              \
        COMPOSE_OP                                     \
    };                                                 \
    }                                                  \
    inline constexpr fn::FUNC##_fn FUNC{};

namespace lucid
{
/// @defgroup LINAL Linear Algebra
/// @brief Linear algebra classes and operators.
/// @note Functions defined in lucid::fn namespace are not for general use.
/// They provide implementations for functional objects with same names
/// in lucid namespace.
/// @{

/// @brief N-dimensional euclidian vector.
/// @tparam T value type. Must be arithmetic.
/// @tparam N dimensionality.
/// @tparam Container std::array-like container that holds the data.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
    requires(std::is_arithmetic_v<T>)
class Vector
{
    using Data = Container<T, N>;

    Data m_data{};

    template <std::size_t idx>
    constexpr void
    unpack()
    {
        if constexpr(idx < N)
            for(auto i = idx; i < N; ++i) m_data[i] = static_cast<T>(0);
    }

    template <std::size_t idx, typename... Ts>
    constexpr void
    unpack(const T& first, const Ts&... other)
        requires(idx < N)
    {
        m_data[idx] = first;
        if constexpr(sizeof...(other) == 0 && idx < N - 1)
            unpack<idx + 1>(first);
        else
            unpack<idx + 1>(other...);
    }

    template <std::size_t idx,
              typename T1,
              std::size_t N1,
              template <typename, std::size_t>
              typename Container1,
              typename... Ts>
    constexpr void
    unpack(const Vector<T1, N1, Container1>& first, const Ts&... other)
        requires(idx < N)
    {
        for(std::size_t i = 0; i < std::min(N - idx, N1); ++i)
            m_data[idx + i] = static_cast<T>(first[i]);
        unpack<idx + N1>(other...);
    }

    template <std::size_t idx, typename T1, std::size_t N1, typename... Ts>
    constexpr void
    unpack(const Container<T1, N1>& first, const Ts&... other)
        requires(idx < N)
    {
        for(std::size_t i = 0; i < std::min(N - idx, N1); ++i)
            m_data[idx + i] = static_cast<T>(first[i]);
        unpack<idx + N1>(other...);
    }

  public:
    constexpr Vector() noexcept {}

    constexpr Vector(const Vector& rhs) noexcept : m_data(rhs.m_data) {}

    /// @brief Construct Vector from an array.
    explicit constexpr Vector(Data&& rhs) noexcept : m_data(std::move(rhs)) {}

    explicit constexpr Vector(const Data& rhs) noexcept : m_data(rhs) {}

    explicit constexpr Vector(Data& rhs) noexcept : m_data(rhs) {}

    /// @brief Construct Vector with a single scalar.
    /// @f$\mathrm{V}(a) = \mathrm{V}(a, a, a\dots)@f$
    explicit constexpr Vector(T&& rhs) noexcept
    {
        for(T& i: m_data) i = rhs;
    }

    /// @brief Universal constructor.
    /// Constructs Vector from scalars, other vectors or arrays.
    ///
    /// Examples:
    ///
    /// From scalars:
    /// @f$\mathrm{V}(a,b,c\dots) = \mathrm{V}(a, b, c\dots)@f$
    ///
    /// From an array:
    /// @f$\mathrm{V}([a,b,c\dots]) = \mathrm{V}(a, b, c\dots)@f$
    ///
    /// From a scalar and a smaller vector.
    /// @f$\mathrm{V}_N(a, \mathrm{V}_{N-1}(b, c\dots)) = \mathrm{V}_{N}(a,
    /// b, c\dots)@f$
    template <typename... Ts>
    explicit constexpr Vector(const Ts&... rhs) noexcept
    {
        unpack<0>(rhs...);
    }

    constexpr Vector&
    operator=(const Vector& rhs) noexcept
    {
        m_data = rhs.m_data;
        return *this;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Vector&
    operator=(const Vector<T, N, Container2>& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] = rhs[i];
        return *this;
    }

    constexpr Vector&
    operator=(const T& rhs) noexcept
    {
        for(T& i: m_data) i = rhs;
        return *this;
    }

    constexpr auto
    begin() const noexcept
    {
        return m_data.begin();
    }

    constexpr auto
    end() const noexcept
    {
        return m_data.end();
    }

    constexpr auto
    cbegin() const noexcept
    {
        return m_data.cbegin();
    }

    constexpr auto
    cend() const noexcept
    {
        return m_data.cend();
    }

    constexpr const T&
    operator[](const std::size_t i) const noexcept
    {
        CHECK_INDEX(i, N);
        return m_data[i];
    }

    constexpr T&
    operator[](const std::size_t i) noexcept
    {
        CHECK_INDEX(i, N);
        return m_data[i];
    }

    template <std::size_t I>
    constexpr T&
    get() noexcept
    {
        return m_data[I];
    }

    template <std::size_t I>
    constexpr const T&
    get() const noexcept
    {
        return m_data[I];
    }

    constexpr const T&
    at(const std::size_t i) const noexcept
    {
        CHECK_INDEX(i, N);
        return m_data[i];
    }

    constexpr T&
    at(const std::size_t i) noexcept
    {
        CHECK_INDEX(i, N);
        return m_data[i];
    }

    /// @brief Access the internal container.
    constexpr const Data&
    data() const noexcept
    {
        return m_data;
    }

    /// @brief Access the internal container.
    constexpr Data&
    data() noexcept
    {
        return m_data;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Vector&
    operator+=(const Vector<T, N, Container2>& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] += rhs[i];

        return *this;
    }

    constexpr Vector&
    operator+=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] += rhs;

        return *this;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Vector&
    operator-=(const Vector<T, N, Container2>& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] -= rhs[i];

        return *this;
    }

    constexpr Vector&
    operator-=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] -= rhs;

        return *this;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Vector&
    operator*=(const Vector<T, N, Container2>& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] *= rhs[i];

        return *this;
    }

    constexpr Vector&
    operator*=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] *= rhs;

        return *this;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Vector&
    operator/=(const Vector<T, N, Container2>& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] /= rhs[i];

        return *this;
    }

    constexpr Vector&
    operator/=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < N; ++i) m_data[i] /= rhs;

        return *this;
    }

    constexpr std::size_t
    size() const noexcept
    {
        return N;
    }
};

/// @brief Template deduction guide for Vector.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
Vector(Container<T, N>&&) -> Vector<T, N, Container>;

/// @brief Row-major matrix of size @f$M\times N@f$.
/// @tparam T value type. Must be arithmetic.
/// @tparam M number of rows.
/// @tparam N number of columns.
/// @tparam Container std::array-like container that used to store the data.
template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
    requires(M > 1 && N > 1 && std::is_arithmetic_v<T>)
class Matrix
{
    static constexpr std::size_t MxN = M * N;

    using Data = Container<T, MxN>;
    Data m_data{};

    static constexpr std::size_t
    pos(const std::size_t i,
        const std::size_t j,
        const std::size_t J = N) noexcept
    {
        return i * J + j;
    }

    static constexpr std::pair<std::size_t, std::size_t>
    indices(const std::size_t pos) noexcept
    {
        return std::pair(pos / N, pos % N);
    }

    // variyng template sink
    template <std::size_t>
    constexpr void
    unpack() const noexcept
    {
    }

    template <std::size_t idx, typename... Ts>
    constexpr void
    unpack(const T& head, Ts&&... tail) noexcept
        requires(idx < MxN)
    {
        m_data[idx] = head;

        if constexpr(sizeof...(tail) == 0 && idx < MxN - 1)
            unpack<idx + 1>(head);
        else
            unpack<idx + 1>(std::forward<Ts>(tail)...);
    }

    // unpack vector arguments
    template <std::size_t idx,
              std::size_t MxN1,
              template <typename, std::size_t>
              typename Container2,
              typename... Ts>
    constexpr void
    unpack(const Vector<T, MxN1, Container2>& head, Ts&&... tail) noexcept
        requires(idx < MxN)
    {
        const constexpr auto end = std::min(MxN - idx, MxN1);

        for(std::size_t i = 0; i < end; ++i) m_data[idx + i] = head[i];

        unpack<idx + end>(std::forward<Ts>(tail)...);
    }

    template <std::size_t idx, std::size_t MxN1, typename... Ts>
    constexpr void
    unpack(const Container<T, MxN1>& head, Ts&&... tail) noexcept
        requires(idx < MxN)
    {
        const constexpr auto end = std::min(MxN - idx, MxN1);
        for(std::size_t i = 0; i < end; ++i) m_data[idx + i] = head[i];
        unpack<idx + end>(std::forward<Ts>(tail)...);
    }

    template <std::size_t idx,
              std::size_t M1,
              std::size_t N1,
              template <typename, std::size_t>
              typename Container1,
              typename... Ts>
    constexpr void
    unpack(const Matrix<T, M1, N1, Container1>& head, Ts&&... tail) noexcept
    {
        constexpr auto shift   = indices(idx);
        constexpr auto shift_i = shift.first;
        constexpr auto shift_j = shift.second;
        constexpr auto I       = std::min(M - shift_i, M1);
        constexpr auto J       = std::min(N - shift_j, N1);

        for(std::size_t i = 0; i < I; ++i)
            for(std::size_t j = 0; j < J; ++j)
                m_data[pos(shift_i + i, shift_j + j)] = head.at(i, j);

        unpack<pos(I, J, J) + 1>(std::forward<Ts>(tail)...);
    }

  public:
    constexpr Matrix() noexcept {}

    constexpr Matrix(const Matrix& rhs) noexcept : m_data(rhs.m_data) {}

    constexpr Matrix(Matrix&& rhs) noexcept : m_data(std::move(rhs.m_data)) {}

    /// @brief Construct Matrix with a scalar.
    ///
    /// @f$\mathrm{M}(a) = \mathrm{M}(a, a, a\dots)@f$
    explicit constexpr Matrix(T&& rhs) noexcept
    {
        for(auto& elem: m_data) elem = rhs;
    }

    /// @brief Construct Matrix with an array.
    explicit constexpr Matrix(Data&& rhs) noexcept : m_data(std::move(rhs)) {}

    explicit constexpr Matrix(Data& rhs) noexcept : m_data(rhs) {}

    explicit constexpr Matrix(const Data& rhs) noexcept : m_data(rhs) {}

    /// @brief Universal constructor.
    /// Constructs Matrix from scalars, vectors, arrays or other matrices.
    ///
    /// From scalars:
    /// @f$\mathrm{M}(a,b,c\dots) = \mathrm{M}(a, b, c\dots)@f$
    ///
    /// From an array:
    /// @f$\mathrm{M}([a,b,c\dots]) = \mathrm{M}(a, b, c\dots)@f$
    ///
    /// From vectors:
    /// @f$\mathrm{M}_{m,n}(\mathrm{V}_n(a_{1,1},a_{1,2},\dotsc,a_{1,n}),
    /// \mathrm{V}_n(a_{2,1},a_{2,2},\dotsc,a_{2,n}),\dotsc,
    /// \mathrm{V}_n(a_{m,1},a_{m,2},\dotsc,a_{m,n})) =
    /// \begin{pmatrix}
    /// a_{1,1} & a_{1,2} & \cdots & a_{1,n} \\
    /// a_{2,1} & a_{2,2} & \cdots & a_{2,n} \\
    /// \vdots  & \vdots  & \ddots & \vdots  \\
    /// a_{m,1} & a_{m,2} & \cdots & a_{m,n}
    /// \end{pmatrix}@f$
    ///
    /// From arrays:
    /// @f$\mathrm{M}_{m,n}([a_{1,1},a_{1,2},\dotsc,a_{1,n}],
    /// [a_{2,1},a_{2,2},\dotsc,a_{2,n}],\dotsc,
    /// [a_{m,1},a_{m,2},\dotsc,a_{m,n}]) =
    /// \begin{pmatrix}
    /// a_{1,1} & a_{1,2} & \cdots & a_{1,n} \\
    /// a_{2,1} & a_{2,2} & \cdots & a_{2,n} \\
    /// \vdots  & \vdots  & \ddots & \vdots  \\
    /// a_{m,1} & a_{m,2} & \cdots & a_{m,n}
    /// \end{pmatrix}@f$
    ///
    /// From a smaller matrix:
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
    template <typename... Ts>
    explicit constexpr Matrix(Ts&&... rhs) noexcept
    {
        unpack<0>(std::forward<Ts>(rhs)...);
    }

    constexpr Matrix&
    operator=(const Matrix& rhs) noexcept
    {
        m_data = rhs.m_data;
        return *this;
    }

    constexpr Matrix&
    operator=(Matrix&& rhs) noexcept
    {
        m_data = std::move(rhs.m_data);
        return *this;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Matrix&
    operator=(const Matrix<T, M, N, Container2>& rhs) noexcept
    {
        std::copy(rhs.cbegin(), rhs.cend(), begin());
        return *this;
    }

    constexpr Matrix&
    operator=(const T& rhs) noexcept
    {
        for(T& i: m_data) i = rhs;
        return *this;
    }

    constexpr Vector<const T, N, std::span>
    operator[](const std::size_t i) const noexcept
    {
        CHECK_INDEX(i, M);
        return Vector(std::span<const T, N>(&at(i, 0), N));
    }

    constexpr Vector<T, N, std::span>
    operator[](const std::size_t i) noexcept
    {
        CHECK_INDEX(i, M);
        return Vector(std::span<T, N>(&at(i, 0), N));
    }

    template <std::size_t I>
    constexpr Vector<const T, N, std::span>
    get() const noexcept
    {
        return Vector(std::span<const T, N>(&at(I, 0), N));
    }

    template <std::size_t I>
    constexpr Vector<T, N, std::span>
    get() noexcept
    {
        return Vector(std::span<T, N>(&at(I, 0), N));
    }

    template <std::size_t I, std::size_t J>
    constexpr const T&
    get() const noexcept
        requires(I < M || J < N)
    {
        return std::get<pos(I, J)>(m_data);
    }

    template <std::size_t I, std::size_t J>
    constexpr T&
    get() noexcept
        requires(I < M || J < N)
    {
        return std::get<pos(I, J)>(m_data);
    }

    const constexpr T&
    at(const std::size_t i) const noexcept
    {
        CHECK_INDEX(i, MxN);
        return m_data[i];
    }

    constexpr T&
    at(const std::size_t i) noexcept
    {
        CHECK_INDEX(i, MxN);
        return m_data[i];
    }

    const constexpr T&
    at(const std::size_t i, const std::size_t j) const noexcept
    {
        CHECK_INDEX(i, M);
        CHECK_INDEX(j, N);
        return m_data[pos(i, j)];
    }

    constexpr T&
    at(const std::size_t i, const std::size_t j) noexcept
    {
        CHECK_INDEX(i, M);
        CHECK_INDEX(j, N);
        return m_data[pos(i, j)];
    }

    constexpr auto
    begin() const noexcept
    {
        return m_data.begin();
    }

    constexpr auto
    end() const noexcept
    {
        return m_data.end();
    }

    constexpr decltype(auto)
    cbegin() const noexcept
    {
        return m_data.cbegin();
    }

    constexpr decltype(auto)
    cend() const noexcept
    {
        return m_data.cend();
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Matrix&
    operator+=(const Matrix<T, M, N, Container2>& rhs)
    {
        for(std::size_t i = 0; i < MxN; ++i) at(i) += rhs.at(i);

        return *this;
    }

    constexpr Matrix&
    operator+=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < MxN; ++i) at(i) += rhs;

        return *this;
    }

    template <template <typename, std::size_t> typename Container2>
    constexpr Matrix&
    operator-=(const Matrix<T, M, N, Container2>& rhs)
    {
        for(std::size_t i = 0; i < MxN; ++i) at(i) -= rhs.at(i);

        return *this;
    }

    constexpr Matrix&
    operator-=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < MxN; ++i) at(i) -= rhs;

        return *this;
    }

    constexpr Matrix&
    operator*=(const T& rhs) noexcept
    {
        for(std::size_t i = 0; i < MxN; ++i) at(i) *= rhs;

        return *this;
    }

    /// @brief Access the internal container.
    constexpr const Data&
    data() const noexcept
    {
        return m_data;
    }

    /// @brief Access the internal container.
    constexpr Data&
    data() noexcept
    {
        return m_data;
    }

    /// @brief Create identity matrix.
    static constexpr Matrix
    identity()
    {
        Matrix ret{};

        for(std::size_t i = 0; i < M; ++i)
            for(std::size_t j = 0; j < N; ++j) ret.at(i, j) = T(i == j);

        return ret;
    }
};

/// @}

namespace detail
{
template <typename T>
struct vector_val_type
{
    using type = T;
};

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
struct vector_val_type<Vector<T, N, Container>>
{
    using type = T;
};

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr const T&
elem(const Vector<T, N, Container>& v, const std::size_t i) noexcept
{
    return v[i];
}

template <typename T>
constexpr const T&
elem(const T& v, const std::size_t) noexcept
{
    return v;
}

template <std::size_t I, typename... Args>
constexpr decltype(auto)
elem(const std::size_t i, const Args&... vectors) noexcept
{
    return elem(select<I>(vectors...), i);
}

template <typename... Vectors>
struct vector_dim;

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container,
          typename... Rest>
struct vector_dim<Vector<T, N, Container>, Rest...>
{
    static constexpr std::size_t n = N;
};

template <std::size_t N, typename T>
struct check_dim
{
    static constexpr bool value = true;
};

template <std::size_t N1,
          typename T,
          std::size_t N2,
          template <typename, std::size_t>
          typename Container>
struct check_dim<N1, Vector<T, N2, Container>>
{
    static constexpr bool value = N1 == N2;
};

template <typename T>
struct promote_tuple
{
    template <std::size_t N>
    using ret_type = Vector<std::decay_t<T>, N, std::array>;

    template <std::size_t N>
    static constexpr void
    assign(ret_type<N>& v, const std::size_t i, const T& val) noexcept
    {
        v[i] = val;
    }
};

template <typename T1, typename T2>
struct promote_tuple<std::pair<T1, T2>>
{
    template <std::size_t N>
    using ret_type = std::pair<Vector<std::decay_t<T1>, N, std::array>,
                               Vector<std::decay_t<T2>, N, std::array>>;

    template <std::size_t N>
    static constexpr void
    assign(ret_type<N>&             v,
           const std::size_t        i,
           const std::pair<T1, T2>& val) noexcept
    {
        std::tie(v.first[i], v.second[i]) = val;
    }
};

template <typename... Ts>
struct promote_tuple<std::tuple<Ts...>>
{
    template <std::size_t N>
    using ret_type = std::tuple<Vector<std::decay_t<Ts>, N, std::array>...>;

    template <std::size_t N>
    static constexpr void
    assign(ret_type<N>&             v,
           const std::size_t        i,
           const std::tuple<Ts...>& val) noexcept
    {
        std::apply([&val, &i](Ts&... vv) noexcept { std::tie(vv[i]...) = val; },
                   v);
    }
};

template <typename F, std::size_t... Idxs, typename... Vectors>
constexpr auto
transform_impl(const F& f,
               std::index_sequence<Idxs...>,
               const Vectors&... vs) noexcept
{
    using val_type = std::invoke_result_t<
        F,
        typename vector_val_type<
            std::tuple_element_t<Idxs, std::tuple<Vectors...>>>::type...>;
    constexpr std::size_t N = vector_dim<Vectors...>::n;
    static_assert((true && ... && check_dim<N, Vectors>::value),
                  "Vectors should have the same dimensionality");
    using pt       = promote_tuple<val_type>;
    using ret_type = typename pt::template ret_type<N>;

    ret_type ret{};

    for(std::size_t i = 0; i < N; ++i)
        pt::assign(ret, i, f(elem<Idxs>(i, vs...)...));

    return ret;
}

template <int         _Sgn,
          std::size_t J,
          std::size_t K,
          typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr T
cross_impl_factor_ab(const Vector<T, N, Container1>& a,
                     const Vector<T, N, Container2>& b) noexcept
{
    return _Sgn * a.template get<J>() * b.template get<K>();
}

template <std::size_t I,
          std::size_t J,
          std::size_t K,
          typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr T
cross_impl_factor(const Vector<T, N, Container1>& a,
                  const Vector<T, N, Container2>& b) noexcept
{
    // passing sgn as template parameter to force compile-time evaluation
    return cross_impl_factor_ab<sgn(std::array<std::size_t, 3>{I, J, K}), J, K>(
        a,
        b);
}

template <std::size_t I,
          std::size_t J,
          std::size_t... Ks,
          typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr T
cross_impl_ks(const Vector<T, N, Container1>& a,
              const Vector<T, N, Container2>& b,
              std::index_sequence<Ks...>) noexcept
{
    return (... + cross_impl_factor<I, J, Ks>(a, b));
}

template <std::size_t I,
          std::size_t... Js,
          typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr T
cross_impl_js(const Vector<T, N, Container1>& a,
              const Vector<T, N, Container2>& b,
              std::index_sequence<Js...>) noexcept
{
    return (... + cross_impl_ks<I, Js>(a, b, std::make_index_sequence<N>{}));
}

template <std::size_t... Is,
          typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr Vector<T, N, std::array>
cross_impl(const Vector<T, N, Container1>& a,
           const Vector<T, N, Container2>& b,
           std::index_sequence<Is...>) noexcept
{
    return Vector<T, N, std::array>(
        cross_impl_js<Is>(a, b, std::make_index_sequence<N>{})...);
}

template <std::size_t I,
          std::size_t J,
          std::size_t Idx,
          std::size_t Jdx,
          typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T
minor_matrix_op(const Matrix<T, M, N, Container>& m) noexcept
{
    constexpr std::size_t idx = Idx >= I ? Idx + 1 : Idx;
    constexpr std::size_t jdx = Jdx >= J ? Jdx + 1 : Jdx;
    return m.template get<idx, jdx>();
}

template <std::size_t I,
          std::size_t J,
          std::size_t Idx,
          std::size_t... Jdxs,
          typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr auto
minor_matrix_jdxs(const Matrix<T, M, N, Container>& m,
                  std::index_sequence<Jdxs...>) noexcept
{
    return std::tuple{minor_matrix_op<I, J, Idx, Jdxs>(m)...};
}

template <std::size_t I,
          std::size_t J,
          std::size_t... Idxs,
          typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr auto
minor_matrix_impl(const Matrix<T, M, N, Container>& m,
                  std::index_sequence<Idxs...>) noexcept
{
    return std::tuple_cat(
        minor_matrix_jdxs<I, J, Idxs>(m, std::make_index_sequence<N - 1>{})...);
}

template <typename Idxs,
          std::size_t... Is,
          typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr T
det_elem(const Matrix<T, M, M, Container>& m,
         std::index_sequence<Is...>) noexcept
{
    constexpr auto _sgn = sgn(intlist::stdarray<Idxs>);
    return _sgn * (... * m.template get<Is, intlist::at<Is, Idxs>>());
}

template <std::size_t... Ns,
          typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr T
det_impl(const Matrix<T, M, M, Container>& m,
         std::index_sequence<Ns...>) noexcept
{
    return (... +
            det_elem<intlist::nlperm<Ns, M>>(m, std::make_index_sequence<M>{}));
}
} // namespace detail

/// @addtogroup LINAL
/// @{

/// @brief Get vector view.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr Vector<const T, N, std::span>
ref(const Vector<T, N, Container>& v) noexcept
{
    return Vector(std::span<const T, N>(v.cbegin(), N));
}

/// @brief Get matrix view.
template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr Matrix<const T, M, N, std::span>
ref(const Matrix<T, M, N, Container>& m) noexcept
{
    return Matrix(std::span{m.data()});
}

/// @brief Get vector view of size @f$M\times N@f$ of matrix @p m.
template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr Vector<const T, M * N, std::span>
flat_ref(const Matrix<T, M, N, Container>& m) noexcept
{
    return Vector(std::span{m.data()});
}

/// @brief Apply function @p f to corresponding elements of input vectors @p vs.
/// @param f accepts as many arguments as the number of input vectors @p vs.
/// @param Vectors must be of the same dimensionality.
/// @return Vector containing results of application @p f to the elements of
/// input.
template <typename F, typename... Vectors>
constexpr auto
transform(const F& f, const Vectors&... vs) noexcept
{
    return detail::transform_impl(f,
                                  std::index_sequence_for<Vectors...>{},
                                  vs...);
}

/// @brief Accumulate elements of vector @p v with @p binary_op.
/// @param binary_op binary operator.
/// @param a vector to accumulate.
/// @param init initial value.
///
/// The operation is left-associative.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container,
          typename BinaryOperation,
          typename Init>
constexpr auto
reduce(BinaryOperation                binary_op,
       const Vector<T, N, Container>& v,
       Init                           init) noexcept
{
    for(std::size_t i = 0; i < N; ++i) init = binary_op(init, v[i]);

    return init;
}

/// @brief Efficient shortcut for consequent binary transform and reduce.
/// @param binary_op1 operator for transform.
/// @param binary_op2 operator for reduce.
///
/// @code{.cpp} reduce(binary_op2, transform(binary_op1, a, b), init);
/// @endcode
template <typename T1,
          typename T2,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2,
          typename BinaryOperation1,
          typename BinaryOperation2,
          typename Init>
constexpr auto
transform_reduce(BinaryOperation1                 binary_op1,
                 BinaryOperation2                 binary_op2,
                 const Vector<T1, N, Container1>& a,
                 const Vector<T2, N, Container2>& b,
                 Init                             init) noexcept
{
    for(std::size_t i = 0; i < N; ++i)
        init = binary_op2(init, binary_op1(a[i], b[i]));

    return init;
}

/// @brief Efficient shortcut for consequent unary transform and reduce.
/// @param unary_op operator for transform.
/// @param binary_op operator for reduce.
///
/// @code{.cpp} reduce(binary_op, transform(unary_op, a), init); @endcode
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container,
          typename UnaryOperation,
          typename BinaryOperation,
          typename Init>
constexpr auto
transform_reduce(UnaryOperation                 unary_op,
                 BinaryOperation                binary_op,
                 const Vector<T, N, Container>& a,
                 Init                           init) noexcept
{
    for(std::size_t i = 0; i < N; ++i) init = binary_op(init, unary_op(a[i]));

    return init;
}

VEC_NAMED_FN_OBJ(almost_equal_vec, almost_equal)
VEC_NAMED_FN_OBJ(isfinite, std::isfinite)
VEC_NAMED_FN_OBJ(clamp, std::clamp)
VEC_NAMED_FN_OBJ(minmax, std::minmax)
VEC_NAMED_FN_OBJ(fmod, std::fmod)
VEC_NAMED_FN_OBJ(atan, std::atan)
// functions that have static_math counterpart
VEC_NAMED_FN_OBJ_SMATH(sqrt)
VEC_NAMED_FN_OBJ_SMATH(abs)
VEC_NAMED_FN_OBJ_SMATH(pow)
VEC_NAMED_FN_OBJ_SMATH(sin)
VEC_NAMED_FN_OBJ_SMATH(cos)
VEC_NAMED_FN_OBJ_SMATH(tan)
VEC_NAMED_FN_OBJ_SMATH(round)
VEC_NAMED_FN_OBJ_SMATH(floor)

/// @}

namespace fn
{
// we define multiplies and operator* separately to
// not override Matrix multiplication
struct multiplies_fn
{
    template <typename T1,
              std::size_t N,
              template <typename, std::size_t>
              typename Container,
              typename T2>
    constexpr auto
    operator()(const Vector<T1, N, Container>& lhs,
               const T2&                       rhs) const noexcept
    {
        return transform(std::multiplies<T1>(), lhs, rhs);
    }

    COMPOSE_OP
};

/// @addtogroup LINAL
/// @{

/// @brief Compute dot product of the given vectors.
template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container1,
          template <typename, std::size_t> typename Container2>
constexpr T
dot(const Vector<T, N, Container1>& a,
    const Vector<T, N, Container2>& b) noexcept
{
    if constexpr(std::is_floating_point_v<T>)
    {
        T ret{};
        for(std::size_t i = 0; i < N; ++i) ret = std::fma(a[i], b[i], ret);
        return ret;
    }
    else
        return transform_reduce(std::multiplies<T>(),
                                std::plus<T>(),
                                a,
                                b,
                                T{0});
}

/// @brief Compute product of matrix and column-vector.
template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t> typename MContainer,
          template <typename, std::size_t> typename VContainer>
constexpr Vector<T, N, std::array>
dot(const Matrix<T, M, N, MContainer>& lhs,
    const Vector<T, N, VContainer>&    rhs) noexcept
{
    Vector<T, N, std::array> ret{};

    for(std::size_t i = 0; i < M; ++i)
        for(std::size_t j = 0; j < N; ++j)
            ret[i] = std::fma(lhs.at(i, j), rhs[j], ret[i]);

    return ret;
}

/// @brief Compute matrix product.
template <typename T,
          std::size_t M1,
          std::size_t N1,
          std::size_t M2,
          std::size_t N2,
          template <typename, std::size_t> typename Container1,
          template <typename, std::size_t> typename Container2>
constexpr typename std::enable_if_t<N1 == M2, Matrix<T, M1, N2, std::array>>
dot(const Matrix<T, M1, N1, Container1>& lhs,
    const Matrix<T, M2, N2, Container2>& rhs) noexcept
{
    Matrix<T, M1, N2, std::array> ret{};

    for(std::size_t i = 0; i < M1; ++i)
        for(std::size_t j = 0; j < N2; ++j)
            for(std::size_t r = 0; r < N1; ++r)
                ret.at(i, j) =
                    std::fma(lhs.at(i, r), rhs.at(r, j), ret.at(i, j));

    return ret;
}

/// @brief N-dimensional cross product.
///
/// The alogrithm:
/// @code{.cpp}
/// Vector<T, N, std::array> ret{0};
/// for(std::size_t i = 0; i < N; ++i)
///     for(std::size_t j = 0; j < N; ++j)
///         for(std::size_t k = 0; k < N; ++k)
///             ret[i] += sgn({i, j, k}) * a[j] * b[k];
/// @endcode
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr auto
cross(const Vector<T, N, Container1>& a,
      const Vector<T, N, Container2>& b) noexcept
{
    return detail::cross_impl(a, b, std::make_index_sequence<N>{});
}

/// @brief Compute squared length of a vector.
template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr T
length2(const Vector<T, N, Container>& a) noexcept
{
    if constexpr(std::is_floating_point_v<T>)
    {
        T ret{};
        for(std::size_t i = 0; i < N; ++i) ret = std::fma(a[i], a[i], ret);
        return ret;
    }
    else
        return transform_reduce(static_pow<2, T>, std::plus<T>(), a, T{0});
}

/// @brief Compute length of a vector.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T
length(const Vector<T, N, Container>& a) noexcept
{
    return sqrt(length2(a));
}

/// @brief Normalize vector.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr Vector<T, N, std::array>
normalize(const Vector<T, N, Container>& a) noexcept
{
    const T l = length(a);
    if constexpr(std::is_floating_point_v<T>)
        return (almost_equal(l, T{1}, 5) || almost_equal(l, T{0}, 5)) ? a :
                                                                        a / l;
    else
        return (l == 1 && l == 0) ? a : a / l;
}

/// @brief Compute distance between two vectors.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr T
distance(const Vector<T, N, Container1>& a,
         const Vector<T, N, Container2>& b) noexcept
{
    return length(a - b);
}

/// @brief Sum of vector elements.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T
sum(const Vector<T, N, Container>& a) noexcept
{
    return reduce(std::plus<T>(), a, T{0});
}

/// @brief Product of vector elements.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T
product(const Vector<T, N, Container>& a) noexcept
{
    return reduce(std::multiplies<T>(), a, T{1});
}

/// @brief Test whether all vector elements evaluate to true.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr bool
all(const Vector<T, N, Container>& a) noexcept
{
    return reduce(std::logical_and<bool>(), a, true);
}

/// @brief Test if at least one vector element evaluates to true.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr bool
any(const Vector<T, N, Container>& a) noexcept
{
    return reduce(std::logical_or<bool>(), a, false);
}

/// @brief Average of vector elements.
template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T
avg(const Vector<T, N, Container>& a) noexcept
{
    return sum(a) / N;
}

/// @brief Roll vector elements.
/// @param shift the number of places which elements are shifted. The sign
/// defines the direction, i.e. (-) to the left, otherwise - to the right.
template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr Vector<T, N, std::array>
roll(const Vector<T, N, Container>& v, const int shift) noexcept
{
    Vector<T, N, std::array> ret{};

    for(std::size_t i = 0; i < N; ++i) ret[i] = v[(i - shift) % N];

    return ret;
}

struct min_fn
{
    template <typename T>
    constexpr decltype(auto)
    operator()(const T& a, const T& b) const noexcept
    {
        if constexpr(std::is_arithmetic_v<T>)
            return std::min(a, b);
        else
            return transform(*this, a, b);
    }

    template <typename T,
              std::size_t N,
              template <typename, std::size_t>
              typename Container>
    constexpr T
    operator()(const Vector<T, N, Container>& a) const noexcept
    {
        return reduce(*this, a, std::numeric_limits<T>::max());
    }

    COMPOSE_OP
};

struct max_fn
{
    template <typename T>
    constexpr decltype(auto)
    operator()(const T& a, const T& b) const noexcept
    {
        if constexpr(std::is_arithmetic_v<T>)
            return std::max(a, b);
        else
            return transform(*this, a, b);
    }

    template <typename T,
              std::size_t N,
              template <typename, std::size_t>
              typename Container>
    constexpr T
    operator()(const Vector<T, N, Container>& a) const noexcept
    {
        return reduce(*this, a, std::numeric_limits<T>::min());
    }

    COMPOSE_OP
};

template <std::integral Int>
struct downsample_fn
{
    template <std::floating_point R>
    constexpr Int
    operator()(const R& value) const noexcept
    {
        R clamped = std::clamp(value, R{0}, R{1});
        clamped *= std::numeric_limits<Int>::max();
        return static_cast<Int>(clamped);
    }

    template <std::floating_point R,
              std::size_t         N,
              template <typename, std::size_t>
              typename Container>
    constexpr Vector<Int, N, std::array>
    operator()(const Vector<R, N, Container>& vec) const noexcept
    {
        return transform(*this, vec);
    }

    COMPOSE_OP
};

/// @brief Transpose matrix.
template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr auto
transpose(const Matrix<T, M, N, Container>& a) noexcept
{
    Matrix<T, N, M, std::array> ret{};

    for(std::size_t i = 0; i < N; ++i)
        for(std::size_t j = 0; j < M; ++j) ret[i][j] = a[j][i];

    return ret;
}

/// @brief Compute matrix determinant.
template <typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr T
det(const Matrix<T, M, M, Container>& m) noexcept
{
    return detail::det_impl(m, std::make_index_sequence<fac(M)>{});
}

/// @brief Contructs minor matrix by removing @p I row and @p J column.
/// @tparam I row to remove.
/// @tparam J column to remove.
template <std::size_t I,
          std::size_t J,
          typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr Matrix<T, M - 1, N - 1, std::array>
minor_matrix(const Matrix<T, M, N, Container>& m) noexcept
    requires(I < M || J < N)
{
    return std::make_from_tuple<Matrix<T, M - 1, N - 1, std::array>>(
        detail::minor_matrix_impl<I, J>(m, std::make_index_sequence<M - 1>{}));
}

namespace detail
{
template <std::size_t I,
          std::size_t J,
          typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr T
cofactor_op(const Matrix<T, M, M, Container>& m) noexcept
{
    return minus_one_pow(I + J) * det(minor_matrix<I, J>(m));
}

template <std::size_t I,
          std::size_t... Js,
          typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr auto
cofactor_js(const Matrix<T, M, M, Container>& m,
            std::index_sequence<Js...>) noexcept
{
    return std::tuple{cofactor_op<I, Js>(m)...};
}

template <std::size_t... Is,
          typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr auto
cofactor_impl(const Matrix<T, M, M, Container>& m,
              std::index_sequence<Is...>) noexcept
{
    return std::tuple_cat(cofactor_js<Is>(m, std::make_index_sequence<M>{})...);
}
} // namespace detail

/// @brief Compute cofactor matrix.
template <typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr Matrix<T, M, M, std::array>
cofactor(const Matrix<T, M, M, Container>& m) noexcept
{
    return std::make_from_tuple<Matrix<T, M, M, std::array>>(
        detail::cofactor_impl(m, std::make_index_sequence<M>{}));
}

/// @brief Compute inverse matrix @f$A^{-1}@f$.
template <typename T,
          std::size_t M,
          template <typename, std::size_t>
          typename Container>
constexpr Matrix<T, M, M, std::array>
inverse(const Matrix<T, M, M, Container>& a) noexcept
{
    const auto is_zero = [](const T& val) noexcept -> bool
    {
        if constexpr(std::is_floating_point_v<T>)
            return almost_equal(val, T{0}, 5);
        else
            return val == 0;
    };

    const T d = det(a);

    if(!is_zero(d))
        return transpose(cofactor(a)) * (T{1} / d);
    else
        return a;
}
/// @}
} // namespace fn

/// @addtogroup LINAL
/// @{

/// @brief Select the smallest corresponding elements of two vectors or numbers.
inline constexpr fn::min_fn min;

/// @brief Select the biggest corresponding elements of two vectors or numbers.
inline constexpr fn::max_fn max;

/// @brief Downsample floating point value to integer value with maximum integer
/// equal to 1.0 of floating point value.
template <typename Int>
inline constexpr fn::downsample_fn<Int> downsample;

MK_NIEBLOID(dot)
MK_NIEBLOID(cross)
MK_NIEBLOID(length2)
MK_NIEBLOID(length)
MK_NIEBLOID(normalize)
MK_NIEBLOID(distance)
MK_NIEBLOID(sum)
MK_NIEBLOID(product)
MK_NIEBLOID(all)
MK_NIEBLOID(any)
MK_NIEBLOID(avg)
MK_NIEBLOID(roll)
MK_NIEBLOID(transpose)
MK_NIEBLOID(det)
MK_NIEBLOID(inverse)

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr const T&
get_x(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<0>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T&
get_x(Vector<T, N, Container>& v) noexcept
{
    return v.template get<0>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr const T&
get_y(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<1>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T&
get_y(Vector<T, N, Container>& v) noexcept
{
    return v.template get<1>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr const T&
get_z(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<2>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T&
get_z(Vector<T, N, Container>& v) noexcept
{
    return v.template get<2>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr const T&
get_w(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<3>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr T&
get_w(Vector<T, N, Container>& v) noexcept
{
    return v.template get<3>();
}

constexpr fn::multiplies_fn multiplies;

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container,
          template <typename, std::size_t> typename Container2>
constexpr auto
operator*(const Vector<T, N, Container>&  lhs,
          const Vector<T, N, Container2>& rhs) noexcept
{
    return multiplies(lhs, rhs);
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr auto
operator*(const Vector<T, N, Container>& lhs, const T& rhs) noexcept
{
    return multiplies(lhs, rhs);
}

template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename MContainer,
          template <typename, std::size_t>
          typename VContainer>
constexpr Vector<T, N, std::array>
operator*(const Matrix<T, M, N, MContainer>& m,
          const Vector<T, N, VContainer>&    v) noexcept
{
    return dot(m, v);
}

template <typename T,
          std::size_t M1,
          std::size_t N1,
          std::size_t M2,
          std::size_t N2,
          template <typename, std::size_t>
          typename Container1,
          template <typename, std::size_t>
          typename Container2>
constexpr typename std::enable_if_t<N1 == M2, Matrix<T, M1, N2, std::array>>
operator*(const Matrix<T, M1, N1, Container1>& a,
          const Matrix<T, M2, N2, Container2>& b) noexcept
{
    return dot(a, b);
}

template <typename T,
          std::size_t M,
          std::size_t N,
          template <typename, std::size_t>
          typename Container>
constexpr Matrix<T, M, N, std::array>
operator*(const Matrix<T, M, N, Container>& m, const T& s) noexcept
{
    Matrix<T, M, N, std::array> ret{};

    for(std::size_t i = 0; i < M * N; ++i) ret.at(i) = m.at(i) * s;

    return ret;
}

DEF_UNARY_OP(negate, operator-)
DEF_UNARY_OP(logical_not, operator!)
DEF_BINARY_OP(plus, operator+)
DEF_BINARY_OP(minus, operator-)
DEF_BINARY_OP(divides, operator/)
DEF_BINARY_OP(equal_to, operator==)
DEF_BINARY_OP(not_equal_to, operator!=)
DEF_BINARY_OP(greater, operator>)
DEF_BINARY_OP(less, operator<)
DEF_BINARY_OP(greater_equal, operator>=)
DEF_BINARY_OP(less_equal, operator<=)

/// @}
} // namespace lucid

#undef DEF_BINARY_OP
#undef DEF_UNARY_OP
#undef VEC_NAMED_FN_OBJ
#undef VEC_FN_OBJ
#undef VEC_NAMED_FN_OBJ_SMATH

namespace std
{
template <typename T, size_t N, template <typename, size_t> typename Container>
class tuple_size<lucid::Vector<T, N, Container>>
{
  public:
    static constexpr const size_t value = N;
};

template <size_t I,
          typename T,
          size_t N,
          template <typename, size_t>
          typename Container>
class tuple_element<I, lucid::Vector<T, N, Container>>
{
  public:
    using type = T;
};

template <size_t I,
          typename T,
          size_t N,
          template <typename, size_t>
          typename Container>
constexpr decltype(auto)
get(const lucid::Vector<T, N, Container>& vec) noexcept
{
    return vec.template get<I>();
}

template <typename T,
          size_t M,
          size_t N,
          template <typename, size_t>
          typename Container>
class tuple_size<lucid::Matrix<T, M, N, Container>>
{
  public:
    static const constexpr size_t value = M;
};

template <size_t I,
          typename T,
          size_t M,
          size_t N,
          template <typename, size_t>
          typename Container>
class tuple_element<I, lucid::Matrix<T, M, N, Container>>
{
  public:
    using type = lucid::Vector<T, N, std::span>;
};

template <size_t I,
          typename T,
          size_t M,
          size_t N,
          template <typename, size_t>
          typename Container>
class tuple_element<I, const lucid::Matrix<T, M, N, Container>>
{
  public:
    using type = lucid::Vector<const T, N, std::span>;
};

template <size_t I,
          typename T,
          size_t M,
          size_t N,
          template <typename, size_t>
          typename Container>
constexpr decltype(auto)
get(const lucid::Matrix<T, M, N, Container>& mat) noexcept
{
    return mat.template get<I>();
}
} // namespace std
