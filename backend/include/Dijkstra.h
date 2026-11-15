#pragma once

#include "Graph.h"

#include <string>
#include <vector>

namespace metromitra {

enum class RouteObjective { Distance, TravelTime, FewestTransfers };


struct RouteResult {
    std::vector<std::string> stationIds;
    std::vector<std::string> linesUsed;
    std::vector<std::string> segmentLines;
    double totalDistanceKm = 0.0;
    double estimatedTravelTimeMinutes = 0.0;
    std::size_t transferCount = 0;
};

// Min-heap Dijkstra with one record per station: O((V+E) log(V+E)) time.
// Distance and ride-time are optimal; fewest transfers is an approximation.
RouteResult shortestRoute(const Graph& graph, const std::string& sourceId,
                          const std::string& destinationId, RouteObjective objective);

} // namespace metromitra
