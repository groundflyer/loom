// -*- C++ -*-
// ref.hpp
//

#pragma once

#include <smallpt/vm/events.hpp>

namespace loom
{
bool
reference(EventState& estate, ViewState& vstate) noexcept;

bool
reference_mp(EventState& estate, ViewState& vstate) noexcept;
} // namespace loom
