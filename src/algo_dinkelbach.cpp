#include "macroitems/algo_dinkelbach.hpp"
#include "macroitems/instance.hpp"
#include "internal/closure_lp.hpp"
#include "internal/work_graph.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace macroitems {

DinkelbachMacroitemResult compute_next_macroitem_dinkelbach(
    const Instance& instance,
    const std::vector<int>& fixed_prefix_nodes,
    long double tolerance,
    int max_iterations) {
    if (tolerance <= 0.0L)
        throw std::invalid_argument("Dinkelbach tolerance must be positive");
    if (max_iterations <= 0)
        throw std::invalid_argument("Dinkelbach max_iterations must be positive");

    const NodeMask prefix = nodes_to_mask(instance.n, fixed_prefix_nodes);
    if (!is_closed_mask(instance, prefix))
        throw std::invalid_argument("fixed prefix must be precedence-closed");
    if (all_selected(prefix))
        throw std::invalid_argument("fixed prefix already contains all nodes");

    long double min_ratio = std::numeric_limits<long double>::infinity();
    long double max_ratio = -std::numeric_limits<long double>::infinity();
    for (int i = 0; i < instance.n; ++i) {
        if (!prefix[i]) {
            const long double r = instance.profit[i] / instance.weight[i];
            min_ratio = std::min(min_ratio, r);
            max_ratio = std::max(max_ratio, r);
        }
    }
    const long double span = std::max(1.0L, max_ratio - min_ratio);
    long double lambda = min_ratio - span;
    NodeMask best_mask = prefix;
    std::vector<DinkelbachIteration> iterations;

    for (int iter = 0; iter < max_iterations; ++iter) {
        const auto sol = solve_lagrangian_closure_lp(instance, prefix, lambda, 0.0L);
        const NodeMask mask = closure_solution_to_mask(instance, sol, prefix);
        const auto [p, w] = incremental_profit_weight(instance, prefix, mask);
        const long double residual = p - lambda * w;
        iterations.push_back({lambda, p, w, residual});

        if (w <= kEps) break;
        const long double next_lambda = p / w;
        best_mask = mask;
        if (residual <= tolerance) {
            lambda = next_lambda;
            break;
        }
        lambda = next_lambda;
    }

    // Dinkelbach identifies the optimal ratio. A second LP selects the largest
    // support among all closures with that same reduced-profit value.
    NodeMask next = solve_largest_support_closure_at_ratio(instance, prefix, lambda);
    if (next == prefix && best_mask != prefix) next = best_mask;
    if (next == prefix) throw std::runtime_error("Dinkelbach did not extend the prefix");

    // Build diff mask (next \ prefix).
    NodeMask diff(instance.n, false);
    for (int i = 0; i < instance.n; ++i) diff[i] = next[i] && !prefix[i];

    auto macro = make_macroitem(instance, mask_to_nodes(diff));
    return {macro, mask_to_nodes(next), macro.ratio(), iterations};
}

MacroSequence compute_macroitems_lagrangian_dinkelbach(const Instance& instance) {
    NodeMask prefix_mask = empty_mask(instance.n);
    std::vector<int> prefix_nodes;
    MacroSequence sequence;
    while (!all_selected(prefix_mask)) {
        auto step = compute_next_macroitem_dinkelbach(instance, prefix_nodes);
        sequence.items.push_back(step.macroitem);
        prefix_nodes = step.new_prefix_nodes;
        prefix_mask = nodes_to_mask(instance.n, prefix_nodes);
    }
    return sequence;
}

}  // namespace macroitems
