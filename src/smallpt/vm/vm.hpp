// -*- C++ -*-
// vm.hpp
//

#pragma once

#include "events.hpp"

namespace loom
{
bool
predicated(EventState& estate, ViewState& vstate);

bool
predicated_mp(EventState& estate, ViewState& vstate);

bool
hybrid(EventState& estate, ViewState& vstate);

bool
hybrid_mp(EventState&                     estate,
          ViewState&                      vstate,
          const std::chrono::milliseconds eg);
} // namespace loom
