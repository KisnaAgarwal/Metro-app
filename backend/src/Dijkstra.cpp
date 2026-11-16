#include "Dijkstra.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>

using namespace std;

namespace metromitra {
namespace {
// One row in our table for each station.
struct StationRecord {
    double cost = numeric_limits<double>::infinity();
    string previousStation;
    Edge previousEdge;
    string arrivalLine;
};
}

RouteResult shortestRoute(const Graph& graph, const string& sourceId,
                          const string& destinationId, RouteObjective objective) {
    if (!graph.findStation(sourceId) || !graph.findStation(destinationId)) {
        throw invalid_argument("no metro station ");
    }

    // start ko 0
    unordered_map<string, StationRecord> best;
    best[sourceId].cost = 0;

    using Entry = pair<double, string>;
    priority_queue<Entry, vector<Entry>, greater<Entry>> pending;
    pending.push({0, sourceId});

    while (!pending.empty()) {
        const auto [cost, currentId] = pending.top();
        pending.pop();
        const auto current = best.at(currentId);
        if (cost > current.cost) continue;
        if (currentId == destinationId) break;

        // 3kisna ye wala kar de please
        for (const auto& edge : graph.neighbors(currentId)) {
            bool changesLine = edge.type == EdgeType::Transfer;
            if (edge.type == EdgeType::Ride && !current.arrivalLine.empty() &&
                current.arrivalLine != edge.line) {
                changesLine = true;
            }

            double edgeCost = edge.travelTimeMinutes;
            if (objective == RouteObjective::Distance) edgeCost = edge.distanceKm;
            if (objective == RouteObjective::FewestTransfers) edgeCost = changesLine ? 1 : 0;

            const double newCost = cost + edgeCost;
            auto& next = best[edge.destinationId];
            if (newCost >= next.cost) continue;

            // update kardo better route mil gaya
            next.cost = newCost;
            next.previousStation = currentId;
            next.previousEdge = edge;
            next.arrivalLine = current.arrivalLine;
            if (edge.type == EdgeType::Ride) next.arrivalLine = edge.line;
            if (edge.type == EdgeType::Transfer) next.arrivalLine.clear();
            pending.push({newCost, edge.destinationId});
        }
    }

    if (best.find(destinationId) == best.end()) return {};

    // reversed the ans rushil pls add to JSON output
    RouteResult result;
    vector<Edge> routeEdges;
    string stationId = destinationId;
    while (stationId != sourceId) {
        result.stationIds.push_back(stationId);
        routeEdges.push_back(best.at(stationId).previousEdge);
        stationId = best.at(stationId).previousStation;
    }
    result.stationIds.push_back(sourceId);
    reverse(result.stationIds.begin(), result.stationIds.end());
    reverse(routeEdges.begin(), routeEdges.end());

    // got more info
    string previousLine;
    for (const auto& edge : routeEdges) {
        result.totalDistanceKm += edge.distanceKm;
        result.estimatedTravelTimeMinutes += edge.travelTimeMinutes;
        if (edge.type == EdgeType::Ride) {
            if (!previousLine.empty() && previousLine != edge.line) ++result.transferCount;
            previousLine = edge.line;
            result.segmentLines.push_back(edge.line);
            if (result.linesUsed.empty() || result.linesUsed.back() != edge.line) {
                result.linesUsed.push_back(edge.line);
            }
        } else if (edge.type == EdgeType::Transfer) {
            ++result.transferCount;
            previousLine.clear();
            result.segmentLines.push_back("TRANSFER");
        } else {
            result.segmentLines.push_back("WALK");
        }
    }
    return result;
}
}
