#pragma once

#include "core.hpp"

#include <iosfwd>

namespace macroitems {

/// Return true if the underlying undirected graph is a forest.
bool is_forest(const Instance& instance);
/// Return true if the directed graph has no directed cycle.
bool is_dag(const Instance& instance);
/// Return true if this is a forest of in-trees, i.e., out-degree <= 1.
bool is_in_tree_forest(const Instance& instance);
/// Return true if this is a forest of out-trees, i.e., in-degree <= 1.
bool is_out_tree_forest(const Instance& instance);

/// Validate dimensions, positive weights, arc endpoints, duplicated arcs,
/// acyclicity and optionally the forest property.
/// Throws std::invalid_argument with a readable error when validation fails.
void validate_instance_or_throw(const Instance& instance, bool require_forest);

/// Return the 8-node example from Figure 1 of the paper.
/// Its underlying undirected graph is not a forest, so it is used for LP and
/// primal/dual validation rather than for the forest algorithm.
Instance make_paper_example();

/// Return the expected optimal macroitem sequence for make_paper_example().
MacroSequence expected_paper_macroitems();

/// Print a human-readable macroitem sequence with 1-based node ids.
std::ostream& operator<<(std::ostream& os, const MacroSequence& sequence);

}  // namespace macroitems
