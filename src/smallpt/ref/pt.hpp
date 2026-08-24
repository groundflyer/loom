// -*- C++ -*-
/// @file pt.hpp
/// @brief Reference smallpt functions.

#pragma once

#include "niebloids.hpp"
#include "ray.hpp"

#include <common/debug.hpp>
#include <common/forbid.hpp>
#include <common/functional.hpp>
#include <common/math.hpp>
#include <common/random.hpp>
#include <common/tuple.hpp>
#include <smallpt/common/common.hpp>
#include <smallpt/common/typedefs.hpp>

#include <array>
#include <atomic>
#include <cstdint>

namespace loom
{
template <std::floating_point real>
static real
convert_fov(const real fov) noexcept
{
    return std::tan(fov * real(0.5));
}

/// @brief Perspective camera.
template <std::floating_point real>
struct Perspective
{
    real       tan_fov_half{1};
    Vec3<real> pos;
    Vec3<real> dir;

    template <template <typename, std::size_t> typename Container>
    constexpr Ray<real>
    operator()(const Vec2<real, Container>& ndc) const noexcept
    {
        return {pos, dir - Vec3<real>(ndc * tan_fov_half)};
    }
};

namespace detail
{
template <unsigned idx, std::floating_point T, typename Prim, typename... Rest>
std::pair<unsigned, Intersection<T>>
hider_impl(const std::pair<unsigned, Intersection<T>>& prev,
           const Ray<T>&                               ray,
           const Prim&                                 prim,
           const Rest&... rest) noexcept
{
    const Intersection<T> prev_isect    = prev.second;
    const Intersection<T> current_isect = intersect(ray, prim);
    const std::pair<unsigned, Intersection<T>>  current{idx, current_isect};
    const std::pair<unsigned, Intersection<T>>& closest =
        prev_isect ? (prev_isect.t < current_isect.t ? prev : current) :
                     current;

    if constexpr(sizeof...(Rest) > 0)
        return hider_impl<idx + 1>(closest, ray, rest...);
    else
        return closest;
}
} // namespace detail

template <std::floating_point T>
struct Hider
{
    Ray<T> ray;

    template <typename... Prims>
    std::pair<unsigned, Intersection<T>>
    operator()(const Prims&... prims) const noexcept
    {
        return detail::hider_impl<0>(std::pair{-1u, Intersection<T>{}},
                                     ray,
                                     prims...);
    }
};

/// @brief Generate vector of two random numbers [0, 1).
static Vec2<float>
sample_u(const std::array<std::uint32_t, 2>& u) noexcept
{
    return transform(gen_canonical, Vec2<std::uint32_t>(u));
}

template <std::floating_point T>
static Vec2<T>
jitter_sample_pos(const T            width,
                  const Vec2<T>&     pos,
                  const Vec2<float>& u) noexcept
{
    return (pos - width * static_cast<T>(0.5)) + u * width;
}

template <std::floating_point T>
static Vec3<T>
sample_hemisphere(const Vec3<T>& n, const Vec2<T>& u) noexcept
{
    const auto& [u1, u2] = u;
    const T r1           = static_cast<T>(2) * Pi<T> * (u1 - T{0.5});
    const T r2s          = std::sqrt(u2);
    const auto [x, y]    = basis(n);
    return normalize(x * std::cos(r1) * r2s + y * std::sin(r1) * r2s +
                     n * std::sqrt(1 - u2));
}

/// @brief Pixel position to film sample space.
template <std::floating_point T>
struct ToSampleSpace
{
    Vec2<T> res;
    T       ratio;

  public:
    ToSampleSpace(const Vec2u& _res) noexcept :
        res(_res), ratio(res[0] / res[1])
    {
    }

    Vec2<T>
    operator()(const Vec2u& pos) const noexcept
    {
        const auto& [x, y] = pos;
        const auto [w, h]  = res;
        const T sx = ratio * (static_cast<T>(x) / w - static_cast<T>(0.5));
        const T sy = static_cast<T>(y) / h - static_cast<T>(0.5);
        return Vec2<T>{sx, sy};
    }
};

/// @brief Fresnel term approximation.
/// @param cd cosine theta.
/// @param eta relative index of refraction.
///
/// See @cite 10.1111:1467-8659.1320121
template <std::floating_point T>
static T
schlick(const T cd, const T eta) noexcept
{
    T r0 = (1 - eta) / (1 + eta);
    r0 *= r0;
    return r0 + (1 - r0) * static_pow<5>(1 - cd);
}

/// @brief Cornell box description.
template <std::floating_point real>
class cornell_box
{
    using Vec2r = Vec2<real>;
    using Vec3r = Vec3<real>;
    using Mat4r = Mat4<real>;
    using Rgb   = RGB<std::array>;

