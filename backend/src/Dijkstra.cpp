#include "Dijkstra.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace metromitra {
namespace {

struct SearchRecord {
    std::string stationId;
    std::string incomingLine;
    double cost = 0.0;
    double tieCost = 0.0;
    double distanceKm = 0.0;
    double travelTimeMinutes = 0.0;
    std::size_t transferCount = 0;
    bool hasParent = false;
    std::string parentKey;
    Edge incomingEdge;
};

std::string stateKey(const std::string& stationId, const std::string& line) {
    return stationId + '\x1f' + line;
}

double interchangePenalty(const Station& station) {
    const auto lineCount = static_cast<std::size_t>(
        1 + std::count(station.line.begin(), station.line.end(), '|'));
    return lineCount >= 3 ? 3.0 : 2.0;
}

} // namespace

RouteResult shortestRoute(const Graph& graph, const std::string& sourceId,
                          const std::string& destinationId, RouteObjective objective) {
    if (!graph.findStation(sourceId)) throw std::invalid_argument("Unknown source station: " + sourceId);
    if (!graph.findStation(destinationId)) throw std::invalid_argument("Unknown destination station: " + destinationId);

    using QueueItem = std::tuple<double, double, std::string>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
    std::unordered_map<std::string, SearchRecord> records;
    const auto startKey = stateKey(sourceId, "");
    records.emplace(startKey, SearchRecord{sourceId, "", 0.0, 0.0, 0.0, 0.0, 0, false, {}, {}});
    frontier.emplace(0.0, 0.0, startKey);

    while (!frontier.empty()) {
        const auto [cost, tieCost, currentKey] = frontier.top();
        frontier.pop();
        const auto currentIt = records.find(currentKey);
        if (currentIt == records.end() || cost != currentIt->second.cost || tieCost != currentIt->second.tieCost) continue;
        const auto current = currentIt->second;

        for (const auto& edge : graph.neighbors(current.stationId)) {
            const auto* station = graph.findStation(current.stationId);
            double addedTime = edge.travelTimeMinutes;
            std::size_t addedTransfers = 0;
            std::string nextLine = current.incomingLine;

            if (edge.type == EdgeType::Transfer) {
                // The edge's own time represents the explicitly modelled transfer.
                ++addedTransfers;
                nextLine.clear();
            } else if (edge.type == EdgeType::Ride) {
                if (!current.incomingLine.empty()) {
                    if (current.incomingLine == edge.line) {
                        addedTime += station->dwellTimeMinutes.value_or(kDefaultStationDwellMinutes);
                    } else {
                        addedTime += interchangePenalty(*station);
                        ++addedTransfers;
                    }
                }
                nextLine = edge.line;
            }

            const double edgeCost = objective == RouteObjective::Distance ? edge.distanceKm
                : objective == RouteObjective::TravelTime ? addedTime
                : static_cast<double>(addedTransfers);
            const double edgeTieCost = objective == RouteObjective::FewestTransfers ? addedTime : 0.0;
            const double candidate = current.cost + edgeCost;
            const double candidateTieCost = current.tieCost + edgeTieCost;
            const auto nextKey = stateKey(edge.destinationId, nextLine);
            const auto old = records.find(nextKey);
            if (old == records.end() || std::tie(candidate, candidateTieCost) <
                                           std::tie(old->second.cost, old->second.tieCost)) {
                SearchRecord next;
                next.stationId = edge.destinationId;
                next.incomingLine = nextLine;
                next.cost = candidate;
                next.tieCost = candidateTieCost;
                next.distanceKm = current.distanceKm + edge.distanceKm;
                next.travelTimeMinutes = current.travelTimeMinutes + addedTime;
                next.transferCount = current.transferCount + addedTransfers;
                next.hasParent = true;
                next.parentKey = currentKey;
                next.incomingEdge = edge;
                records[nextKey] = std::move(next);
                frontier.emplace(candidate, candidateTieCost, nextKey);
            }
        }
    }

    std::string destinationKey;
    double destinationCost = 0.0;
    bool found = false;
    for (const auto& [key, record] : records) {
        if (record.stationId != destinationId) continue;
        if (!found || std::tie(record.cost, record.tieCost) <
                          std::tie(destinationCost, records.at(destinationKey).tieCost)) {
            found = true;
            destinationCost = record.cost;
            destinationKey = key;
        }
    }
    if (!found) return {};

    RouteResult result;
    const auto& finalRecord = records.at(destinationKey);
    result.totalDistanceKm = finalRecord.distanceKm;
    result.estimatedTravelTimeMinutes = finalRecord.travelTimeMinutes;
    result.transferCount = finalRecord.transferCount;

    std::vector<Edge> pathEdges;
    for (std::string key = destinationKey;;) {
        const auto& record = records.at(key);
        result.stationIds.push_back(record.stationId);
        if (!record.hasParent) break;
        pathEdges.push_back(record.incomingEdge);
        key = record.parentKey;
    }
    std::reverse(result.stationIds.begin(), result.stationIds.end());
    std::reverse(pathEdges.begin(), pathEdges.end());
    for (const auto& edge : pathEdges) {
        result.segmentLines.push_back(edge.type == EdgeType::Ride ? edge.line
            : edge.type == EdgeType::Transfer ? "TRANSFER" : "WALK");
        if (edge.type == EdgeType::Ride &&
            (result.linesUsed.empty() || result.linesUsed.back() != edge.line)) {
            result.linesUsed.push_back(edge.line);
        }
    }
    return result;
}

} // namespace metromitra
