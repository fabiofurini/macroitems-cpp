#pragma once

#include "core.hpp"

namespace macroitems {

/// Compute the optimal macroitem sequence on a directed forest.
///
/// This is the heap-based practical variant used for benchmarking.
MacroSequence compute_forest_macroitems(const Instance& instance);

/// Compute the optimal macroitem sequence on a directed forest without heaps.
///
/// This follows Algorithm 1 from the paper more closely: each iteration scans
/// the current final nodes and live arcs to find the best candidate. It keeps
/// the same incremental closure-sum updates as the heap-based implementation,
/// so ratios affected by removals/contractions are updated locally rather than
/// recomputing all closure sums from scratch.
MacroSequence compute_forest_macroitems_noheap(const Instance& instance);

}  // namespace macroitems
