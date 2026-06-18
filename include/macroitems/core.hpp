#pragma once

#include <vector>

namespace macroitems {

constexpr long double kEps = 1e-10L;

/// Fraction represented as numerator/denominator.
/// We keep ratios in this form so comparisons can use cross multiplication
/// instead of dividing early and amplifying numerical noise.
struct Ratio {
    long double num = 0.0L;
    long double den = 1.0L;

    /// Return the floating-point value of the ratio.
    long double value() const { return num / den; }
};

/// Compare two ratios by cross multiplication.
/// Returns +1 if a>b, -1 if a<b, and 0 if they are equal up to kEps.
inline int compare_ratio(const Ratio& a, const Ratio& b) {
    const long double lhs = a.num * b.den;
    const long double rhs = b.num * a.den;
    if (lhs > rhs + kEps) return 1;
    if (lhs + kEps < rhs) return -1;
    return 0;
}

/// Directed precedence arc (tail, head), meaning x_tail <= x_head.
struct Arc {
    int tail = -1;
    int head = -1;
};

/// Complete PCKP instance.
/// Nodes are indexed from 0 to n-1 internally; printing functions convert to
/// 1-based indices to match the paper.
struct Instance {
    int n = 0;
    std::vector<long double> profit;
    std::vector<long double> weight;
    std::vector<Arc> arcs;
};

/// One macroitem: a subset of original nodes and its aggregate profit/weight.
struct Macroitem {
    std::vector<int> nodes;
    long double profit = 0.0L;
    long double weight = 0.0L;

    /// Aggregate profit-to-weight ratio P(M)/W(M).
    Ratio ratio() const { return {profit, weight}; }
};

/// Ordered optimal sequence of macroitems.
struct MacroSequence {
    std::vector<Macroitem> items;
};

/// Primal LP solution induced by an optimal macroitem sequence.
struct PrimalSolution {
    /// LP variables x_j.
    std::vector<long double> x;
    /// Objective value p^T x.
    long double objective = 0.0L;
    /// Total consumed capacity w^T x.
    long double used_capacity = 0.0L;
    /// Index of the fractional macroitem, or -1 if no split is needed.
    int split_macroitem = -1;
    /// Common fractional value assigned to the split macroitem.
    long double split_fraction = 0.0L;
};

/// Result returned by the internal LP solver.
struct LpSolution {
    enum class Status { Optimal, Infeasible, Unbounded };
    Status status = Status::Infeasible;
    /// Optimal primal vector when status == Optimal.
    std::vector<long double> primal;
    /// Dual vector for the standard-form inequalities when available.
    std::vector<long double> dual;
    /// Optimal objective value.
    long double objective = 0.0L;
};

/// One iteration of the Dinkelbach/Lagrangian macroitem search.
struct DinkelbachIteration {
    /// Current multiplier lambda used in max (p-lambda w)^T x.
    long double lambda = 0.0L;
    /// Profit of the incremental closure selected at this iteration.
    long double incremental_profit = 0.0L;
    /// Weight of the incremental closure selected at this iteration.
    long double incremental_weight = 0.0L;
    /// Dinkelbach residual P-lambda W. Convergence is reached when <= tolerance.
    long double residual = 0.0L;
};

/// Result of one Dinkelbach call for the next macroitem after a fixed prefix.
struct DinkelbachMacroitemResult {
    /// Macroitem found after the fixed prefix.
    Macroitem macroitem;
    /// New closed prefix, represented as original node ids.
    std::vector<int> new_prefix_nodes;
    /// Final ratio P(M)/W(M), equal to the macroitem breakpoint.
    Ratio ratio;
    /// Iteration log useful for debugging and experiments.
    std::vector<DinkelbachIteration> iterations;
};

}  // namespace macroitems
