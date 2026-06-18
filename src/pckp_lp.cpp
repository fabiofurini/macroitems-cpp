#include "macroitems/pckp_lp.hpp"
#include "macroitems/lp.hpp"

namespace macroitems {

LpSolution solve_pckp_lp_relaxation(const Instance& instance, long double capacity) {
    LinearProgram lp;
    lp.c = instance.profit;

    std::vector<long double> capacity_row(instance.n, 0.0L);
    for (int i = 0; i < instance.n; ++i) capacity_row[i] = instance.weight[i];
    lp.A.push_back(capacity_row);
    lp.b.push_back(capacity);

    for (const auto& a : instance.arcs) {
        std::vector<long double> row(instance.n, 0.0L);
        row[a.tail] = 1.0L;
        row[a.head] = -1.0L;
        lp.A.push_back(row);
        lp.b.push_back(0.0L);
    }

    for (int i = 0; i < instance.n; ++i) {
        std::vector<long double> row(instance.n, 0.0L);
        row[i] = 1.0L;
        lp.A.push_back(row);
        lp.b.push_back(1.0L);
    }

    return solve_max_lp(lp);
}

}  // namespace macroitems
