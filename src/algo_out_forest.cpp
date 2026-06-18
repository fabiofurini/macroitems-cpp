#include "macroitems/algo_out_forest.hpp"
#include "macroitems/instance.hpp"
#include "internal/work_graph.hpp"

#include <optional>
#include <queue>
#include <stdexcept>
#include <vector>

namespace macroitems {

MacroSequence compute_out_tree_macroitems_heap(const Instance& instance) {
    // -----------------------------------------------------------------------
    // Dual of the in-tree algorithm.  In an out-tree each node has at most one
    // incoming arc (from its parent).  The macroitem sequence must be emitted in
    // decreasing ratio order, but it is easier to build it in increasing order
    // using a min-heap and reverse at the end.
    //
    // Key structural property: a node v with in-degree 0 in the current
    // contracted graph is a leaf of the out-tree.  Because every arc points
    // away from the root, the minimal preceding set of v is just {v} itself.
    // So the globally lowest ratio is always found at such a leaf → emit it
    // (or contract it into its unique parent if the parent's ratio is lower).
    // -----------------------------------------------------------------------
    std::vector<WorkNode> g(instance.n);
    // version[v] is bumped after each contraction that changes v's ratio,
    // so that stale heap entries can be detected and skipped on extraction.
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
    // Lazy min-heap: yields the node with the lowest current p/w ratio.
    // Building the sequence from the bottom up and reversing at the end is
    // equivalent to building from the top down with a max-heap.
    auto cmp = [](const Entry& a, const Entry& b) {
        const int cr = compare_ratio(a.ratio, b.ratio);
        if (cr != 0) return cr > 0;  // lower ratio → higher priority
        return a.node > b.node;       // tie-break by node index for determinism
    };
    std::priority_queue<Entry, std::vector<Entry>, decltype(cmp)> heap(cmp);
    auto push = [&](int v) {
        if (g[v].alive) heap.push({{g[v].p, g[v].w}, v, version[v]});
    };
    for (int i = 0; i < instance.n; ++i) push(i);

    // Collect macroitems in increasing ratio order; reversed before returning.
    std::vector<Macroitem> increasing;
    std::optional<Ratio> current_ratio;  // ratio of the macroitem being built
    int alive_count = instance.n;
    while (alive_count > 0) {
        // Pop the node with the lowest current ratio, skipping stale entries.
        Entry top;
        do {
            if (heap.empty()) throw std::runtime_error("out-tree heap became empty");
            top = heap.top();
            heap.pop();
        } while (!g[top.node].alive || top.version != version[top.node]);
        const int v = top.node;

        if (g[v].in.empty()) {
            // --- EMIT --------------------------------------------------------
            // v has no incoming arc, so it is a leaf in the current contracted
            // out-tree.  Its ratio is the smallest remaining and it forms the
            // next macroitem in increasing order (or is merged into the current
            // one if the ratios match).
            auto macro = make_macroitem(instance, g[v].original_nodes);
            if (!current_ratio || compare_ratio(macro.ratio(), *current_ratio) > 0) {
                // New (strictly higher) ratio → start a fresh macroitem.
                current_ratio = macro.ratio();
                increasing.push_back(macro);
            } else {
                // Same ratio → merge into the last collected macroitem.
                auto& last = increasing.back();
                last.nodes.insert(last.nodes.end(), macro.nodes.begin(), macro.nodes.end());
                std::sort(last.nodes.begin(), last.nodes.end());
                last.profit += macro.profit;
                last.weight += macro.weight;
            }
            // Disconnect v from its successors; they may now become leaves.
            for (int succ : std::vector<int>(g[v].out.begin(), g[v].out.end()))
                g[succ].in.erase(v);
            g[v].out.clear();
            g[v].alive = false;
            --alive_count;
        } else {
            // --- CONTRACT v into its unique parent u -------------------------
            // v has exactly one incoming arc u→v.  The wing ratio of that arc
            // equals p(v)/w(v), which is the lowest ratio in the heap, so v
            // and u must belong to the same future macroitem.
            // Merge v into u: accumulate p/w, rewire v's successors to u,
            // then push a fresh heap entry for u with an updated version.
            const int u = *g[v].in.begin();
            // Successors of v (its children in the out-tree) now point to u.
            for (int succ : std::vector<int>(g[v].out.begin(), g[v].out.end())) {
                g[succ].in.erase(v);
                if (succ != u) {
                    g[u].out.insert(succ);
                    g[succ].in.insert(u);
                }
            }
            // Remove the u→v arc from u's outgoing set.
            g[u].out.erase(v);
            // Accumulate aggregate profit, weight, and original node list into u.
            g[u].p += g[v].p;
            g[u].w += g[v].w;
            g[u].original_nodes.insert(g[u].original_nodes.end(),
                                       g[v].original_nodes.begin(), g[v].original_nodes.end());
            // Mark v dead and push the updated u entry onto the heap.
            g[v].alive = false;
            g[v].in.clear();
            g[v].out.clear();
            ++version[u];  // invalidate any stale heap entries for u
            push(u);
            --alive_count;
        }
    }
    // The macroitems were collected in increasing ratio order; reverse to obtain
    // the standard decreasing-ratio macroitem sequence.
    MacroSequence sequence;
    sequence.items.assign(increasing.rbegin(), increasing.rend());
    return sequence;
}

}  // namespace macroitems
