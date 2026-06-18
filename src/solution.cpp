#include "macroitems/solution.hpp"
#include <algorithm>

namespace macroitems {

PrimalSolution primal_from_macroitems(const Instance& instance,
                                       const MacroSequence& sequence,
                                       long double capacity) {
    PrimalSolution sol;
    sol.x.assign(instance.n, 0.0L);
    long double used = 0.0L;
    for (int r = 0; r < static_cast<int>(sequence.items.size()); ++r) {
        const auto& item = sequence.items[r];
        // Since capacity is an upper bound, filling it with nonpositive-ratio
        // macroitems can only fail to improve the LP objective.
        if (compare_ratio(item.ratio(), {0.0L, 1.0L}) <= 0) break;
        if (used + item.weight <= capacity + kEps) {
            for (int v : item.nodes) sol.x[v] = 1.0L;
            used += item.weight;
        } else {
            const long double frac = std::max(0.0L, (capacity - used) / item.weight);
            for (int v : item.nodes) sol.x[v] = frac;
            sol.split_macroitem = r;
            sol.split_fraction = frac;
            used = capacity;
            break;
        }
    }
    sol.used_capacity = used;
    for (int i = 0; i < instance.n; ++i) sol.objective += instance.profit[i] * sol.x[i];
    return sol;
}

}  // namespace macroitems
