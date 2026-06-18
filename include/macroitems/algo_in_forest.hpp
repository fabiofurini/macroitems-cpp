#pragma once

#include "core.hpp"

namespace macroitems {

/// Compute macroitems on a forest of in-trees using a max-heap.
///
/// In an in-tree every node has at most one outgoing arc, hence F_(i,j)={i}.
/// The largest candidate ratio is therefore the largest current node ratio.
/// Complexity is O(n log n). The returned sequence is in decreasing ratio order.
MacroSequence compute_in_tree_macroitems_heap(const Instance& instance);

}  // namespace macroitems
