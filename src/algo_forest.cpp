#include "macroitems/algo_forest.hpp"
#include "macroitems/instance.hpp"
#include "internal/work_graph.hpp"

#include <cmath>
#include <limits>
#include <queue>
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

struct WingEntry {
    FastRatio ratio;
    int edge = -1;
    int version = 0;
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

MacroSequence compute_forest_macroitems(const Instance& instance) {
    // -----------------------------------------------------------------------
    // Initialise one working node per original node.  During the algorithm nodes
    // get merged via contraction, so g[i].p / g[i].w accumulate the aggregate
    // profit and weight of all original nodes that have been folded into i.
    // -----------------------------------------------------------------------
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
    // Mirror every directed arc in both adjacency sets so that
    // closure sums can follow outgoing arcs and the contraction step can repair
    // incoming arcs of the surviving endpoint.
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

    std::vector<int> version(instance.n, 0);
    struct FinalEntry {
        FastRatio ratio;
        int node = -1;
        int version = 0;
    };
    auto final_cmp = [](const FinalEntry& a, const FinalEntry& b) {
        const int cr = compare_fast_ratio(a.ratio, b.ratio);
        if (cr != 0) return cr < 0;
        return a.node > b.node;
    };
    std::priority_queue<FinalEntry, std::vector<FinalEntry>, decltype(final_cmp)> final_heap(final_cmp);
    auto push_final = [&](int u) {
        if (g[u].alive && g[u].first_out == -1)
            final_heap.push({{g[u].p, g[u].w}, u, version[u]});
    };
    auto discard_stale_finals = [&]() {
        while (!final_heap.empty()) {
            const auto top = final_heap.top();
            if (g[top.node].alive && g[top.node].first_out == -1 &&
                top.version == version[top.node]) {
                return;
            }
            final_heap.pop();
        }
    };
    for (int i = 0; i < instance.n; ++i) push_final(i);

    std::vector<int> edge_version(edges.size(), 0);
    auto wing_cmp = [](const WingEntry& a, const WingEntry& b) {
        const int cr = compare_fast_ratio(a.ratio, b.ratio);
        if (cr != 0) return cr < 0;
        return a.edge > b.edge;
    };
    std::priority_queue<WingEntry, std::vector<WingEntry>, decltype(wing_cmp)> wing_heap(wing_cmp);
    auto current_wing_ratio = [&](int e) {
        const int u = edges[e].from;
        const int v = edges[e].to;
        return FastRatio{closure.sums[u].p - closure.sums[v].p,
                         closure.sums[u].w - closure.sums[v].w};
    };
    auto push_wing = [&](int e) {
        if (e != -1 && edges[e].alive && g[edges[e].from].alive && g[edges[e].to].alive)
            wing_heap.push({current_wing_ratio(e), e, edge_version[e]});
    };
    auto touch_edge = [&](int e) {
        if (e == -1 || !edges[e].alive) return;
        ++edge_version[e];
        push_wing(e);
    };
    auto touch_node_edges = [&](int u) {
        for (int e = g[u].first_out; e != -1; e = edges[e].next_out) touch_edge(e);
        for (int e = g[u].first_in; e != -1; e = edges[e].next_in) touch_edge(e);
    };
    auto discard_stale_wings = [&]() {
        while (!wing_heap.empty()) {
            const auto top = wing_heap.top();
            if (edges[top.edge].alive && g[edges[top.edge].from].alive &&
                g[edges[top.edge].to].alive && top.version == edge_version[top.edge]) {
                return;
            }
            wing_heap.pop();
        }
    };
    for (int e = 0; e < static_cast<int>(edges.size()); ++e) push_wing(e);

    std::vector<int> mark(instance.n, 0);
    std::vector<int> selected_final(instance.n, 0);
    std::vector<int> reverse_stack;
    std::vector<int> ancestors_a;
    std::vector<int> ancestors_b;
    std::vector<int> affected;
    int stamp = 1;
    int selected_stamp = 1;

    // Main loop: each iteration either emits one macroitem or contracts one arc.
    // The loop terminates because every iteration strictly reduces the number of
    // alive working nodes (contraction) or removes them (emission).
    while (!active_nodes.empty()) {
        // ------------------------------------------------------------------
        // Step 1 — compute every current preceding-set sum once, then scan
        // every arc (u→v) in the current contracted graph and
        // compute the ratio of the minimal preceding set F_u relative to F_v:
        //   r(u→v) = (P(F_u) - P(F_v)) / (W(F_u) - W(F_v))
        // This is the "wing" ratio of the arc.  The arc with the highest wing
        // ratio is a candidate for the next contraction.
        // ------------------------------------------------------------------
        discard_stale_wings();
        const bool has_wing = !wing_heap.empty();
        const int best_edge = has_wing ? wing_heap.top().edge : -1;
        const FastRatio best_wing =
            has_wing ? wing_heap.top().ratio : FastRatio{-std::numeric_limits<double>::infinity(), 1.0};

        // ------------------------------------------------------------------
        // Step 2 — among the nodes that are currently "final" (out-degree = 0
        // in the contracted graph), find the one(s) with the highest ratio
        // p/w.  A final node with no outgoing arc is the head of its own
        // minimal preceding set, so its ratio is the best attainable without
        // first contracting another arc.
        // Ties are collected together: equal-ratio finals form a single macroitem.
        // ------------------------------------------------------------------
        discard_stale_finals();
        if (final_heap.empty()) {
            for (int u : active_nodes) push_final(u);
            discard_stale_finals();
        }
        if (final_heap.empty()) throw std::runtime_error("working graph has no final node");
        const FastRatio best_final = final_heap.top().ratio;

        // ------------------------------------------------------------------
        // Decision: emit macroitem vs. contract arc.
        //
        // The theorem guarantees that the next macroitem always starts at the
        // globally best ratio candidate, which is the maximum of:
        //   (a) the best final-node ratio   (no further contraction needed), and
        //   (b) the best wing ratio          (arc must be contracted first).
        //
        // If (a) > (b): the best final node is already optimal → emit it.
        // If (b) >= (a): the two endpoints of the best-wing arc must end up in
        //               the same future macroitem → contract u into v.
        // ------------------------------------------------------------------
        if (!has_wing || compare_fast_ratio(best_final, best_wing) > 0) {
            // --- EMIT macroitem -------------------------------------------
            // Collect original nodes from all tied best-final working nodes
            // into one macroitem, then remove those nodes from the graph.
            std::vector<int> best_finals;
            while (true) {
                discard_stale_finals();
                if (final_heap.empty() ||
                    compare_fast_ratio(final_heap.top().ratio, best_final) != 0) {
                    break;
                }
                const int node = final_heap.top().node;
                final_heap.pop();
                if (selected_final[node] == selected_stamp) continue;
                selected_final[node] = selected_stamp;
                best_finals.push_back(node);
            }
            ++selected_stamp;
            std::vector<int> macro_nodes;
            int macro_size = 0;
            for (int f : best_finals) {
                macro_size += g[f].original_count;
            }
            macro_nodes.reserve(macro_size);
            for (int f : best_finals) {
                for (int node = g[f].first_original; node != -1; node = next_original[node]) {
                    macro_nodes.push_back(node);
                }
            }
            sequence.items.push_back(make_macroitem(instance, macro_nodes));
            std::vector<int> final_candidates;
            affected.clear();
            for (int f : best_finals) {
                ancestors_a.clear();
                collect_reverse_reachable(f, g, edges, stamp++, mark, ancestors_a, reverse_stack);
                const auto delta = closure.sums[f];
                for (int x : ancestors_a) {
                    if (x == f) continue;
                    closure.sums[x].p -= delta.p;
                    closure.sums[x].w -= delta.w;
                    affected.push_back(x);
                }
                // Detach f from all predecessors so they become final candidates
                // in future iterations if their own outgoing arcs are now gone.
                while (g[f].first_in != -1) {
                    const int e = g[f].first_in;
                    const int pred = edges[e].from;
                    detach_in(g, edges, e);
                    detach_out(g, edges, e);
                    edges[e].alive = false;
                    final_candidates.push_back(pred);
                }
                remove_active(f);
            }
            for (int x : affected) touch_node_edges(x);
            for (int pred : final_candidates) push_final(pred);
        } else {
            // --- CONTRACT arc u → v ---------------------------------------
            // Merge working node u into v (v survives as the representative).
            // All predecessors of u that are not v are rewired to point to v.
            const int u = edges[best_edge].from;
            const int v = edges[best_edge].to;
            ancestors_a.clear();
            ancestors_b.clear();
            affected.clear();
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
                affected.push_back(x);
            }
            // Rewire predecessors of u → now point to v instead.
            while (g[u].first_in != -1) {
                const int e = g[u].first_in;
                detach_in(g, edges, e);
                attach_in(g, edges, v, e);
                touch_edge(e);
            }
            // Rewire successors of u (other than v itself) → now reachable from v.
            while (g[u].first_out != -1) {
                const int e = g[u].first_out;
                const int succ = edges[e].to;
                detach_out(g, edges, e);
                if (succ != v) {
                    attach_out(g, edges, v, e);
                    touch_edge(e);
                } else {
                    detach_in(g, edges, e);
                    edges[e].alive = false;
                    ++edge_version[e];
                }
            }
            // Accumulate u's aggregate data into v.
            g[v].p += g[u].p;
            g[v].w += g[u].w;
            if (g[u].first_original != -1) {
                next_original[g[u].last_original] = g[v].first_original;
                g[v].first_original = g[u].first_original;
                g[v].original_count += g[u].original_count;
            }
            ++version[v];
            push_final(v);
            for (int x : affected) touch_node_edges(x);
            // Mark u as dead.
            remove_active(u);
        }
    }
    return sequence;
}

}  // namespace macroitems
