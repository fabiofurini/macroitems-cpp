#pragma once

#include "core.hpp"

namespace macroitems {

/// Build the primal LP solution induced by a macroitem sequence and capacity.
///
/// Positive-ratio macroitems are packed in sequence order. Macroitems before
/// the split get value 1, the split macroitem gets one common fractional value,
/// and the rest get 0. Nonpositive-ratio macroitems are skipped because the LP
/// capacity constraint is <= c and need not be filled.
PrimalSolution primal_from_macroitems(const Instance& instance,
                                      const MacroSequence& sequence,
                                      long double capacity);

}  // namespace macroitems