    // Box points coordinates
    static constexpr std::array<Vec3r, 8> room_points{Vec3r(-1, -1, 1),  // 0
                                                      Vec3r(-1, -1, -1), // 1
                                                      Vec3r(1, -1, -1),  // 2
                                                      Vec3r(1, -1, 1),   // 3
                                                      Vec3r(-1, 1, 1),   // 4
                                                      Vec3r(-1, 1, -1),  // 5
                                                      Vec3r(1, 1, -1),   // 6
                                                      Vec3r(1, 1, 1)};   // 7

    // Quad point indices for room geometry
    using room_geo_descr =
        type_sequence<std::index_sequence<0, 3, 2, 1>,  // floor
                      std::index_sequence<0, 1, 5, 4>,  // left wall
                      std::index_sequence<2, 3, 7, 6>,  // right wall
                      std::index_sequence<4, 5, 6, 7>,  // ceiling
                      std::index_sequence<1, 2, 6, 5>>; // back wall

    static constexpr real tr = smath::sqrt(static_cast<real>(3)) / 6;
    static constexpr real tR = 2 * tr;
    // Tetrahedron vertices
    static constexpr std::array<Vec3r, 4> tetrahedron_points{
        Vec3r(-0.5, 0, -tr),   // 0
        Vec3r(0, 0, tR),       // 1
        Vec3r(0.5, 0, -tr),    // 2
        Vec3r(0, tr + tR, 0)}; // 3

    // Triangle point indices for tetrahedron
    using tetrahedron_geo_descr = type_sequence<std::index_sequence<0, 1, 3>,
                                                std::index_sequence<1, 2, 3>,
                                                std::index_sequence<2, 0, 3>>;

    static constexpr Mat4r tetrahedron_transform =
        translate(Vec3r{-0.4, -1, -0.2}) *
        rotate(radians(static_cast<real>(-15)), Vec3r{0, 1, 0}) *
        scale(Vec3r{0.95});

    static constexpr Vec3r cyl_pos{0.5, -1, -0.3};
    static constexpr real  cyl_rad{0.25};
    static constexpr real  cyl_h{0.65};

  public:
    FORBID_ALL(cornell_box);

    struct Gallery
    {
        struct Lambert
        {
            const Rgb color{0};
        };

        struct Emission
        {
            static constexpr real value = 5;
        };

        struct Glass
        {
            static constexpr real ior = 1.5;
        };

        struct Plastic
        {
            static constexpr real ior = 2.5;
            static constexpr Rgb  color{0, 0.212, 0.553};
        };

        struct Metal
        {
            static constexpr Vec2r alpha{0.3, 0.05};
            static constexpr Rgb   color{1, 0.827, 0.537};
        };

        struct Window
        {
            static constexpr real ior = 1.56;
        };

        static constexpr std::tuple materials{
            Lambert{Rgb{1}},           // 0
            Lambert{Rgb{1, 0.4, 0.2}}, // 1
            Lambert{Rgb{0.2, 1, 0.4}}, // 2
            Plastic{},                 // 3
            Glass{},                   // 4
            Metal{},                   // 5
            Emission{},                // 6
            Window{}                   // 7
        };

        // Material indicies for primitives
        static constexpr unsigned mat_idxs[]{
            // room of 5 primitives
            0, // grey floor
            1, // red wall
            2, // green wall
            0, // grey ceiling
            7, // back window
               // objects
            3, // plastic tetrahedron of three primitives
            3,
            3,
            4, // glass sphere
            5, // metallic cylinder
            5, // metallic cylinder cap
            6  // light
        };
    };

    static constexpr Disk<real> light{
        Vec3r{0, 0.9999, 0},
        Vec3r{0,     -1, 0},
        0.35
    };

