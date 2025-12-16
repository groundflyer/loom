// -*- C++ -*-
// typedefs.hpp
//

#pragma once

#include "linal.hpp"

namespace loom
{
template <typename T, template <typename, std::size_t> typename Container>
using Vec2 = Vector<T, 2, Container>;

template <typename T, template <typename, std::size_t> typename Container>
using Vec3 = Vector<T, 3, Container>;

template <typename T, template <typename, std::size_t> typename Container>
using Vec4 = Vector<T, 4, Container>;

template <typename T, template <typename, std::size_t> typename Container>
using Mat3 = Matrix<T, 3, 3, Container>;

template <typename T, template <typename, std::size_t> typename Container>
using Mat4 = Matrix<T, 4, 4, Container>;

template <typename T>
using Vec2a = Vec2<T, std::array>;

template <typename T>
using Vec3a = Vec3<T, std::array>;

template <typename T>
using Vec4a = Vec4<T, std::array>;

template <typename T>
using Mat3a = Mat3<T, std::array>;

template <typename T>
using Mat4a = Mat4<T, std::array>;

template <typename T>
inline constexpr Mat3a<T> m3id = Mat3a<T>::identity();

template <typename T>
inline constexpr Mat4a<T> m4id = Mat4a<T>::identity();

using Vec2u = Vec2<unsigned, std::array>;

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr const T&
get_x(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<0>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr T&
get_x(Vector<T, N, Container>& v) noexcept
{
    return v.template get<0>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr const T&
get_y(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<1>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr T&
get_y(Vector<T, N, Container>& v) noexcept
{
    return v.template get<1>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr const T&
get_z(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<2>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr T&
get_z(Vector<T, N, Container>& v) noexcept
{
    return v.template get<2>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr const T&
get_w(const Vector<T, N, Container>& v) noexcept
{
    return v.template get<3>();
}

template <typename T,
          std::size_t N,
          template <typename, std::size_t> typename Container>
constexpr T&
get_w(Vector<T, N, Container>& v) noexcept
{
    return v.template get<3>();
}
} // namespace loom
