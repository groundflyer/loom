// -*- C++ -*-
// utils.hpp
//

#pragma once

#include "../common/typedefs.hpp"
#include "enc.hpp"
#include "pt.hpp"

#include <common/functional.hpp>
#include <vm/bse.hpp>
#include <vm/simd.hpp>

namespace loom
{
static std::uint32_t
encode_res(const Vec2u& res) noexcept
{
    const auto [w, h] = res;
    return w << 16 | h;
}

static inline int FORCE_INLINE
masked_ray_count(const vm::simd::native_vector<std::uint32_t> vrc,
                 const std::size_t tail_size) noexcept
{
#if defined(__AVX512F__)
    return _mm512_mask_reduce_add_epi32(static_cast<std::uint16_t>(-1)
                                            << tail_size,
                                        vrc);
#else // AVX or AVX2
    static constexpr std::size_t   N = vm::simd::native_vector_extent_v<float>;
    static constexpr std::uint32_t l = static_cast<std::uint32_t>(-1);
    static constexpr vm::simd::native_vector<std::uint32_t> masks[8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {l, 0, 0, 0, 0, 0, 0, 0},
        {l, l, 0, 0, 0, 0, 0, 0},
        {l, l, l, 0, 0, 0, 0, 0},
        {l, l, l, l, 0, 0, 0, 0},
        {l, l, l, l, l, 0, 0, 0},
        {l, l, l, l, l, l, l, 0},
        {l, l, l, l, l, l, l, l}
    };

    return __builtin_reduce_add(vrc & masks[N - tail_size]);
#endif
}

using pt_float = vm::pt<float>;

class OutputPixelConverter
{
    std::size_t& r_rc;

  public:
    OutputPixelConverter(std::size_t& rc) noexcept : r_rc(rc) {}

    RGBA<>
    operator()(const vm::output_type<pt_float>& o) const noexcept
    {
        const auto [r, g, b, rc, _] = o;
        r_rc += rc;
        return RGBA{r, g, b, 1};
    }
};

class InputEncoder
{
    std::uint32_t m_res;
    float         m_t;
    unsigned      m_d;
    float         m_b;

  public:
    InputEncoder(const Vec2u&   res,
                 const float    t,
                 const unsigned d,
                 const float    b) noexcept :
        m_res(encode_res(res)), m_t(t), m_d(d), m_b(b)
    {
    }

    template <typename Ip>
    vm::input_type<pt_float>
    operator()(const Ip ip) const noexcept
    {
        return {encode_pp(ip.first), m_res, m_t, m_d, m_b};
    }
};

struct RenderPixelBase
{
    unsigned w;
    unsigned h;
    unsigned res;
    float    threshold;
    unsigned max_depth;
    float    blur;

    RenderPixelBase(const Vec2u& img_res,
                    float        _threshold,
                    unsigned     _max_depth,
                    float        _blur) noexcept :
        w(img_res[0]), h(img_res[1]), res(encode_res(img_res)),
        threshold(_threshold), max_depth(_max_depth), blur(_blur)
    {
    }
};

struct RenderPixelBSE : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    template <typename Pixel>
    std::pair<std::size_t, std::size_t>
    operator()(Pixel val) const noexcept
    {
        vm::bse::function<pt_float> f;

        auto [pos, pix] = val;

        const auto [r, g, b, rc, _] =
            f(encode_pp(pos), res, threshold, max_depth, blur);

        pix = RGBA{r, g, b, 1};

        return {1, rc};
    }
};

using vft = vm::simd::native_vector<float>;
using vut = vm::simd::native_vector<std::uint32_t>;

struct RenderPixelSIMDPredicated : RenderPixelBase
{
    using RenderPixelBase::RenderPixelBase;

    static constexpr std::size_t N = vm::simd::native_vector_extent_v<float>;

    template <typename Chunk>
    std::pair<std::size_t, std::size_t>
    operator()(Chunk view) const noexcept
    {
#ifdef USE_CLD
        vm::simd::compact_function<pt_float, false, false> f;
#else
        vm::simd::compact_function<pt_float, false, true> f;
#endif

        const vut vres = vm::simd::broadcast<N>(res);
        const vft vt   = vm::simd::broadcast<N>(threshold);
        const vut vd   = vm::simd::broadcast<N>(max_depth);
        const vft vbl  = vm::simd::broadcast<N>(blur);
        vut       vpp;

        const std::size_t vs = std::ranges::size(view);

        const bool        is_tail   = N != vs;
        const std::size_t tail_size = N - vs;

        if(is_tail)
        {
            for(std::size_t i = 0; i < vs; ++i)
                vpp[i] = encode_pp(view[i].first);

            // guarantee tail elems to complete with the first element
            for(std::size_t i = tail_size; i < N; ++i) vpp[i] = vpp[0];
        }
        else
        {
            for(std::size_t i = 0; i < N; ++i)
                vpp[i] = encode_pp(view[i].first);
        }

        const auto [vr, vg, vb, vrc, _] = f(vpp, vres, vt, vd, vbl);

        for(std::size_t i = 0; i < vs; ++i)
            view[i].second = RGBA{vr[i], vg[i], vb[i], 1};

        return {vs,
                is_tail ? masked_ray_count(vrc, tail_size) :
                          __builtin_reduce_add(vrc)};
    }
};
} // namespace loom
