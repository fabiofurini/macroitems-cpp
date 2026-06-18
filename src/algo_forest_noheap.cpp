#include "macroitems/algo_forest.hpp"
#include "macroitems/instance.hpp"
#include "internal/work_graph.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace macroitems {

namespace {

struct ClosureSum {
    double p = 0.0;
    double w = 0.0;
};

struct FastRatio {
    double num = 0.0;
    double den = 1.0;
};

int compare_fast_ratio(const FastRatio& a, const FastRatio& b) {
    const double lhs = a.num * b.den;
    const double rhs = b.num * a.den;
    const double eps = 1e-12 * (std::abs(lhs) + std::abs(rhs) + 1.0);
    if (lhs > rhs + eps) return 1;
    if (lhs + eps < rhs) return -1;
    return 0;
}

struct FastNode {
    bool alive = true;
    double p = 0.0;
    double w = 0.0;
    int first_out = -1;
    int first_in = -1;
    int first_original = -1;
    int last_original = -1;
    int original_count = 0;
};

struct FastEdge {
    int from = -1;
    int to = -1;
    int next_out = -1;
    int prev_out = -1;
    int next_in = -1;
    int prev_in = -1;
    bool alive = true;
};

struct ClosureWorkspace {
    std::vector<int> indegree;
    std::vector<int> queue;
    std::vector<int> topo;
    std::vector<ClosureSum> sums;
};

void attach_out(std::vector<FastNode>& g, std::vector<FastEdge>& edges, int node, int edge) {
    edges[edge].from = node;
    edges[edge].prev_out = -1;
    edges[edge].next_out = g[node].first_out;
    if (g[node].first_out != -1) edges[g[node].first_out].prev_out = edge;
    g[node].first_out = edge;
}

void attach_in(std::vector<FastNode>& g, std::vector<FastEdge>& edges, int node, int edge) {
    edges[edge].to = node;
    edges[edge].prev_in = -1;
    edges[edge].next_in = g[node].first_in;
    if (g[node].first_in != -1) edges[g[node].first_in].prev_in = edge;
    g[node].first_in = edge;
}

void detach_out(std::vector<FastNode>& g, std::vector<FastEdge>& edges, int edge) {
    const int node = edges[edge].from;
    const int prev = edges[edge].prev_out;
    const int next = edges[edge].next_out;
    if (prev != -1) edges[prev].next_out = next;
    else g[node].first_out = next;
    if (next != -1) edges[next].prev_out = prev;
    edges[edge].prev_out = -1;
    edges[edge].next_out = -1;
}

void detach_in(std::vector<FastNode>& g, std::vector<FastEdge>& edges, int edge) {
    const int node = edges[edge].to;
    const int prev = edges[edge].prev_in;
    const int next = edges[edge].next_in;
    if (prev != -1) edges[prev].next_in = next;
    else g[node].first_in = next;
    if (next != -1) edges[next].prev_in = prev;
    edges[edge].prev_in = -1;
    edges[edge].next_in = -1;
}

void compute_closure_sums(
    const std::vector<FastNode>& g,
    const std::vector<FastEdge>& edges,
    const std::vector<int>& active_nodes,
    ClosureWorkspace& workspace) {
    for (int u : active_nodes) {
        workspace.indegree[u] = 0;
        workspace.sums[u].p = g[u].p;
        workspace.sums[u].w = g[u].w;
    }
    for (int u : active_nodes) {
        for (int e = g[u].first_out; e != -1; e = edges[e].next_out) {
            const int v = edges[e].to;
            ++workspace.indegree[v];
        }
    }

    workspace.queue.clear();
    workspace.queue.reserve(active_nodes.size());
    for (int u : active_nodes) {
        if (workspace.indegree[u] == 0) workspace.queue.push_back(u);
    }

    workspace.topo.clear();
    workspace.topo.reserve(active_nodes.size());
    int head = 0;
    while (head < static_cast<int>(workspace.queue.size())) {
        const int u = workspace.queue[head++];
        workspace.topo.push_back(u);
        for (int e = g[u].first_out; e != -1; e = edges[e].next_out) {
            const int v = edges[e].to;
            if (--workspace.indegree[v] == 0) workspace.queue.push_back(v);
        }
    }
    if (workspace.topo.size() != active_nodes.size())
        throw std::runtime_error("working graph is not acyclic");

    for (auto it = workspace.topo.rbegin(); it != workspace.topo.rend(); ++it) {
        const int u = *it;
        for (int e = g[u].first_out; e != -1; e = edges[e].next_out) {
            const int v = edges[e].to;
            workspace.sums[u].p += workspace.sums[v].p;
            workspace.sums[u].w += workspace.sums[v].w;
        }
    }
}

void collect_reverse_reachable(
    int start,
    const std::vector<FastNode>& g,
    const std::vector<FastEdge>& edges,
    int stamp,
    std::vector<int>& seen,
    std::vector<int>& nodes,
    std::vector<int>& stack) {
    stack.clear();
    stack.push_back(start);
    seen[start] = stamp;
    while (!stack.empty()) {
        const int u = stack.back();
        stack.pop_back();
        nodes.push_back(u);
        for (int e = g[u].first_in; e != -1; e = edges[e].next_in) {
            const int pred = edges[e].from;
            if (seen[pred] == stamp) continue;
            seen[pred] = stamp;
            stack.push_back(pred);
        }
    }
}

}  // namespace

