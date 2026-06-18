#include "macroitems/result_writer.hpp"
#include "macroitems/instance.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace macroitems {

namespace {

const char* lp_status_string(LpSolution::Status status) {
    switch (status) {
        case LpSolution::Status::Optimal:
            return "optimal";
        case LpSolution::Status::Infeasible:
            return "infeasible";
        case LpSolution::Status::Unbounded:
            return "unbounded";
    }
    return "unknown";
}

std::string topology_string(const Instance& inst) {
    if (!is_forest(inst)) return "dag";
    const bool in_forest = is_in_tree_forest(inst);
    const bool out_forest = is_out_tree_forest(inst);
    if (in_forest && out_forest) return "path";
    if (in_forest) return "inforest";
    if (out_forest) return "outforest";
    return "forest";
}

void write_timing_fields(std::ostream& out, const RunTimings& timings, bool include_solution) {
    out << "time_load_ms " << timings.load_ms << '\n';
    out << "time_validation_ms " << timings.validation_ms << '\n';
    out << "time_algorithm_ms " << timings.algorithm_ms << '\n';
    if (include_solution) out << "time_solution_ms " << timings.solution_ms << '\n';
}

}  // namespace

void print_timings(const RunTimings& timings, bool include_solution) {
    std::cout << std::fixed << std::setprecision(3)
              << "[time] load:       " << timings.load_ms << " ms\n"
              << "[time] validation: " << timings.validation_ms << " ms\n"
              << "[time] algorithm:  " << timings.algorithm_ms << " ms\n";
    if (include_solution) {
        std::cout << "[time] solution:   " << timings.solution_ms << " ms\n";
    }
    std::cout << "[time] write:      " << timings.write_ms << " ms\n";
}

void write_macroitem_result_file(const std::string& path,
                                 const std::string& input_file,
                                 const std::string& algorithm,
                                 const Instance& inst,
                                 long double capacity,
                                 const MacroSequence& sequence,
                                 const PrimalSolution& primal,
                                 const RunTimings& timings) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot open output file: " + path);

    out << std::fixed << std::setprecision(10);
    out << "input_file " << input_file << '\n';
    out << "algorithm " << algorithm << '\n';
    out << "n " << inst.n << '\n';
    out << "arcs " << inst.arcs.size() << '\n';
    out << "topology " << topology_string(inst) << '\n';
    out << "capacity " << static_cast<double>(capacity) << '\n';
    out << "objective " << static_cast<double>(primal.objective) << '\n';
    out << "used_capacity " << static_cast<double>(primal.used_capacity) << '\n';
    out << "split_macroitem " << (primal.split_macroitem + 1) << '\n';
    out << "split_fraction " << static_cast<double>(primal.split_fraction) << '\n';
    write_timing_fields(out, timings, true);

    out << "\nmacroitems " << sequence.items.size() << '\n';
    for (int r = 0; r < static_cast<int>(sequence.items.size()); ++r) {
        const auto& m = sequence.items[r];
        out << "N_" << (r + 1) << " nodes";
        for (int v : m.nodes) out << ' ' << (v + 1);
        out << " profit " << static_cast<double>(m.profit)
            << " weight " << static_cast<double>(m.weight)
            << " ratio " << static_cast<double>(m.ratio().value()) << '\n';
    }

    out << "\nprimal_x";
    for (long double x : primal.x) out << ' ' << static_cast<double>(x);
    out << '\n';
}

void write_lp_result_file(const std::string& path,
                          const std::string& input_file,
                          const Instance& inst,
                          long double capacity,
                          const LpSolution& lp,
                          const RunTimings& timings) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot open output file: " + path);

    out << std::fixed << std::setprecision(10);
    out << "input_file " << input_file << '\n';
    out << "algorithm lp\n";
    out << "n " << inst.n << '\n';
    out << "arcs " << inst.arcs.size() << '\n';
    out << "topology " << topology_string(inst) << '\n';
    out << "capacity " << static_cast<double>(capacity) << '\n';
    out << "status " << lp_status_string(lp.status) << '\n';
    out << "objective " << static_cast<double>(lp.objective) << '\n';
    write_timing_fields(out, timings, false);

    if (lp.status == LpSolution::Status::Optimal) {
        out << "\nprimal_x";
        for (long double x : lp.primal) out << ' ' << static_cast<double>(x);
        out << '\n';
    }
}

void write_csv_run(const std::string& csv_path,
                   const std::string& input_file,
                   const std::string& algorithm,
                   const std::string& output_file,
                   std::optional<long double> user_capacity,
                   const Instance& inst,
                   const MacroSequence& sequence,
                   const PrimalSolution& primal,
                   const RunTimings& timings) {
    // Ensure directory exists.
    const auto parent = std::filesystem::path(csv_path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);

    const bool is_new = !std::filesystem::exists(csv_path);
    std::ofstream out(csv_path, std::ios::app);
    if (!out) throw std::runtime_error("cannot open CSV file: " + csv_path);

    // Header on first write.
    if (is_new) {
        out << "timestamp,instance,algorithm,output_file,capacity,"
               "n_nodes,n_arcs,topology,"
               "objective,used_capacity,"
               "n_macroitems,critical_macroitem,critical_macroitem_size,"
               "min_macroitem_size,max_macroitem_size,"
               "load_ms,validation_ms,algorithm_ms,solution_ms,write_ms\n";
    }

    // Timestamp (UTC).
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char ts[20];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::gmtime(&tt));

    // Macroitem statistics.
    const int nm = static_cast<int>(sequence.items.size());
    int min_sz = nm > 0 ? static_cast<int>(sequence.items[0].nodes.size()) : 0;
    int max_sz = min_sz;
    for (const auto& m : sequence.items) {
        const int sz = static_cast<int>(m.nodes.size());
        min_sz = std::min(min_sz, sz);
        max_sz = std::max(max_sz, sz);
    }
    const int crit_idx  = primal.split_macroitem;  // -1 if no split
    const int crit_size = (crit_idx >= 0 && crit_idx < nm)
                          ? static_cast<int>(sequence.items[crit_idx].nodes.size())
                          : 0;

    out << std::fixed << std::setprecision(6);
    out << ts << ','
        << input_file << ','
        << algorithm << ','
        << output_file << ',';
    if (user_capacity.has_value())
        out << static_cast<double>(*user_capacity);
    out << ','
        << inst.n << ','
        << inst.arcs.size() << ','
        << topology_string(inst) << ','
        << static_cast<double>(primal.objective) << ','
        << static_cast<double>(primal.used_capacity) << ','
        << nm << ','
        << (crit_idx >= 0 ? crit_idx + 1 : -1) << ','  // 1-based; -1 means no split
        << crit_size << ','
        << min_sz << ','
        << max_sz << ','
        << timings.load_ms << ','
        << timings.validation_ms << ','
        << timings.algorithm_ms << ','
        << timings.solution_ms << ','
        << timings.write_ms << '\n';
}

}  // namespace macroitems
