#pragma once

#include "Graph.h"

#include <string>
#include <vector>

namespace metromitra {

// Both traversals run in O(V+E) time and O(V) auxiliary space.
std::vector<std::string> breadthFirstTraversal(const Graph& graph,
                                               const std::string& startId);
std::vector<std::string> depthFirstTraversal(const Graph& graph,
                                             const std::string& startId);

} // namespace metromitra
