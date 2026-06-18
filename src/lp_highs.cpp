#include "macroitems/lp.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "Highs.h"
#include "lp_data/HighsLp.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <stdexcept>

namespace macroitems {

LpSolution solve_max_lp(const LinearProgram& lp) {
    // Check dimensions early so malformed model construction is reported as a
    // programming error rather than as an LP solve failure.
    if (lp.A.size() != lp.b.size()) throw std::invalid_argument("LP has inconsistent A/b dimensions");
    for (const auto& row : lp.A) {
        if (row.size() != lp.c.size()) throw std::invalid_argument("LP has inconsistent A/c dimensions");
    }

    const HighsInt num_row = static_cast<HighsInt>(lp.b.size());
    const HighsInt num_col = static_cast<HighsInt>(lp.c.size());

    // One fresh HiGHS object is used per solve. This keeps the wrapper simple
    // and avoids hidden state across the many small LPs solved by Dinkelbach.
    Highs highs;
    highs.setOptionValue("output_flag", false);
    highs.setOptionValue("log_to_console", false);

    // Translate our standard-form max problem into a HighsLp.
    HighsLp model;
    model.num_col_ = num_col;
    model.num_row_ = num_row;
    model.sense_ = ObjSense::kMaximize;
    model.offset_ = 0.0;
    model.col_cost_.assign(lp.c.begin(), lp.c.end());
    model.col_lower_.assign(num_col, 0.0);
    model.col_upper_.assign(num_col, highs.getInfinity());
    model.row_lower_.assign(num_row, -highs.getInfinity());
    model.row_upper_.assign(lp.b.begin(), lp.b.end());

    // HiGHS expects a sparse matrix in column-wise format.
    model.a_matrix_.format_ = MatrixFormat::kColwise;
    model.a_matrix_.num_col_ = num_col;
    model.a_matrix_.num_row_ = num_row;
    model.a_matrix_.start_.assign(num_col + 1, 0);
    for (HighsInt j = 0; j < num_col; ++j) {
        for (HighsInt i = 0; i < num_row; ++i) {
            const double value = static_cast<double>(lp.A[i][j]);
            if (value == 0.0) continue;
            model.a_matrix_.index_.push_back(i);
            model.a_matrix_.value_.push_back(value);
        }
        model.a_matrix_.start_[j + 1] = static_cast<HighsInt>(model.a_matrix_.index_.size());
    }

    const HighsStatus pass_status = highs.passModel(model);
    if (pass_status != HighsStatus::kOk && pass_status != HighsStatus::kWarning) {
        return {LpSolution::Status::Infeasible, {}, {}, -100.0L - static_cast<int>(pass_status)};
    }
    const HighsStatus run_status = highs.run();
    if (run_status != HighsStatus::kOk && run_status != HighsStatus::kWarning) {
        return {LpSolution::Status::Infeasible, {}, {}, -200.0L - static_cast<int>(run_status)};
    }

    const HighsModelStatus model_status = highs.getModelStatus();
    if (model_status == HighsModelStatus::kInfeasible)
        return {LpSolution::Status::Infeasible, {}, {}, 0.0L};
    if (model_status == HighsModelStatus::kUnbounded ||
        model_status == HighsModelStatus::kUnboundedOrInfeasible) {
        return {LpSolution::Status::Unbounded, {}, {}, 0.0L};
    }
    if (model_status != HighsModelStatus::kOptimal) {
        return {LpSolution::Status::Infeasible, {}, {},
                static_cast<long double>(static_cast<int>(model_status))};
    }

    const HighsSolution& highs_solution = highs.getSolution();
    LpSolution solution;
    solution.status = LpSolution::Status::Optimal;
    solution.primal.assign(highs_solution.col_value.begin(), highs_solution.col_value.end());
    solution.dual.assign(highs_solution.row_dual.begin(), highs_solution.row_dual.end());
    solution.objective = highs.getObjectiveValue();
    return solution;
}

}  // namespace macroitems
