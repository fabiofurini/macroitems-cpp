/// macroitems_main — command-line driver for the macroitem algorithms.
///
/// Usage:
///   macroitems_main -i <instance> -a <algorithm> [-c <capacity>]
///                   [-r <result_file>] [-g <graphml_file>]
///
///   -i  path to the instance file (required)
///   -a  algorithm: forest | forest_noheap | inforest | outforest | dinkelbach | lp (required)
///   -c  capacity: absolute value (e.g. 4) or percentage (e.g. 50%)
///       defaults to total item weight
///   -r  path for the text result file (optional; omit to skip writing)
///   -g  path for the yEd GraphML file (optional; omit to skip writing)

#include "macroitems.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>

// ---------------------------------------------------------------------------
// Helpers 
// ---------------------------------------------------------------------------

namespace {

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::duration<double, std::milli>;

double elapsed_ms(Clock::time_point t0) {
    return std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
}

double process_cpu_ms() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    const double user = static_cast<double>(usage.ru_utime.tv_sec) * 1000.0
                      + static_cast<double>(usage.ru_utime.tv_usec) / 1000.0;
    const double system = static_cast<double>(usage.ru_stime.tv_sec) * 1000.0
                        + static_cast<double>(usage.ru_stime.tv_usec) / 1000.0;
    return user + system;
}

std::string next_non_comment_line(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        if (line[first] == '#') continue;
        return line.substr(first);
    }
    return {};
}

macroitems::Instance load_instance(const std::string& path) {
    std::ifstream fin(path);
    if (!fin) throw std::runtime_error("cannot open input file: " + path);

    macroitems::Instance inst;
    {
        const auto line = next_non_comment_line(fin);
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token != "n") throw std::runtime_error("expected 'n <int>', got: " + line);
        if (!(ss >> inst.n) || inst.n <= 0) throw std::runtime_error("invalid n in: " + line);
    }
    {
        const auto line = next_non_comment_line(fin);
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token != "profits") throw std::runtime_error("expected 'profits ...', got: " + line);
        inst.profit.resize(inst.n);
        for (int i = 0; i < inst.n; ++i) {
            double value = 0.0;
            if (!(ss >> value)) throw std::runtime_error("not enough profits");
            inst.profit[i] = value;
        }
    }
    {
        const auto line = next_non_comment_line(fin);
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token != "weights") throw std::runtime_error("expected 'weights ...', got: " + line);
        inst.weight.resize(inst.n);
        for (int i = 0; i < inst.n; ++i) {
            double value = 0.0;
            if (!(ss >> value)) throw std::runtime_error("not enough weights");
            inst.weight[i] = value;
        }
    }
    {
        const auto line = next_non_comment_line(fin);
        std::istringstream ss(line);
        std::string token;
        int m = 0;
        ss >> token;
        if (token != "arcs") throw std::runtime_error("expected 'arcs <count>', got: " + line);
        if (!(ss >> m) || m < 0) throw std::runtime_error("invalid arc count in: " + line);
        inst.arcs.reserve(m);
        for (int i = 0; i < m; ++i) {
            const auto arc_line = next_non_comment_line(fin);
            std::istringstream as(arc_line);
            int tail = 0, head = 0;
            if (!(as >> tail >> head)) throw std::runtime_error("invalid arc line: " + arc_line);
            if (tail < 1 || tail > inst.n || head < 1 || head > inst.n)
                throw std::runtime_error("arc endpoint out of range: " + arc_line);
            inst.arcs.push_back({tail - 1, head - 1});
        }
    }
    return inst;
}

void check_compatibility(const macroitems::Instance& inst, std::string_view algo) {
    const bool is_macroitem =
        algo == "forest" || algo == "forest_noheap" ||
        algo == "inforest" || algo == "outforest" || algo == "dinkelbach";
    if (!is_macroitem && algo != "lp")
        throw std::runtime_error("unknown algorithm '" + std::string(algo) +
                                 "'. Valid: forest, forest_noheap, inforest, outforest, dinkelbach, lp");

    macroitems::validate_instance_or_throw(inst, false);
    if (algo == "forest" || algo == "forest_noheap" ||
        algo == "inforest" || algo == "outforest") {
        if (!macroitems::is_forest(inst))
            throw std::runtime_error("algorithm '" + std::string(algo) +
                                     "' requires a directed forest, but the input instance is not a forest");
    }
    if (algo == "inforest" && !macroitems::is_in_tree_forest(inst))
        throw std::runtime_error("algorithm 'inforest' requires a forest with out-degree <= 1");
    if (algo == "outforest" && !macroitems::is_out_tree_forest(inst))
        throw std::runtime_error("algorithm 'outforest' requires a forest with in-degree <= 1");
    if (algo == "dinkelbach") {
        if (!macroitems::is_dag(inst)) throw std::runtime_error("algorithm 'dinkelbach' requires a DAG");
    }
}

