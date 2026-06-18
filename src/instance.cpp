#include "macroitems/instance.hpp"
#include "internal/work_graph.hpp"

#include <iomanip>
#include <numeric>
#include <ostream>
#include <queue>
#include <set>
#include <stdexcept>

namespace macroitems {

bool is_dag(const Instance& instance) {
    // Kahn's algorithm: remove zero-indegree vertices until none remain.
    auto out = outgoing(instance);
    std::vector<int> indeg(instance.n, 0);
    for (const auto& a : instance.arcs) ++indeg[a.head];
    std::queue<int> q;
    for (int i = 0; i < instance.n; ++i) {
        if (indeg[i] == 0) q.push(i);
    }
    int seen = 0;
    while (!q.empty()) {
        const int u = q.front();
        q.pop();
        ++seen;
        for (int v : out[u]) {
            if (--indeg[v] == 0) q.push(v);
        }
    }
    return seen == instance.n;
}

bool is_forest(const Instance& instance) {
    // Union-find over the undirected version detects exactly whether an edge
    // closes a cycle. Direction is irrelevant for the forest property.
    std::vector<int> parent(instance.n);
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    for (const auto& a : instance.arcs) {
        int ru = find(a.tail);
        int rv = find(a.head);
        if (ru == rv) return false;
        parent[ru] = rv;
    }
    return true;
}

bool is_in_tree_forest(const Instance& instance) {
    if (!is_forest(instance) || !is_dag(instance)) return false;
    std::vector<int> out_degree(instance.n, 0);
    for (const auto& a : instance.arcs) {
        if (++out_degree[a.tail] > 1) return false;
    }
    return true;
}

bool is_out_tree_forest(const Instance& instance) {
    if (!is_forest(instance) || !is_dag(instance)) return false;
    std::vector<int> in_degree(instance.n, 0);
    for (const auto& a : instance.arcs) {
        if (++in_degree[a.head] > 1) return false;
    }
    return true;
}

void validate_instance_or_throw(const Instance& instance, bool require_forest) {
    if (instance.n <= 0 || static_cast<int>(instance.profit.size()) != instance.n ||
        static_cast<int>(instance.weight.size()) != instance.n) {
        throw std::invalid_argument("invalid instance dimensions");
    }
    for (long double w : instance.weight) {
        if (w <= 0.0L) throw std::invalid_argument("all weights must be positive");
    }
    std::set<std::pair<int, int>> seen;
    for (const auto& a : instance.arcs) {
        if (a.tail < 0 || a.tail >= instance.n || a.head < 0 || a.head >= instance.n || a.tail == a.head) {
            throw std::invalid_argument("invalid arc endpoint");
        }
        if (!seen.insert({a.tail, a.head}).second) throw std::invalid_argument("duplicate arc");
    }
    if (!is_dag(instance)) throw std::invalid_argument("precedence graph must be acyclic");
    if (require_forest && !is_forest(instance))
        throw std::invalid_argument("forest macroitem algorithm requires an undirected forest");
}

Instance make_paper_example() {
    return {8,
            {1, -1, -1, -1, 6, 5, 2, 3},
            {1, 1, 1, 2, 2, 1, 1, 1},
            {{4, 0}, {4, 1}, {4, 5}, {5, 2}, {6, 2}, {6, 3}, {7, 4}, {7, 5}, {7, 6}}};
}

MacroSequence expected_paper_macroitems() {
    const auto instance = make_paper_example();
    return {{make_macroitem(instance, {2, 5}),
             make_macroitem(instance, {0, 1, 4}),
             make_macroitem(instance, {3, 6, 7})}};
}

std::ostream& operator<<(std::ostream& os, const MacroSequence& sequence) {
    os << std::fixed << std::setprecision(6);
    for (int i = 0; i < static_cast<int>(sequence.items.size()); ++i) {
        const auto& m = sequence.items[i];
        os << "N_" << (i + 1) << "={";
        for (int j = 0; j < static_cast<int>(m.nodes.size()); ++j) {
            if (j) os << ",";
            os << (m.nodes[j] + 1);
        }
        os << "} P=" << static_cast<double>(m.profit)
           << " W=" << static_cast<double>(m.weight)
           << " ratio=" << static_cast<double>(m.ratio().value());
        if (i + 1 != static_cast<int>(sequence.items.size())) os << '\n';
    }
    return os;
}

}  // namespace macroitems
