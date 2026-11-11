#pragma once

#include "Graph.h"

#include <string>
#include <vector>

namespace metromitra {

enum class RouteObjective { Distance, TravelTime, FewestTransfers };
constexpr double kDefaultStationDwellMinutes = 0.75; // User-selected 45-second assumption.

struct RouteResult {
    std::vector<std::string> stationIds;
    std::vector<std::string> linesUsed;
    std::vector<std::string> segmentLines;
    double totalDistanceKm = 0.0;
    double estimatedTravelTimeMinutes = 0.0;
    std::size_t transferCount = 0;
};

// Binary min-heap Dijkstra over station/incoming-line states: O((S+F) log S)
// time and O(S+F) space, where S/F are states/transitions in the expanded graph.
RouteResult shortestRoute(const Graph& graph, const std::string& sourceId,
                          const std::string& destinationId, RouteObjective objective);

} // namespace metromitra
