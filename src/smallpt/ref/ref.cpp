// -*- C++ -*-
// fm.cpp
#include "ref.hpp"
#include "../common/render.hpp"
#include "pt.hpp"

namespace loom
{
bool
reference(EventState& estate, ViewState& vstate) noexcept
{
    return scanline<RenderPixel>(estate, vstate);
}

bool
reference_mp(EventState& estate, ViewState& vstate) noexcept
{
    return scanline_mp<RenderPixel>(estate, vstate);
}
} // namespace loom