    static constexpr auto geometry = std::tuple_cat(
        // room
        typelist::maybe_collect(
            room_geo_descr{
    },
            []<typename idxs>() noexcept -> Quad<real>
            {
                return intlist::collect<Quad<real>>(idxs{},
                                                    []<std::size_t i>() noexcept
                                                    { return room_points[i]; });
            }),
        // objects
        typelist::maybe_collect(tetrahedron_geo_descr{},
                                []<typename idxs>() noexcept -> Triangle<real>
                                {
                                    return apply_transform(
                                        tetrahedron_transform,
                                        intlist::collect<Triangle<real>>(
                                            idxs{},
                                            []<std::size_t i>() noexcept
                                            { return tetrahedron_points[i]; }));
                                }),
        std::tuple{
            Sphere<real>{Vec3r{0.1, -0.7, 0.3}, 0.3},
            Cylinder<real>{cyl_pos, cyl_rad, cyl_h},
            Disk<real>{cyl_pos + Vec3r{0, cyl_h, 0}, Vec3r{0, 1, 0}, cyl_rad},
            light});

    static Perspective<real>
    camera(const real fov = radians(static_cast<real>(110))) noexcept
    {
        return {convert_fov(fov), Vec3r(0, 0, 2), Vec3r(0, 0, -1)};
    }
};

using Scene   = cornell_box<float>;
using Gallery = Scene::Gallery;
using Camera  = Perspective<float>;
using Vec2f   = Vec2<float>;
using Vec3f   = Vec3<float>;
using Counter = std::array<std::uint32_t, 2>;

static Ray<float>
bias_ray(const Vec3f& p, const Vec3f& d) noexcept
{
    constexpr float bias = 0.001;
    return {p + d * bias, d};
}

static inline float
luminance(const RGB<>& rgb) noexcept
{
    return dot(rgb, RGB{0.2126f, 0.7152f, 0.0722});
}

/// @brief Compute relative index of refraction.
///
/// Incident medium is vacuum (1).
static float
compute_eta(const float ior, const bool enter) noexcept
{
    return enter ? 1 / ior : ior;
}

static float
reflectance(const float cos_theta, const float eta) noexcept
{
    return schlick(std::abs(cos_theta), eta);
}

static Vec3f
reflect(const Vec3f& n, const Vec3f& i, const float cos_theta) noexcept
{
    return i + n * cos_theta * 2.f;
}

static Vec3f
refract(const Vec3f& nfn,
        const Vec3f& i,
        const float  eta,
        const float  ddn,
        const float  cos2t) noexcept
{
    return normalize(i * eta - nfn * std::fma(ddn, eta, std::sqrt(cos2t)));
}

/// @brief Distribution based on Trowbridge-Reitz @cite Trowbridge:75.
/// @param dotWmWg dot product of normal and microfacet.
/// @param alpha surface roughness.
///
/// See @cite Trowbridge:75 and @cite 10.2312:EGWR/EGSR07/195-206
/// Slightly modified for beautification.
static float
gtr(const float dotWmWg, const float alpha) noexcept
{
    return alpha / (alpha + 1 / (dotWmWg * dotWmWg) - 1);
}

/// @brief Microfacet geometry attenuation factor.
/// @param cos_theta cosine of angle.
/// @param alpha surface rougness.
///
/// See @cite 10.2312:EGWR/EGSR07/195-206
static float
gaf(const float cos_theta, const float alpha) noexcept
{
    return 2 / (1 + std::sqrt(std::fma(alpha * alpha,
                                       (1 / (cos_theta * cos_theta) - 1),
                                       1)));
}

/// @brief Microfacet slope generation.
///
/// See @cite HeitzdEon2014
static Vec2f
get_slope(const float cos_theta, const Vec2f u) noexcept
{
    auto [sx, sy] = u;

    if(cos_theta > 0.9999f)
    {
        const float r   = std::sqrt(sx / (1 - sx));
        const float phi = Pi<float> * 2 * (1 - sy);
        return Vec2f{r * std::cos(phi), r * std::sin(phi)};
    }

    const float cos_theta2 = cos_theta * cos_theta;
    const float B2         = (1 - cos_theta2) / std::max(cos_theta2, 1e-6f);
    const float B          = std::sqrt(B2);
    const float a          = 1 / B;
    const float G1         = 2 / (1 + std::sqrt(1 + 1 / (a * a)));
    const float A          = 2 * sx / G1 - 1;
    const float A2         = A * A;
    const float denom      = A2 - 1;
    const float tmp =
        1 / ((std::abs(denom) < 1e-6f) ? std::copysign(1e-6f, denom) : denom);
    const float D       = std::sqrt(B2 * tmp * tmp - (A2 - B2) * tmp);
    const float Btmp    = B * tmp;
    const float slopex1 = Btmp - D;
    const float slopex2 = Btmp + D;

    const float retx = (A < 0 || slopex2 > 1 / B) ? slopex1 : slopex2;

    float S;
    if(sy > .5f)
    {
        S  = 1;
        sy = 2 * (sy - .5f);
    }
    else
    {
        S  = -1;
        sy = 2 * (.5f - sy);
    }

    const float z =
        sy * std::fma(std::fma(sy, .27385f, -.73369f), sy, .46341f) /
        std::fma(std::fma(std::fma(sy, .093073f, .309420f), sy, -1.f),
                 sy,
                 .597999f);
    const float rety = S * z * std::sqrt(1 + retx * retx);

    return Vec2f{retx, rety};
}

/// @brief Microfacet generation.
/// @param wi incident direction.
/// @param alpha surface roughness.
///
/// See @cite HeitzdEon2014
static Vec3f
microfacet(Vec3f wi, const Vec2f& alpha, const Vec2f u) noexcept
{
    const auto [alphax, alphay] = alpha;

    wi[0] *= alphax;
    wi[1] *= alphay;

    wi = normalize(wi);

    const auto [wix, wiy, wiz] = wi;

    const auto [sx, sy] = get_slope(wiz, u);

    const float lensq  = std::sqrt(wix * wix + wiy * wiy);
    const bool  tb     = lensq > 1e-12f;
    const float cosphi = tb ? wix / lensq : 1.f;
    const float sinphi = tb ? wiy / lensq : 0.f;

    const float nx = std::fma(sinphi, sy, cosphi * sx) * alphax;
    const float ny = std::fma(cosphi, sy, -sinphi * sx) * alphay;

    return make_normal(-nx, -ny, 1.f);
}

/// @brief incident projected roughness
static float
anisorough_i(const Vec3f& wi,
             const Vec3f& tu,
             const Vec3f& tv,
             const Vec2f  alpha) noexcept
{
    const float cs = dot(wi, tu);
    const float sn = dot(wi, tv);

    const auto [alphax, alphay] = alpha;

    return std::sqrt(cs * cs * alphax * alphax + sn * sn * alphay * alphay);
}

/// @brief projected on surface rougness.
static float
anisorough_n(const Vec3f& wm,
             const Vec3f& wg,
             const Vec3f& tu,
             const Vec3f& tv,
             const float  dotWmWg,
             const Vec2f& alpha) noexcept
{
    const Vec3f ph = normalize(wm - wg * dotWmWg);
    const float cu = dot(ph, tu);
    const float cv = dot(ph, tv);

    const auto [alphax, alphay] = alpha;

    return 1 / (cu * cu / alphax + cv * cv / alphay);
}

template <typename Prim>
static Vec3f
tangent(const Vec3f& /* p */, const Prim&) noexcept
{
    return Vec3f{0, 1, 0};
}

static Vec3f
tangent(const Vec3f& p, const Disk<float>& prim) noexcept
{
    return normalize(p - prim.position);
}

inline constexpr std::array<std::uint32_t, 2> key{0x1dead000, 0x200beef};

struct Shading
{
    using ret_t = std::pair<RGB<>, unsigned>;

