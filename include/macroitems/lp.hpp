#pragma once

#include "core.hpp"

#include <vector>

namespace macroitems {

/// Standard-form LP consumed by the HiGHS wrapper:
/// maximize c^T x subject to A x <= b and x >= 0.
struct LinearProgram {
    /// Constraint matrix A.
    std::vector<std::vector<long double>> A;
    /// Right-hand side vector b.
    std::vector<long double> b;
    /// Objective coefficient vector c.
    std::vector<long double> c;
};

/// Solve a standard-form maximization LP with HiGHS.
/// The wrapper keeps the rest of the code independent of HiGHS data structures:
/// callers build a simple dense-row model, and this function converts it to the
/// sparse column-wise model expected by HiGHS.
LpSolution solve_max_lp(const LinearProgram& lp);

}  // namespace macroitems
