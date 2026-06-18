#include "macroitems/yed_writer.hpp"
#include "macroitems/instance.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace macroitems {

namespace {

// ---------------------------------------------------------------------------
// Graph helpers
// ---------------------------------------------------------------------------

std::vector<int> macroitem_index_by_node(const Instance& instance,
                                         const MacroSequence& sequence) {
    std::vector<int> index(instance.n, -1);
    for (int r = 0; r < static_cast<int>(sequence.items.size()); ++r) {
        for (int v : sequence.items[r].nodes) {
            if (v >= 0 && v < instance.n) index[v] = r;
        }
    }
    return index;
}

std::vector<int> topological_levels(const Instance& instance) {
    std::vector<std::vector<int>> out(instance.n);
    std::vector<int> indeg(instance.n, 0);
    for (const auto& arc : instance.arcs) {
        out[arc.tail].push_back(arc.head);
        ++indeg[arc.head];
    }

    std::queue<int> q;
    std::vector<int> level(instance.n, 0);
    for (int i = 0; i < instance.n; ++i) {
        if (indeg[i] == 0) q.push(i);
    }

    int seen = 0;
    while (!q.empty()) {
        const int u = q.front();
        q.pop();
        ++seen;
        for (int v : out[u]) {
            level[v] = std::max(level[v], level[u] + 1);
            if (--indeg[v] == 0) q.push(v);
        }
    }
    if (seen != instance.n) throw std::invalid_argument("yEd writer requires a DAG");
    return level;
}

// ---------------------------------------------------------------------------
// Paper colour palette (4 colours, cycling — matches paper definitions)
// ---------------------------------------------------------------------------

static constexpr int kPaletteSize = 4;

static const char* kColorHex[kPaletteSize] = {
    "#FFF0AA", "#FFD2D2", "#D2F0D2", "#D2E1FF"};

const char* paper_color_hex(int macro_idx) {
    if (macro_idx < 0) return "#FFFFFF";
    return kColorHex[macro_idx % kPaletteSize];
}

// ---------------------------------------------------------------------------
// Formatting helper
// ---------------------------------------------------------------------------

std::string fmt_val(long double v) {
    const long long iv = llroundl(v);
    if (std::abs(v - static_cast<long double>(iv)) < 1e-6L)
        return std::to_string(iv);
    std::ostringstream oss;
    oss << std::defaultfloat << std::setprecision(4) << static_cast<double>(v);
    return oss.str();
}

}  // namespace

// ===========================================================================
// write_yed_graphml_file
// Writes a GraphML file readable by yEd Desktop.
// Nodes are coloured by macroitem; layout mirrors the paper (sinks at top).
// ===========================================================================

void write_yed_graphml_file(const std::string& path,
                            const Instance& instance,
                            const MacroSequence& sequence) {
    const auto macro_of = macroitem_index_by_node(instance, sequence);

    // yEd coordinate system: y increases downward.
    // Mirror the paper layout: sinks at top (y = 0), sources at bottom.
    const auto level    = topological_levels(instance);
    const int max_level = instance.n > 0
                          ? *std::max_element(level.begin(), level.end())
                          : 0;

    std::vector<std::vector<int>> layers(max_level + 1);
    for (int v = 0; v < instance.n; ++v) layers[level[v]].push_back(v);

    constexpr double kSpacing = 110.0;  // pixels between node centres
    constexpr double kNodeSz  =  55.0;  // node diameter in pixels

    std::vector<std::pair<double, double>> xy(instance.n);
    for (int l = 0; l <= max_level; ++l) {
        auto& layer = layers[l];
        std::sort(layer.begin(), layer.end());
        const int sz = static_cast<int>(layer.size());
        // sinks (level == max_level) → y = 0 (top); sources → y = max_level * kSpacing
        const double y = (max_level - l) * kSpacing;
        for (int pos = 0; pos < sz; ++pos) {
            xy[layer[pos]] = {kSpacing * (pos - 0.5 * (sz - 1)), y};
        }
    }

    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot open yEd output file: " + path);

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        << "<graphml\n"
        << "  xmlns=\"http://graphml.graphdrawing.org/graphml\"\n"
        << "  xmlns:y=\"http://www.yworks.com/xml/graphml\"\n"
        << "  xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
        << "  xsi:schemaLocation=\"http://graphml.graphdrawing.org/graphml"
           " http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd\">\n"
        << "  <key id=\"d0\" for=\"node\" yfiles.type=\"nodegraphics\"/>\n"
        << "  <key id=\"d1\" for=\"edge\" yfiles.type=\"edgegraphics\"/>\n"
        << "  <graph id=\"G\" edgedefault=\"directed\">\n";

    // Nodes
    for (int v = 0; v < instance.n; ++v) {
        const double cx  = xy[v].first;
        const double cy  = xy[v].second;
        const char*  hex = paper_color_hex(macro_of[v]);
        out << "    <node id=\"n" << v << "\">\n"
            << "      <data key=\"d0\">\n"
            << "        <y:ShapeNode>\n"
            << "          <y:Geometry"
            << " x=\"" << (cx - kNodeSz / 2.0) << "\""
            << " y=\"" << (cy - kNodeSz / 2.0) << "\""
            << " width=\""  << kNodeSz << "\""
            << " height=\"" << kNodeSz << "\"/>\n"
            << "          <y:Fill color=\"" << hex << "\" transparent=\"false\"/>\n"
            << "          <y:BorderStyle type=\"line\" width=\"2.0\" color=\"#000000\"/>\n"
            << "          <y:NodeLabel alignment=\"center\" autoSizePolicy=\"content\">"
            << (v + 1) << " ("
            << fmt_val(instance.profit[v]) << ","
            << fmt_val(instance.weight[v]) << ")"
            << "</y:NodeLabel>\n"
            << "          <y:Shape type=\"ellipse\"/>\n"
            << "        </y:ShapeNode>\n"
            << "      </data>\n"
            << "    </node>\n";
    }

    // Edges
    for (int i = 0; i < static_cast<int>(instance.arcs.size()); ++i) {
        const auto& arc = instance.arcs[i];
        out << "    <edge id=\"e" << i
            << "\" source=\"n" << arc.tail
            << "\" target=\"n" << arc.head << "\">\n"
            << "      <data key=\"d1\">\n"
            << "        <y:PolyLineEdge>\n"
            << "          <y:LineStyle type=\"line\" width=\"1.5\" color=\"#000000\"/>\n"
            << "          <y:Arrows source=\"none\" target=\"standard\"/>\n"
            << "        </y:PolyLineEdge>\n"
            << "      </data>\n"
            << "    </edge>\n";
    }

    out << "  </graph>\n"
        << "</graphml>\n";
}

}  // namespace macroitems
