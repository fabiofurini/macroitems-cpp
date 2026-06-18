#pragma once

#include "core.hpp"

namespace macroitems {

/// Solve the natural LP relaxation of the PCKP with HiGHS.
///
/// The caller is responsible for validating the instance before timing this
/// routine when clean algorithm timings are required.
///
/// max p^T x, wx <= c, x_i - x_j <= 0 for each precedence arc, and 0 <= x <= 1.
LpSolution solve_pckp_lp_relaxation(const Instance& instance, long double capacity);

}  // namespace macroitems
