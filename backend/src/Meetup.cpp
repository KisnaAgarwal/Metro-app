#include "Meetup.h"
#include "Dijkstra.h"
#include "JsonOutput.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace metromitra {
int writeMeetupJson(const Graph& graph, const std::string& originIds,
                    const std::string& finalDestinationId) {
    std::vector<std::string> origins;
    std::istringstream input(originIds);
    for (std::string id; std::getline(input, id, ',');) {
        if (id.empty()) throw std::invalid_argument("Meetup station IDs cannot be empty");
        if (!graph.findStation(id)) throw std::invalid_argument("Unknown meetup origin station: " + id);
        origins.push_back(id);
    }
    if (origins.size() < 2 || origins.size() > 8)
        throw std::invalid_argument("Meetup supports between 2 and 8 friends");

    if (!finalDestinationId.empty() && !graph.findStation(finalDestinationId))
        throw std::invalid_argument("Unknown meetup final destination station: " + finalDestinationId);

    struct MeetupStage {
        std::string stationId;
        std::vector<std::size_t> friendIndexes;
        double elapsedMinutes = 0.0;
    };
    std::vector<RouteResult> destinationRoutes;
    std::vector<std::vector<RouteResult>> routesToStations;
    std::vector<MeetupStage> bestStages;
    std::vector<std::string> bestJoinStationIds;
    RouteResult bestOnwardRoute;
    RouteResult bestFinalLeg;
    if (!finalDestinationId.empty()) {
        const auto stationIds = graph.stationIds();
        routesToStations.resize(origins.size());
        for (const auto& originId : origins) {
            auto route = shortestRoute(graph, originId, finalDestinationId, RouteObjective::TravelTime);
            if (route.stationIds.empty()) {
                std::cout << "{\"ok\":false}\n";
                return 0;
            }
            destinationRoutes.push_back(std::move(route));
        }
        for (std::size_t friendIndex = 0; friendIndex < origins.size(); ++friendIndex) {
            routesToStations[friendIndex].reserve(stationIds.size());
            for (const auto& stationId : stationIds)
                routesToStations[friendIndex].push_back(shortestRoute(graph, origins[friendIndex], stationId, RouteObjective::TravelTime));
        }
    }

    std::string bestStationId;
    std::vector<RouteResult> bestRoutes;
    double bestTotal = std::numeric_limits<double>::infinity();
    double bestLongest = std::numeric_limits<double>::infinity();
    const auto stationIds = graph.stationIds();
    if (!finalDestinationId.empty()) {
        for (std::size_t seedIndex = 0; seedIndex < stationIds.size(); ++seedIndex) {
            const auto& seedId = stationIds[seedIndex];
            std::vector<std::size_t> initialFriends;
            for (std::size_t friendIndex = 0; friendIndex < origins.size(); ++friendIndex) {
                const auto& path = destinationRoutes[friendIndex].stationIds;
                if (std::find(path.begin(), path.end(), seedId) != path.end()) initialFriends.push_back(friendIndex);
            }
            if (initialFriends.size() < 2) continue;

            auto sharedRoute = shortestRoute(graph, seedId, finalDestinationId, RouteObjective::TravelTime);
            if (sharedRoute.stationIds.empty()) continue;
            std::vector<MeetupStage> stages{{seedId, initialFriends, 0.0}};
            std::vector<RouteResult> friendRoutes(origins.size());
            std::vector<std::string> joinStationIds(origins.size(), seedId);
            double totalTime = 0.0;
            double longestTime = 0.0;
            bool canGatherEveryone = true;

            for (const auto friendIndex : initialFriends) {
                friendRoutes[friendIndex] = routesToStations[friendIndex][seedIndex];
                totalTime += friendRoutes[friendIndex].estimatedTravelTimeMinutes;
                longestTime = std::max(longestTime, friendRoutes[friendIndex].estimatedTravelTimeMinutes);
            }
            for (std::size_t friendIndex = 0; friendIndex < origins.size(); ++friendIndex) {
                if (std::find(initialFriends.begin(), initialFriends.end(), friendIndex) != initialFriends.end()) continue;
                std::size_t joinBackboneIndex = sharedRoute.stationIds.size();
                for (std::size_t pathIndex = 1; pathIndex < sharedRoute.stationIds.size(); ++pathIndex) {
                    const auto& stationId = sharedRoute.stationIds[pathIndex];
                    const auto& personalPath = destinationRoutes[friendIndex].stationIds;
                    if (std::find(personalPath.begin(), personalPath.end(), stationId) != personalPath.end()) {
                        joinBackboneIndex = pathIndex;
                        break;
                    }
                }
                if (joinBackboneIndex == sharedRoute.stationIds.size()) {
                    canGatherEveryone = false;
                    break;
                }
                const auto& joinStationId = sharedRoute.stationIds[joinBackboneIndex];
                const auto joinStationIt = std::find(stationIds.begin(), stationIds.end(), joinStationId);
                const auto joinIndex = static_cast<std::size_t>(std::distance(stationIds.begin(), joinStationIt));
                auto route = routesToStations[friendIndex][joinIndex];
                if (route.stationIds.empty()) { canGatherEveryone = false; break; }
                friendRoutes[friendIndex] = route;
                joinStationIds[friendIndex] = joinStationId;
                totalTime += route.estimatedTravelTimeMinutes;
                longestTime = std::max(longestTime, route.estimatedTravelTimeMinutes);
                auto stageIt = std::find_if(stages.begin(), stages.end(), [&](const MeetupStage& stage) {
                    return stage.stationId == joinStationId;
                });
                if (stageIt == stages.end()) {
                    const auto backbonePrefix = shortestRoute(graph, seedId, joinStationId, RouteObjective::TravelTime);
                    stages.push_back({joinStationId, {friendIndex}, backbonePrefix.estimatedTravelTimeMinutes});
                } else {
                    stageIt->friendIndexes.push_back(friendIndex);
                }
            }
            if (!canGatherEveryone) continue;
            std::sort(stages.begin(), stages.end(), [&](const MeetupStage& left, const MeetupStage& right) {
                const auto leftIt = std::find(sharedRoute.stationIds.begin(), sharedRoute.stationIds.end(), left.stationId);
                const auto rightIt = std::find(sharedRoute.stationIds.begin(), sharedRoute.stationIds.end(), right.stationId);
                return leftIt < rightIt;
            });
            if (totalTime < bestTotal - 1e-9 ||
                (std::abs(totalTime - bestTotal) <= 1e-9 && longestTime < bestLongest - 1e-9)) {
                bestStationId = seedId;
                bestRoutes = std::move(friendRoutes);
                bestJoinStationIds = std::move(joinStationIds);
                bestStages = std::move(stages);
                bestOnwardRoute = std::move(sharedRoute);
                const auto& lastStageId = bestStages.back().stationId;
                bestFinalLeg = shortestRoute(graph, lastStageId, finalDestinationId, RouteObjective::TravelTime);
                bestTotal = totalTime;
                bestLongest = longestTime;
            }
        }
    } else for (const auto& candidateId : stationIds) {
        std::vector<RouteResult> routes;
        double totalTime = 0.0;
        double longestTime = 0.0;
        bool reachableByEveryone = true;
        for (std::size_t friendIndex = 0; friendIndex < origins.size(); ++friendIndex) {
            const auto& originId = origins[friendIndex];
            auto route = shortestRoute(graph, originId, candidateId, RouteObjective::TravelTime);
            if (route.stationIds.empty()) { reachableByEveryone = false; break; }
            totalTime += route.estimatedTravelTimeMinutes;
            longestTime = std::max(longestTime, route.estimatedTravelTimeMinutes);
            routes.push_back(std::move(route));
        }
        if (!reachableByEveryone) continue;
        if (totalTime < bestTotal - 1e-9 ||
            (std::abs(totalTime - bestTotal) <= 1e-9 && longestTime < bestLongest - 1e-9)) {
            bestStationId = candidateId;
            bestRoutes = std::move(routes);
            bestTotal = totalTime;
            bestLongest = longestTime;
        }
    }
    if (bestStationId.empty()) {
        std::cout << "{\"ok\":false}\n";
        return 0;
    }
    std::cout << std::setprecision(10) << "{\"ok\":true,\"totalTimeMinutes\":" << bestTotal
              << ",\"longestTripMinutes\":" << bestLongest << ",\"meetup\":";
    writeStationJson(*graph.findStation(bestStationId));
    if (!finalDestinationId.empty()) {
        std::cout << ",\"finalDestination\":";
        writeStationJson(*graph.findStation(finalDestinationId));
        std::cout << ",\"onwardRoute\":{\"estimatedTimeMinutes\":" << bestOnwardRoute.estimatedTravelTimeMinutes << ",\"stations\":[";
        for (std::size_t i = 0; i < bestOnwardRoute.stationIds.size(); ++i) {
            if (i) std::cout << ',';
            writeStationJson(*graph.findStation(bestOnwardRoute.stationIds[i]));
        }
        std::cout << "],\"segmentLines\":[";
        for (std::size_t i = 0; i < bestOnwardRoute.segmentLines.size(); ++i)
            std::cout << (i ? "," : "") << jsonString(bestOnwardRoute.segmentLines[i]);
        std::cout << "]},\"finalLeg\":{\"from\":";
        writeStationJson(*graph.findStation(bestStages.back().stationId));
        std::cout << ",\"estimatedTimeMinutes\":" << bestFinalLeg.estimatedTravelTimeMinutes << ",\"stations\":[";
        for (std::size_t i = 0; i < bestFinalLeg.stationIds.size(); ++i) {
            if (i) std::cout << ',';
            writeStationJson(*graph.findStation(bestFinalLeg.stationIds[i]));
        }
        std::cout << "]},\"stages\":[";
        for (std::size_t i = 0; i < bestStages.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << "{\"station\":";
            writeStationJson(*graph.findStation(bestStages[i].stationId));
            std::cout << ",\"elapsedMinutes\":" << bestStages[i].elapsedMinutes << ",\"friends\":[";
            for (std::size_t j = 0; j < bestStages[i].friendIndexes.size(); ++j)
                std::cout << (j ? "," : "") << (bestStages[i].friendIndexes[j] + 1);
            std::cout << "]}";
        }
        std::cout << "]";
    }
    std::cout << ",\"journeys\":[";
    for (std::size_t i = 0; i < origins.size(); ++i) {
        if (i) std::cout << ',';
        const auto& route = bestRoutes[i];
        std::cout << "{\"from\":";
        writeStationJson(*graph.findStation(origins[i]));
        std::cout << ",\"distanceKm\":" << route.totalDistanceKm
                  << ",\"estimatedTimeMinutes\":" << route.estimatedTravelTimeMinutes;
        if (!finalDestinationId.empty()) {
            std::cout << ",\"joinAt\":";
            writeStationJson(*graph.findStation(bestJoinStationIds[i]));
        }
        std::cout << ",\"transfers\":" << route.transferCount
                  << ",\"stations\":[";
        for (std::size_t j = 0; j < route.stationIds.size(); ++j) {
            if (j) std::cout << ',';
            writeStationJson(*graph.findStation(route.stationIds[j]));
        }
        std::cout << "],\"segmentLines\":[";
        for (std::size_t j = 0; j < route.segmentLines.size(); ++j)
            std::cout << (j ? "," : "") << jsonString(route.segmentLines[j]);
        std::cout << "]}";
    }
    std::cout << "]}\n";
    return 0;
}
} // namespace metromitra
