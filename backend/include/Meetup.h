#pragma once

#include "Graph.h"
#include <string>

namespace metromitra {
// Find a meeting station (or staged meetings), then write the result as JSON.
int writeMeetupJson(const Graph& graph, const std::string& originIds,
                    const std::string& finalDestinationId);
}
