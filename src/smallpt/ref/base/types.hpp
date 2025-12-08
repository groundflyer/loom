// -*- C++ -*-
// types.hpp --
//

#pragma once

#include "linal.hpp"

#include <cstdint>

namespace loom
{
#ifdef _DOUBLE_PRECISION
typedef double real;
#else
typedef float real;
#endif // _DOUBLE_PRECISION

constexpr real operator"" _r(long double d) { return static_cast<real>(d); }

constexpr real operator"" _r(unsigned long long d)
{
    return static_cast<real>(d);
}

template <template <typename, std::size_t> typename Container>
using Vec2_ = Vector<real, 2, Container>;

template <template <typename, std::size_t> typename Container>
using Vec2i_ = Vector<int, 2, Container>;

template <template <typename, std::size_t> typename Container>
using Vec2u_ = Vector<unsigned, 2, Container>;

template <template <typename, std::size_t> typename Container>
using Vec3_ = Vector<real, 3, Container>;

template <template <typename, std::size_t> typename Container>
using Vec4_ = Vector<real, 4, Container>;

template <template <typename, std::size_t> typename Container>
using Vec3i_ = Vector<int, 3, Container>;

template <template <typename, std::size_t> typename Container>
using Mat2_ = Matrix<real, 2, 2, Container>;

template <template <typename, std::size_t> typename Container>
using Mat3_ = Matrix<real, 3, 3, Container>;

template <template <typename, std::size_t> typename Container>
using Mat4_ = Matrix<real, 4, 4, Container>;

template <template <typename, std::size_t> typename Container>
using RGB_ = Vector<float, 3, Container>;

template <template <typename, std::size_t> typename Container>
using RGBA_ = Vector<float, 4, Container>;

template <template <typename, std::size_t> typename Container>
using RGB8_ = Vector<std::uint8_t, 3, Container>;

template <template <typename, std::size_t> typename Container>
using RGBA8_ = Vector<std::uint8_t, 4, Container>;

using Vec2  = Vec2_<std::array>;
using Vec3  = Vec3_<std::array>;
using Vec4  = Vec4_<std::array>;
using Vec3i = Vec3i_<std::array>;
using Vec2i = Vec2i_<std::array>;
using Vec2u = Vec2u_<std::array>;
using Mat2  = Mat2_<std::array>;
using Mat3  = Mat3_<std::array>;
using Mat4  = Mat4_<std::array>;
using RGB   = RGB_<std::array>;
using RGBA  = RGBA_<std::array>;
using RGB8  = RGB8_<std::array>;
using RGBA8 = RGBA8_<std::array>;

inline constexpr real Pi      = Pi_<real>;
inline constexpr Mat3 Mat3_id = Mat3::identity();
inline constexpr Mat4 Mat4_id = Mat4::identity();
} // namespace loom
