#include "macroitems.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace macroitems;

namespace {

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

bool near(long double a, long double b, long double eps = 1e-7L) {
    return std::fabs(a - b) <= eps;
}

bool same_macro_sequence(const MacroSequence& a, const MacroSequence& b) {
    if (a.items.size() != b.items.size()) return false;
    for (int i = 0; i < static_cast<int>(a.items.size()); ++i) {
        auto an = a.items[i].nodes;
        auto bn = b.items[i].nodes;
        std::sort(an.begin(), an.end());
        std::sort(bn.begin(), bn.end());
        if (an != bn) return false;
        if (!near(a.items[i].profit, b.items[i].profit)) return false;
        if (!near(a.items[i].weight, b.items[i].weight)) return false;
    }
    return true;
}

Instance make_path_in_tree() {
    return {4, {10, 3, 8, 2}, {2, 1, 4, 1}, {{0, 1}, {2, 1}, {3, 2}}};
}

Instance make_path_out_tree() {
    return {4, {5, 4, 9, 1}, {1, 2, 3, 1}, {{0, 1}, {1, 2}, {1, 3}}};
}

void test_paper_example_dinkelbach() {
    const auto instance = make_paper_example();
    const auto expected = expected_paper_macroitems();
    const auto sequence = compute_macroitems_lagrangian_dinkelbach(instance);
    require(same_macro_sequence(sequence, expected), "Dinkelbach macroitems differ on paper example");

    const auto primal = primal_from_macroitems(instance, sequence, 4.0L);
    require(near(primal.objective, 7.0L), "paper primal objective should be 7");
    require(primal.split_macroitem == 1, "paper split macroitem should be N_2");
    require(near(primal.split_fraction, 0.5L), "paper split fraction should be 1/2");

    const auto lp = solve_pckp_lp_relaxation(instance, 4.0L);
    require(lp.status == LpSolution::Status::Optimal, "paper direct LP should be optimal");
    require(near(lp.objective, primal.objective), "direct LP objective differs from macroitem objective");
}

void test_forest_algorithms() {
    const auto in_instance = make_path_in_tree();
    const auto in_general = compute_forest_macroitems(in_instance);
    const auto in_noheap = compute_forest_macroitems_noheap(in_instance);
    const auto in_heap = compute_in_tree_macroitems_heap(in_instance);
    require(same_macro_sequence(in_general, in_noheap), "in-tree no-heap forest algorithm differs from heap forest algorithm");
    require(same_macro_sequence(in_general, in_heap), "in-tree heap differs from forest algorithm");

    const auto out_instance = make_path_out_tree();
    const auto out_general = compute_forest_macroitems(out_instance);
    const auto out_noheap = compute_forest_macroitems_noheap(out_instance);
    const auto out_heap = compute_out_tree_macroitems_heap(out_instance);
    require(same_macro_sequence(out_general, out_noheap), "out-tree no-heap forest algorithm differs from heap forest algorithm");
    require(same_macro_sequence(out_general, out_heap), "out-tree heap differs from forest algorithm");
}

void test_validation() {
    const auto paper = make_paper_example();
    require(is_dag(paper), "paper example should be a DAG");
    require(!is_forest(paper), "paper example should not be a forest");
    require(is_in_tree_forest(make_path_in_tree()), "path in-tree topology not recognized");
    require(is_out_tree_forest(make_path_out_tree()), "path out-tree topology not recognized");
}

}  // namespace

int main() {
    try {
        test_paper_example_dinkelbach();
        test_forest_algorithms();
        test_validation();
        std::cout << "All tests passed.\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