macroitems::MacroSequence run_algorithm(const macroitems::Instance& inst, std::string_view algo) {
    if (algo == "forest")        return macroitems::compute_forest_macroitems(inst);
    if (algo == "forest_noheap") return macroitems::compute_forest_macroitems_noheap(inst);
    if (algo == "inforest")      return macroitems::compute_in_tree_macroitems_heap(inst);
    if (algo == "outforest")     return macroitems::compute_out_tree_macroitems_heap(inst);
    if (algo == "dinkelbach")    return macroitems::compute_macroitems_lagrangian_dinkelbach(inst);
    throw std::logic_error("unreachable algorithm dispatch");
}

long double total_weight(const macroitems::Instance& inst) {
    long double total = 0.0L;
    for (long double w : inst.weight) total += w;
    return total;
}

long double parse_capacity(const std::string& arg, long double total) {
    if (!arg.empty() && arg.back() == '%') {
        const double pct = std::stod(arg.substr(0, arg.size() - 1));
        if (pct <= 0.0 || pct > 100.0) throw std::runtime_error("percentage capacity must be in (0, 100]");
        return total * static_cast<long double>(pct) / 100.0L;
    }
    const double cap = std::stod(arg);
    if (cap < 0.0) throw std::runtime_error("capacity must be nonnegative");
    return cap;
}

}  // namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // Parse flag-based arguments.
    std::string input_file, algorithm, output_file, graphml_file, cap_arg;
    for (int i = 1; i < argc; ++i) {
        std::string_view flag = argv[i];
        auto need_arg = [&](std::string& dest) {
            if (++i >= argc) {
                std::cerr << "Error: flag " << flag << " requires an argument\n";
                std::exit(1);
            }
            dest = argv[i];
        };
        if      (flag == "-i") need_arg(input_file);
        else if (flag == "-a") need_arg(algorithm);
        else if (flag == "-c") need_arg(cap_arg);
        else if (flag == "-r") need_arg(output_file);
        else if (flag == "-g") need_arg(graphml_file);
        else {
            std::cerr << "Error: unknown flag '" << flag << "'\n";
            return 1;
        }
    }
    if (input_file.empty() || algorithm.empty()) {
        std::cerr << "Usage: " << argv[0]
                  << " -i <instance> -a <algorithm> [-c <capacity>]"
                  << " [-r <result_file>] [-g <graphml_file>]\n"
                  << "Algorithms: forest | forest_noheap | inforest | outforest | dinkelbach | lp\n";
        return 1;
    }

    try {
        macroitems::RunTimings timings;

        auto t0 = Clock::now();
        const auto inst = load_instance(input_file);
        timings.load_ms = elapsed_ms(t0);

        t0 = Clock::now();
        check_compatibility(inst, algorithm);
        const bool capacity_given = !cap_arg.empty();
        const long double capacity =
            capacity_given ? parse_capacity(cap_arg, total_weight(inst))
                           : total_weight(inst);
        timings.validation_ms = elapsed_ms(t0);

        if (algorithm == "lp") {
            const double cpu0 = process_cpu_ms();
            const auto lp = macroitems::solve_pckp_lp_relaxation(inst, capacity);
            timings.algorithm_ms = process_cpu_ms() - cpu0;

            t0 = Clock::now();
            if (!output_file.empty())
                macroitems::write_lp_result_file(output_file, input_file, inst, capacity, lp, timings);
            timings.write_ms = elapsed_ms(t0);

            macroitems::print_timings(timings, false);
            if (!output_file.empty()) std::cout << "Result written to " << output_file << '\n';
            return 0;
        }

        const double cpu0 = process_cpu_ms();
        const auto sequence = run_algorithm(inst, algorithm);
        timings.algorithm_ms = process_cpu_ms() - cpu0;

        t0 = Clock::now();
        const auto primal = macroitems::primal_from_macroitems(inst, sequence, capacity);
        timings.solution_ms = elapsed_ms(t0);

        t0 = Clock::now();
        if (!output_file.empty())
            macroitems::write_macroitem_result_file(
                output_file, input_file, algorithm, inst, capacity, sequence, primal, timings);
        if (!graphml_file.empty())
            macroitems::write_yed_graphml_file(graphml_file, inst, sequence);
        macroitems::write_csv_run(
            "results/runs.csv", input_file, algorithm,
            output_file.empty() ? "" : output_file,
            capacity_given ? std::optional<long double>{capacity} : std::nullopt,
            inst, sequence, primal, timings);
        timings.write_ms = elapsed_ms(t0);

        macroitems::print_timings(timings, true);
        if (!output_file.empty()) std::cout << "Result written to " << output_file << '\n';
        if (!graphml_file.empty()) std::cout << "yEd written to    " << graphml_file << '\n';
        std::cout << "CSV log:          results/runs.csv\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
