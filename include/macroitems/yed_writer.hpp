#pragma once

#include "core.hpp"

#include <string>

namespace macroitems {

/// Write a GraphML file readable by yEd Desktop.
/// Nodes are coloured by macroitem membership; layout mirrors the paper
/// (sinks at top, sources at bottom).
void write_yed_graphml_file(const std::string& path,
                            const Instance& instance,
                            const MacroSequence& sequence);

}  // namespace macroitems
