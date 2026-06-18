#include "macroitems/algo_in_forest.hpp"
#include "macroitems/instance.hpp"
#include "internal/work_graph.hpp"

#include <optional>
#include <queue>
#include <stdexcept>
#include <vector>

namespace macroitems {

MacroSequence compute_in_tree_macroitems_heap(const Instance& instance) {
    // -----------------------------------------------------------------------
    // In an in-tree each node has at most one outgoing arc (toward its parent).
    // Because F_u = {u} for every leaf/node with a unique parent, the optimal
    // wing ratio of the single arc u→v always equals p(u)/w(u).  Therefore the
    // globally best wing or final ratio is always the highest current node ratio,
    // which a max-heap gives us in O(log n) per step instead of O(n²).
    // -----------------------------------------------------------------------
    std::vector<WorkNode> g(instance.n);
    // version[v] is bumped each time v's ratio changes after a contraction;
    // stale heap entries (with an outdated version) are discarded on extraction.
    std::vector<int> version(instance.n, 0);
    for (int i = 0; i < instance.n; ++i) {
        g[i].p = instance.profit[i];
        g[i].w = instance.weight[i];
        g[i].original_nodes = {i};
    }
    for (const auto& a : instance.arcs) {
        g[a.tail].out.insert(a.head);
        g[a.head].in.insert(a.tail);
    }

    struct Entry {
        Ratio ratio;
        int node = -1;
        int version = 0;
    };
    // Lazy max-heap: yields the node with the highest current p/w ratio.
    // "Lazy" means we push updated entries rather than updating in place;
    // outdated entries are skipped when popped.
    auto cmp = [](const Entry& a, const Entry& b) {
        const int cr = compare_ratio(a.ratio, b.ratio);
        if (cr != 0) return cr < 0;  // higher ratio → higher priority
        return a.node > b.node;       // tie-break by node index for determinism
    };
    std::priority_queue<Entry, std::vector<Entry>, decltype(cmp)> heap(cmp);
    auto push = [&](int v) {
        if (g[v].alive) heap.push({{g[v].p, g[v].w}, v, version[v]});
    };
    for (int i = 0; i < instance.n; ++i) push(i);

    MacroSequence sequence;
    std::optional<Ratio> current_ratio;  // ratio of the macroitem being built
    int alive_count = instance.n;
    while (alive_count > 0) {
        // Pop the best current node, skipping stale (already contracted) entries.
        Entry top;
        do {
            if (heap.empty()) throw std::runtime_error("in-tree heap became empty");
            top = heap.top();
            heap.pop();
        } while (!g[top.node].alive || top.version != version[top.node]);
        const int u = top.node;

        if (g[u].out.empty()) {
            // --- EMIT --------------------------------------------------------
            // u has no outgoing arc, so it is a root in the current contracted
            // in-tree.  Its ratio p(u)/w(u) is the best remaining ratio and it
            // forms the next macroitem (or is merged into the current one if the
            // ratio matches — ties must stay in the same macroitem).
            auto macro = make_macroitem(instance, g[u].original_nodes);
            if (!current_ratio || compare_ratio(macro.ratio(), *current_ratio) < 0) {
                // New (strictly lower) ratio → start a fresh macroitem.
                current_ratio = macro.ratio();
                sequence.items.push_back(macro);
            } else {
                // Same ratio as the last emitted macroitem → merge into it.
                auto& last = sequence.items.back();
                last.nodes.insert(last.nodes.end(), macro.nodes.begin(), macro.nodes.end());
                std::sort(last.nodes.begin(), last.nodes.end());
                last.profit += macro.profit;
                last.weight += macro.weight;
            }
            // Disconnect u from its predecessors; they may now become roots.
            for (int pred : std::vector<int>(g[u].in.begin(), g[u].in.end()))
                g[pred].out.erase(u);
            g[u].in.clear();
            g[u].alive = false;
            --alive_count;
        } else {
            // --- CONTRACT u into its unique parent v -------------------------
            // In an in-tree u has exactly one outgoing arc u→v.  The wing ratio
            // of that arc equals p(u)/w(u), which is the highest ratio in the
            // heap, so u and v must belong to the same future macroitem.
            // Merge u into v: accumulate p/w, rewire u's predecessors to v,
            // then push a fresh heap entry for v with an updated version.
            const int v = *g[u].out.begin();
            // Predecessors of u (its children in the in-tree) now point to v.
            for (int pred : std::vector<int>(g[u].in.begin(), g[u].in.end())) {
                g[pred].out.erase(u);
                if (pred != v) {
                    g[pred].out.insert(v);
                    g[v].in.insert(pred);
                }
            }
            // Remove the u→v arc from v's incoming set.
            g[v].in.erase(u);
            // Accumulate aggregate profit, weight, and original node list into v.
            g[v].p += g[u].p;
            g[v].w += g[u].w;
            g[v].original_nodes.insert(g[v].original_nodes.end(),
                                       g[u].original_nodes.begin(), g[u].original_nodes.end());
            // Mark u dead and push the updated v entry onto the heap.
            g[u].alive = false;
            g[u].in.clear();
            g[u].out.clear();
            ++version[v];  // invalidate any stale heap entries for v
            push(v);
            --alive_count;
        }
    }
    return sequence;
}

}  // namespace macroitems