    static constexpr float    p_nee    = 0.5f;
    static constexpr unsigned ctr_incr = 39916801;

    unsigned max_depth;
    Counter  c;
    unsigned depth;
    unsigned ray_count;
    Vec3f    p;
    Vec3f    n;
    Vec3f    t;
    Vec3f    i;

    static ret_t
    radiance(const unsigned    max_depth,
             const Ray<float>& ray,
             const Counter&    c,
             const unsigned    depth     = 0,
             unsigned          ray_count = 0) noexcept
    {
        if(depth >= max_depth) return {{}, ray_count};

        const auto [prim_id, isect] = std::apply(Hider{ray}, Scene::geometry);
        ray_count++;

        if(!isect) return {{}, ray_count};

        const Vec3f p     = ray.origin + ray.dir * isect.t;
        const auto [n, t] = visit_at(
            prim_id,
            [&p](const auto& prim) noexcept
            {
                return std::pair{normal(p, prim), tangent(p, prim)}
                 ;
        },
            Scene::geometry,
            constant(std::pair{Vec3f{0, 1, 0}, Vec3f{0, 1, 0}}));

        return visit_at(Gallery::mat_idxs[prim_id],
                        Shading{
                            max_depth,
                            c,
                            depth + 1,
                            ray_count,
                            p,
                            n,
                            t,
                            ray.dir
        },
                        Gallery::materials,
                        constant(ret_t{RGB{1, 0, 1}, ray_count}));
    }

