#pragma once

#include "core.hpp"

#include <vector>

namespace macroitems {

/// Compute the next macroitem with Dinkelbach's algorithm.
///
/// fixed_prefix_nodes must be a precedence-closed set already selected by
/// previous macroitems. The method repeatedly solves the Lagrangian closure LP
/// max (p-lambda w)^T x with the prefix fixed, updates lambda to the ratio of
/// the incremental closure, and finishes with a tie-breaking LP for largest
/// support.
DinkelbachMacroitemResult compute_next_macroitem_dinkelbach(
    const Instance& instance,
    const std::vector<int>& fixed_prefix_nodes,
    long double tolerance = 1e-9L,
    int max_iterations = 100);

/// Compute the full macroitem sequence by repeatedly applying Dinkelbach.
/// This is the explicit Lagrangian/Dinkelbach implementation. It is slower
/// than the forest-specific algorithms, but it works as an independent
/// validation method on small/medium instances.
MacroSequence compute_macroitems_lagrangian_dinkelbach(const Instance& instance);

}  // namespace macroitems
