#pragma once

#include "core.hpp"

namespace macroitems {

/// Compute macroitems on a forest of out-trees using a min-heap.
///
/// The algorithm extracts macroitems in increasing ratio order using a min-heap
/// over current node ratios, then reverses the list before returning it.
/// Complexity is O(n log n).
MacroSequence compute_out_tree_macroitems_heap(const Instance& instance);

}  // namespace macroitems
