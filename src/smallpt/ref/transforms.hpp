// -*- C++ -*-
/// @file transforms.hpp
/// @brief Coordinate transformations.

#pragma once

#include <smallpt/common/typedefs.hpp>

#include <utility>

namespace loom
{
namespace fn
{
/// @brief Build a normalized vector.
template <std::floating_point T, std::floating_point... Args>
constexpr Vec3<T>
make_normal(const T a, const Args... args) noexcept
{
    return normalize(Vec3<T>(a, args...));
}

/// @brief Convert a vector from homogenous coordinates.
/// @param a homogenous column vector.
template <std::floating_point T,
          template <typename, std::size_t> typename Container>
constexpr Vec3<T>
dehomogenize(const Vec4<T, Container>& a) noexcept
{
    Vec3<T> ret{a};
    const T w = get_w(a);
    if(!(almost_equal(w, static_cast<T>(0), 5) ||
         almost_equal(w, static_cast<T>(1), 5)))
        ret /= w;
    return ret;
}

/// @brief Apply transform to a vector.
/// @param t transform to apply.
/// @param v vector to transform.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer,
          template <typename, std::size_t> typename VecContainer>
constexpr Vec3<T>
apply_transform(const Mat4<T, MatContainer>& t,
                const Vec3<T, VecContainer>& v) noexcept
{
    return dehomogenize(t * Vec4<T>(v));
}

/// @brief Apply transform to a point.
/// @param t transform to apply.
/// @param p point to transform.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer,
          template <typename, std::size_t> typename Vec3Container>
constexpr Vec3<T>
apply_transform_p(const Mat4<T, MatContainer>&  t,
                  const Vec3<T, Vec3Container>& p) noexcept
{
    return Vec3<T>(t * Vec4<T>(p, 1));
}

/// @brief Apply transform to a normal.
/// @param t transform to apply.
/// @param n noraml to transform.
template <std::floating_point T,
          template <typename, std::size_t> typename MatContainer,
          template <typename, std::size_t> typename NormalContainer>
constexpr Vec3<T>
apply_transform_n(const Mat4<T, MatContainer>&    t,
                  const Vec3<T, NormalContainer>& n) noexcept
{
    return normalize(dehomogenize(transpose(inverse(t)) * (Vec4<T>(n))));
}

/// @brief Create translation transform.
/// @param delta translation delta.
template <std::floating_point T,
          template <typename, std::size_t> typename Container>
constexpr Mat4<T>
translate(const Vec3<T, Container>& delta) noexcept
{
    const auto& [x, y, z] = delta;
    return Mat4<T>(1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1);
}

/// @brief Create scale transform.
/// @param delta scale delta.
template <std::floating_point T,
          template <typename, std::size_t> typename Container>
constexpr Mat4<T>
scale(const Vec3<T, Container>& delta) noexcept
{
    const auto& [x, y, z] = delta;
    return Mat4<T>(x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1);
}

/// @brief Create clockwise rotation transform.
/// @param angle angle defined in radians.
/// @param axis axis to rotate object around.
template <std::floating_point T,
          template <typename, std::size_t> typename Container>
constexpr Mat4<T>
rotate(const T angle, const Vec3<T, Container>& axis) noexcept
{
    const auto& [x, y, z] = axis;
    const Mat3<T> K{0, -z, y, z, 0, -x, -y, x, 0};
    return Mat4<T>(m3id<T> + K * sin(angle) +
                       K * K * (static_cast<T>(1) - cos(angle)),
                   0,
                   0,
                   0,
                   1);
}

/// @brief Create Look-At transform.
/// @param eye eye position.
/// @param target target position.
/// @param up vector (usually {0, 1, 0}).
template <std::floating_point T,
          template <typename, std::size_t> typename EyeContainer,
          template <typename, std::size_t> typename CenContainer,
          template <typename, std::size_t> typename UpContainer>
constexpr Mat4<T>
look_at(const Vec3<T, EyeContainer>& eye,
        const Vec3<T, CenContainer>& target,
        const Vec3<T, UpContainer>&  up) noexcept
{
    const Vec3<T> f = normalize(eye - target);
    const Vec3<T> r = normalize(cross(up, f));
    const Vec3<T> u = normalize(cross(f, r));
    return inverse(Mat4<T>(r, 0, u, 0, f, 0, 0, 0, 0, 1) * translate(-eye));
}

/// @brief Build orthonormal basis for given normal (z axis).
/// @cite Duff2017Basis
/// @return a pair of normals representing x and y axes.
template <std::floating_point T,
          template <typename, std::size_t> typename Container>
constexpr std::pair<Vec3<T>, Vec3<T>>
basis(const Vec3<T, Container>& n) noexcept
{
    const auto& [nx, ny, nz] = n;
    const T sign             = std::copysign(static_cast<T>(1), nz);
    const T a                = static_cast<T>(-1) / (sign + nz);
    const T b                = nx * ny * a;
    return std::pair(make_normal(static_cast<T>(1) + sign * nx * nx * a,
                                 sign * b,
                                 -sign * nx),
                     make_normal(b, sign + ny * ny * a, -ny));
}

/// @brief Build orthonormal basis for given normal (z axis).
/// @cite Duff2017Basis
/// @return 3x3 rotation matrix.
template <std::floating_point T,
          template <typename, std::size_t> typename Container>
constexpr Mat3<T>
basis_matrix(const Vec3<T, Container>& z) noexcept
{
    const auto [x, y] = basis(z);
    return transpose(Mat3<T>(x, y, z));
}
} // namespace fn
} // namespace loom