    /// @brief Direct lighting estimator.
    /// @return light PDF, cos(theta) and direction to light.
    static std::pair<std::optional<std::tuple<float, float, Vec3f>>, unsigned>
    direct(const Vec3f& p, const Vec3f& n, const Counter& c) noexcept
    {
        const Vec3f lp =
            sample(sample_u(threefry2x32r32<key[0], key[1]>(c)), Scene::light);
        const Vec3f d         = normalize(lp - p);
        const float cos_theta = dot(n, d);

        if(cos_theta < 0) return {{}, 0}; // haven't trace

        const auto [prim_id, isect] =
            std::apply(Hider{bias_ray(p, d)}, Scene::geometry);

        if(!isect || prim_id != 11) return {{}, 1}; // traced one ray

        const float     l = length2(Scene::light.position - p);
        constexpr float pa =
            Scene::light.radius * Scene::light.radius * Pi<float>;

        return {
            {{l / (std::abs(dot(Scene::light.normal, d)) * pa), cos_theta, d}},
            1u};
    }

    ret_t
    lambert(const RGB<>& color) const noexcept
    {
        const Vec3f d =
            sample_hemisphere(n, sample_u(threefry2x32r32<key[0], key[1]>(c)));

        auto cc = bump_ctr(c, ctr_incr);

        auto [L, rc] =
            radiance(max_depth, bias_ray(p, d), cc, depth, ray_count);

        cc = bump_ctr(cc, ctr_incr);

        const auto [r, _] = threefry2x32r32<key[0], key[1]>(cc);

        cc = bump_ctr(cc, ctr_incr);

        if(gen_canonical(r) < p_nee)
        {
            const auto [dr, drc] = direct(p, n, cc);
            rc += drc;
            if(dr)
            {
                const auto [p_l, ctl, _] = dr.value();
                L += Scene::Gallery::Emission::value * ctl / (p_l * p_nee);
            }
        }

        return {color * L, rc};
    }

    ret_t
    reflection() const noexcept
    {
        const float cos_theta = dot(n, -i);
        const bool  into      = cos_theta > 0;
        const Vec3f nfn       = into ? n : -n;

        return radiance(max_depth,
                        bias_ray(p, reflect(nfn, i, cos_theta)),
                        bump_ctr(c, ctr_incr),
                        depth,
                        ray_count);
    }

    ret_t
    operator()(const Gallery::Emission&) const noexcept
    {
        return {RGB{Gallery::Emission::value}, ray_count};
    }

    ret_t
    operator()(const Gallery::Lambert& mat) const noexcept
    {
        return lambert(mat.color);
    }

    ret_t
    operator()(const Gallery::Glass& /* mat */) const noexcept
    {
        const float cos_theta = dot(n, -i);
        const bool  into      = cos_theta > 0;
        const Vec3f nfn       = into ? n : -n;
        const float eta       = compute_eta(Gallery::Glass::ior, into);
        const float f         = reflectance(cos_theta, eta);
        const float ddn       = dot(i, nfn);
        const float cos2t     = 1 - eta * eta * (1 - ddn * ddn);

        auto refraction = [&]() noexcept -> ret_t
        {
            const auto [L, rc] =
                radiance(max_depth,
                         bias_ray(p, refract(nfn, i, eta, ddn, cos2t)),
                         bump_ctr(c, ctr_incr),
                         depth);
            return {L / (eta * eta), rc};
        };

        if(cos2t < 0) // total internal reflection
            return reflection();

        const auto [sx, _] = threefry2x32r32<key[0], key[1]>(c);

        if(gen_canonical(sx) < f)
            return reflection();
        else
            return refraction();
    }

    ret_t
    operator()(const Gallery::Plastic&) const noexcept
    {
        const Vec3f wi        = -i;
        const float cos_theta = dot(n, wi);
        const bool  into      = cos_theta > 0;
        const float eta       = compute_eta(Gallery::Plastic::ior, into);
        const float f         = reflectance(cos_theta, eta);

        const auto [sx, _] = threefry2x32r32<key[0], key[1]>(c);

        if(gen_canonical(sx) < f)
            return reflection();
        else
            return lambert(Gallery::Plastic::color);
    }

