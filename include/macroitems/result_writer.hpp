#pragma once

#include "core.hpp"

#include <optional>
#include <string>

namespace macroitems {

struct RunTimings {
    double load_ms = 0.0;
    double validation_ms = 0.0;
    double algorithm_ms = 0.0;
    double solution_ms = 0.0;
    double write_ms = 0.0;
};

void print_timings(const RunTimings& timings, bool include_solution);

void write_macroitem_result_file(const std::string& path,
                                 const std::string& input_file,
                                 const std::string& algorithm,
                                 const Instance& instance,
                                 long double capacity,
                                 const MacroSequence& sequence,
                                 const PrimalSolution& primal,
                                 const RunTimings& timings);

void write_lp_result_file(const std::string& path,
                          const std::string& input_file,
                          const Instance& instance,
                          long double capacity,
                          const LpSolution& lp,
                          const RunTimings& timings);

/// Append one row to a CSV performance log (created with header if new).
/// Written to <csv_path>; caller is responsible for the directory existing.
void write_csv_run(const std::string& csv_path,
                   const std::string& input_file,
                   const std::string& algorithm,
                   const std::string& output_file,
                   std::optional<long double> user_capacity,
                   const Instance& instance,
                   const MacroSequence& sequence,
                   const PrimalSolution& primal,
                   const RunTimings& timings);

}  // namespace macroitems