MacroSequence compute_forest_macroitems_noheap(const Instance& instance) {
    std::vector<FastNode> g(instance.n);
    std::vector<int> next_original(instance.n, -1);
    for (int i = 0; i < instance.n; ++i) {
        g[i].p = instance.profit[i];
        g[i].w = instance.weight[i];
        g[i].first_original = i;
        g[i].last_original = i;
        g[i].original_count = 1;
    }

    std::vector<FastEdge> edges(instance.arcs.size());
    for (int e = 0; e < static_cast<int>(instance.arcs.size()); ++e) {
        const auto& a = instance.arcs[e];
        edges[e].alive = true;
        attach_out(g, edges, a.tail, e);
        attach_in(g, edges, a.head, e);
    }

    MacroSequence sequence;
    std::vector<int> active_nodes(instance.n);
    std::vector<int> active_pos(instance.n);
    for (int i = 0; i < instance.n; ++i) {
        active_nodes[i] = i;
        active_pos[i] = i;
    }
    auto remove_active = [&](int u) {
        const int pos = active_pos[u];
        const int last = active_nodes.back();
        active_nodes[pos] = last;
        active_pos[last] = pos;
        active_nodes.pop_back();
        g[u].alive = false;
    };

    ClosureWorkspace closure;
    closure.indegree.assign(instance.n, 0);
    closure.sums.resize(instance.n);
    compute_closure_sums(g, edges, active_nodes, closure);

    auto current_wing_ratio = [&](int e) {
        const int u = edges[e].from;
        const int v = edges[e].to;
        return FastRatio{closure.sums[u].p - closure.sums[v].p,
                         closure.sums[u].w - closure.sums[v].w};
    };

    auto find_best_wing = [&]() {
        int best_edge = -1;
        FastRatio best_ratio{-std::numeric_limits<double>::infinity(), 1.0};
        for (int e = 0; e < static_cast<int>(edges.size()); ++e) {
            if (!edges[e].alive) continue;
            if (!g[edges[e].from].alive || !g[edges[e].to].alive) continue;
            const FastRatio ratio = current_wing_ratio(e);
            const int cr = compare_fast_ratio(ratio, best_ratio);
            if (cr > 0 || (cr == 0 && (best_edge == -1 || e < best_edge))) {
                best_edge = e;
                best_ratio = ratio;
            }
        }
        return std::pair<int, FastRatio>{best_edge, best_ratio};
    };

    auto find_best_final = [&]() {
        int best_node = -1;
        FastRatio best_ratio{-std::numeric_limits<double>::infinity(), 1.0};
        for (int u : active_nodes) {
            if (!g[u].alive || g[u].first_out != -1) continue;
            const FastRatio ratio{g[u].p, g[u].w};
            const int cr = compare_fast_ratio(ratio, best_ratio);
            if (cr > 0 || (cr == 0 && (best_node == -1 || u < best_node))) {
                best_node = u;
                best_ratio = ratio;
            }
        }
        return std::pair<int, FastRatio>{best_node, best_ratio};
    };

    std::vector<int> mark(instance.n, 0);
    std::vector<int> reverse_stack;
    std::vector<int> ancestors_a;
    std::vector<int> ancestors_b;
    std::vector<int> affected;
    int stamp = 1;

    while (!active_nodes.empty()) {
        const auto [best_edge, best_wing] = find_best_wing();
        const bool has_wing = best_edge != -1;
        const auto [best_final_node, best_final] = find_best_final();
        if (best_final_node == -1) throw std::runtime_error("working graph has no final node");

        if (!has_wing || compare_fast_ratio(best_final, best_wing) > 0) {
            std::vector<int> best_finals;
            for (int u : active_nodes) {
                if (!g[u].alive || g[u].first_out != -1) continue;
                if (compare_fast_ratio({g[u].p, g[u].w}, best_final) == 0) {
                    best_finals.push_back(u);
                }
            }

            std::vector<int> macro_nodes;
            int macro_size = 0;
            for (int f : best_finals) macro_size += g[f].original_count;
            macro_nodes.reserve(macro_size);
            for (int f : best_finals) {
                for (int node = g[f].first_original; node != -1; node = next_original[node]) {
                    macro_nodes.push_back(node);
                }
            }
            sequence.items.push_back(make_macroitem(instance, macro_nodes));

            for (int f : best_finals) {
                ancestors_a.clear();
                collect_reverse_reachable(f, g, edges, stamp++, mark, ancestors_a, reverse_stack);
                const auto delta = closure.sums[f];
                for (int x : ancestors_a) {
                    if (x == f) continue;
                    closure.sums[x].p -= delta.p;
                    closure.sums[x].w -= delta.w;
                }
                while (g[f].first_in != -1) {
                    const int e = g[f].first_in;
                    detach_in(g, edges, e);
                    detach_out(g, edges, e);
                    edges[e].alive = false;
                }
                remove_active(f);
            }
        } else {
            const int u = edges[best_edge].from;
            const int v = edges[best_edge].to;

            ancestors_a.clear();
            ancestors_b.clear();
            const int stamp_v = stamp++;
            collect_reverse_reachable(v, g, edges, stamp_v, mark, ancestors_a, reverse_stack);
            const int stamp_u = stamp++;
            collect_reverse_reachable(u, g, edges, stamp_u, mark, ancestors_b, reverse_stack);
            const ClosureSum delta{closure.sums[u].p - closure.sums[v].p,
                                   closure.sums[u].w - closure.sums[v].w};
            for (int x : ancestors_a) {
                if (mark[x] == stamp_u) continue;
                closure.sums[x].p += delta.p;
                closure.sums[x].w += delta.w;
            }

            while (g[u].first_in != -1) {
                const int e = g[u].first_in;
                detach_in(g, edges, e);
                attach_in(g, edges, v, e);
            }
            while (g[u].first_out != -1) {
                const int e = g[u].first_out;
                const int succ = edges[e].to;
                detach_out(g, edges, e);
                if (succ != v) {
                    attach_out(g, edges, v, e);
                } else {
                    detach_in(g, edges, e);
                    edges[e].alive = false;
                }
            }

            g[v].p += g[u].p;
            g[v].w += g[u].w;
            if (g[u].first_original != -1) {
                next_original[g[u].last_original] = g[v].first_original;
                g[v].first_original = g[u].first_original;
                g[v].original_count += g[u].original_count;
            }
            remove_active(u);
        }
    }

    return sequence;
}

}  // namespace macroitems