    ret_t
    operator()(const Gallery::Metal&) const noexcept
    {
        const Vec3f wi    = -i;
        const Vec2f alpha = Gallery::Metal::alpha;
        const Vec3f b     = cross(n, t);

        const auto [wmx, wmy, wmz] =
            microfacet(wi, alpha, sample_u(threefry2x32r32<key[0], key[1]>(c)));
        const Vec3f wm = normalize(t * wmx + b * wmy + n * wmz);
        const Vec3f wo = reflect(wm, i, dot(wm, wi));

        const float dotWgWm = dot(n, wm);
        const float dotWgWo = dot(n, wo);

        if(dotWgWo < 0) return {RGB{0}, ray_count};

        const float alpha_n = anisorough_n(wm, n, t, b, dotWgWm, alpha);
        const float G1      = gaf(dotWgWm, alpha_n);

        auto cc = bump_ctr(c, ctr_incr);
        auto [L, rc] =
            radiance(max_depth, bias_ray(p, wo), cc, depth, ray_count);
        L *= G1;

        cc                = bump_ctr(cc, ctr_incr);
        const auto [r, _] = threefry2x32r32<key[0], key[1]>(cc);
        if(gen_canonical(r) < p_nee)
        {
            const auto [dr, drc] = direct(p, n, bump_ctr(cc, ctr_incr));
            rc += drc;
            if(dr)
            {
                const auto [p_l, _, l] = dr.value();
                const Vec3f wm         = normalize(wi + l);
                const float dotWgWm    = dot(n, wm);
                const float alpha_n = anisorough_n(wm, n, t, b, dotWgWm, alpha);
                const float alpha_i = anisorough_i(wi, n, t, alpha);
                const float f       = gtr(dotWgWm, alpha_n);
                const float dotWgWi = dot(n, wi);
                L += Scene::Gallery::Emission::value * f *
                     gaf(dotWgWi, alpha_i) * gaf(dotWgWm, alpha_n) /
                     (4 * dotWgWi * p_nee * p_l);
            }
        }

        return {Gallery::Metal::color * L, rc};
    }

    ret_t
    operator()(const Gallery::Window&) const noexcept
    {
        const Vec3f wi        = -i;
        const float cos_theta = dot(n, wi);
        const bool  into      = cos_theta > 0;
        const float eta       = compute_eta(Gallery::Window::ior, into);
        const float f         = reflectance(cos_theta, eta);

        const auto [sx, sy] = sample_u(threefry2x32r32<key[0], key[1]>(c));

        if(sx < f || sy < 0.4)
            return reflection();
        else
            return {RGB{0}, ray_count};
    }
};

class RenderPixel
{
    static constexpr unsigned min_samples = 64;

    const Camera cam = Scene::camera();

    const unsigned             m_width;
    const ToSampleSpace<float> m_sample_space;
    const float                m_pw; // pixel width
    const float                m_threshold;
    const unsigned             m_max_depth;

  public:
    RenderPixel(const Vec2u&   res,
                const float    threshold,
                const unsigned max_depth,
                const float    blur) noexcept :
        m_width(res[0]), m_sample_space(res),
        m_pw(blur * m_sample_space.ratio / m_sample_space.res[0]),
        m_threshold(threshold), m_max_depth(max_depth)
    {
    }

    template <typename Pixel>
    std::pair<std::size_t, std::size_t>
    operator()(Pixel val) const noexcept
    {
        auto [pos, pix]          = val;
        const auto [px, py]      = pos;
        const Vec2f         pp   = m_sample_space(pos); // pixel position
        const std::uint32_t c0   = px * m_width + py;
        unsigned            ns   = 0;
        unsigned            prc  = 0; // pixel ray count
        float               mean = .0f;
        float               m2   = .0f;

        RGB r{};

        while(ns < min_samples ||
              std::sqrt(m2 / static_cast<float>(ns - 1)) / ns >= m_threshold)
        {
            const auto [v, rc] = Shading::radiance(
                m_max_depth,
                cam(jitter_sample_pos(m_pw,
                                      pp,
                                      sample_u(threefry2x32r32<key[0], key[1]>(
                                          Counter{c0, ns})))),
                Counter{c0, ns + min_samples + 1001});
            const float x = std::min(luminance(v), 1.f);
            ++ns;
            const float delta = x - mean;
            mean += delta / ns;
            const float delta2 = x - mean;
            m2 += delta * delta2;

            r += v;
            prc += rc;
        }

        pix = RGBA{r / static_cast<float>(ns), 1};

        return {1, prc};
    }
};
} // namespace loom
