#include "CsvLoader.h"
#include "Dijkstra.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
std::string jsonString(const std::string& value) {
    std::string result = "\"";
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 0x20) result += ' ';
            else result += static_cast<char>(ch);
        }
    }
    return result + '"';
}

void writeStationJson(const metromitra::Station& station) {
    std::cout << "{\"id\":" << jsonString(station.id)
              << ",\"name\":" << jsonString(station.name)
              << ",\"line\":" << jsonString(station.line)
              << ",\"latitude\":";
    if (station.latitude) std::cout << *station.latitude;
    else std::cout << "null";
    std::cout << ",\"longitude\":";
    if (station.longitude) std::cout << *station.longitude;
    else std::cout << "null";
    std::cout << ",\"interchange\":" << (station.interchange ? "true" : "false") << '}';
}

metromitra::RouteObjective parseObjective(const std::string& value) {
    if (value == "distance") return metromitra::RouteObjective::Distance;
    if (value == "transfers") return metromitra::RouteObjective::FewestTransfers;
    if (value.empty() || value == "time") return metromitra::RouteObjective::TravelTime;
    throw std::invalid_argument("Mode must be 'distance', 'time', or 'transfers'");
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

const metromitra::Station* chooseStation(const metromitra::Graph& graph, const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (type part of the station name): ";
        std::string query;
        if (!std::getline(std::cin, query)) return nullptr;
        query = lowercase(query);
        std::vector<const metromitra::Station*> matches;
        for (const auto& id : graph.stationIds()) {
            const auto* station = graph.findStation(id);
            if (lowercase(station->name).find(query) != std::string::npos) matches.push_back(station);
        }
        if (matches.empty()) {
            std::cout << "No stations match that search. Try another name.\n";
            continue;
        }
        if (matches.size() > 30) {
            std::cout << "Too many matches (" << matches.size() << "). Type a more specific name.\n";
            continue;
        }
        for (std::size_t i = 0; i < matches.size(); ++i) {
            std::cout << "  " << (i + 1) << ") " << matches[i]->name << " [" << matches[i]->id << "]\n";
        }
        std::cout << "Choose a number: ";
        std::string choice;
        if (!std::getline(std::cin, choice)) return nullptr;
        try {
            std::size_t used = 0;
            const auto selected = std::stoul(choice, &used);
            if (used == choice.size() && selected >= 1 && selected <= matches.size())
                return matches[selected - 1];
        } catch (...) {
        }
        std::cout << "Please enter one of the listed numbers.\n";
    }
}

int runInteractive(const metromitra::Graph& graph) {
    using namespace metromitra;
    std::cout << "\nMetroMitra station search\n";
    const auto* source = chooseStation(graph, "Starting station");
    if (!source) return 1;
    const auto* destination = chooseStation(graph, "Ending station");
    if (!destination) return 1;

    std::cout << "Optimize for: 1) Fastest time  2) Shortest distance  3) Fewest transfers\nChoice: ";
    std::string choice;
    if (!std::getline(std::cin, choice)) return 1;
    RouteObjective objective;
    if (choice == "1") objective = RouteObjective::TravelTime;
    else if (choice == "2") objective = RouteObjective::Distance;
    else if (choice == "3") objective = RouteObjective::FewestTransfers;
    else {
        std::cout << "Invalid choice. Enter 1, 2, or 3.\n";
        return 1;
    }

    const auto route = shortestRoute(graph, source->id, destination->id, objective);
    if (route.stationIds.empty()) {
        std::cout << "No route found between those stations in this snapshot.\n";
        return 0;
    }
    const char* objectiveName = objective == RouteObjective::Distance ? "shortest distance"
        : objective == RouteObjective::FewestTransfers ? "fewest transfers" : "fastest time";
    std::cout << "\nRoute (" << objectiveName << "):\n";
    for (std::size_t i = 0; i < route.stationIds.size(); ++i) {
        const auto* station = graph.findStation(route.stationIds[i]);
        std::cout << "  " << station->name;
        if (i + 1 < route.stationIds.size()) std::cout << " ->";
        std::cout << '\n';
    }
    std::cout << "Distance: " << route.totalDistanceKm << " km\n"
              << "Estimated ride + dwell/line-change allowance (excludes train waits and transfer walking): "
              << route.estimatedTravelTimeMinutes << " min\n"
              << "Line changes: " << route.transferCount << "\n";
    if (!route.linesUsed.empty()) {
        std::cout << "Lines: ";
        for (std::size_t i = 0; i < route.linesUsed.size(); ++i)
            std::cout << (i ? " -> " : "") << route.linesUsed[i];
        std::cout << '\n';
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    using namespace metromitra;
    const bool interactive = argc > 1 && std::string(argv[1]) == "--interactive";
    const bool jsonStations = argc > 1 && std::string(argv[1]) == "--json-stations";
    const bool jsonNetwork = argc > 1 && std::string(argv[1]) == "--json-network";
    const bool jsonRoute = argc > 1 && std::string(argv[1]) == "--json-route";
    const bool jsonMeetup = argc > 1 && std::string(argv[1]) == "--json-meetup";
    const bool routeCommand = argc > 1 && (std::string(argv[1]) == "--route" || jsonRoute);
    const bool machineCommand = jsonStations || jsonNetwork || jsonRoute || jsonMeetup;
    if (argc > 1 && std::string(argv[1]) == "--route" && argc < 4) {
        std::cerr << "Usage: metromitra.exe --route SOURCE_ID DESTINATION_ID [distance|time|transfers]\n";
        return 2;
    }
    const std::filesystem::path stations = argc > 1 && !interactive && !machineCommand ? argv[1] : "backend/data/stations.csv";
    const std::filesystem::path connections = argc > 2 && !interactive && !machineCommand ? argv[2] : "backend/data/connections.csv";
    try {
        const auto stationPath = routeCommand ? std::filesystem::path("backend/data/stations.csv") : stations;
        const auto connectionPath = routeCommand ? std::filesystem::path("backend/data/connections.csv") : connections;
        const auto graph = loadGraphCsv(stationPath, connectionPath);
        if (jsonStations) {
            std::cout << '[';
            const auto ids = graph.stationIds();
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (i) std::cout << ',';
                writeStationJson(*graph.findStation(ids[i]));
            }
            std::cout << "]\n";
            return 0;
        }
        if (jsonNetwork) {
            std::cout << "{\"stations\":[";
            const auto ids = graph.stationIds();
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (i) std::cout << ',';
                writeStationJson(*graph.findStation(ids[i]));
            }
            std::cout << "],\"connections\":[";
            bool first = true;
            for (const auto& id : ids) {
                for (const auto& edge : graph.neighbors(id)) {
                    if (id >= edge.destinationId) continue;
                    if (!first) std::cout << ',';
                    first = false;
                    std::cout << "{\"from\":" << jsonString(id)
                              << ",\"to\":" << jsonString(edge.destinationId)
                              << ",\"line\":" << jsonString(edge.line) << '}';
                }
            }
            std::cout << "]}\n";
            return 0;
        }
        if (jsonMeetup) {
            if (argc < 3) throw std::invalid_argument("Meetup requires at least two comma-separated station IDs");
            std::vector<std::string> origins;
            std::istringstream input(argv[2]);
            for (std::string id; std::getline(input, id, ',');) {
                if (id.empty()) throw std::invalid_argument("Meetup station IDs cannot be empty");
                if (!graph.findStation(id)) throw std::invalid_argument("Unknown meetup origin station: " + id);
                origins.push_back(id);
            }
            if (origins.size() < 2 || origins.size() > 8)
                throw std::invalid_argument("Meetup supports between 2 and 8 friends");

            const std::string finalDestinationId = argc > 3 ? argv[3] : "";
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
                    if (!finalDestinationId.empty()) {
                        const auto& destinationRoute = destinationRoutes[friendIndex];
                        if (std::find(destinationRoute.stationIds.begin(), destinationRoute.stationIds.end(), candidateId) == destinationRoute.stationIds.end()) {
                            reachableByEveryone = false;
                            break;
                        }
                    }
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
        if (!jsonRoute) {
            std::cout << "MetroMitra graph ready: " << graph.stationCount()
                      << " stations loaded from CSV.\n";
        }
        if (interactive) return runInteractive(graph);
        if (routeCommand) {
            if (argc < (jsonRoute ? 5 : 4)) throw std::invalid_argument("Route command requires source and destination station IDs");
            RouteObjective objective = parseObjective(argc > 4 ? argv[4] : "time");
            const auto route = shortestRoute(graph, argv[2], argv[3], objective);
            if (jsonRoute) {
                std::cout << std::setprecision(10) << "{\"ok\":" << (!route.stationIds.empty() ? "true" : "false")
                          << ",\"distanceKm\":" << route.totalDistanceKm
                          << ",\"estimatedTimeMinutes\":" << route.estimatedTravelTimeMinutes
                          << ",\"transfers\":" << route.transferCount << ",\"lines\":[";
                for (std::size_t i = 0; i < route.linesUsed.size(); ++i)
                    std::cout << (i ? "," : "") << jsonString(route.linesUsed[i]);
                std::cout << "],\"stations\":[";
                for (std::size_t i = 0; i < route.stationIds.size(); ++i) {
                    if (i) std::cout << ',';
                    writeStationJson(*graph.findStation(route.stationIds[i]));
                }
                std::cout << "],\"segmentLines\":[";
                for (std::size_t i = 0; i < route.segmentLines.size(); ++i)
                    std::cout << (i ? "," : "") << jsonString(route.segmentLines[i]);
                std::cout << "]}\n";
                return 0;
            }
            if (route.stationIds.empty()) {
                std::cout << "No route found between those stations in this snapshot.\n";
                return 0;
            }
            const char* objectiveName = objective == RouteObjective::Distance ? "shortest distance"
                : objective == RouteObjective::FewestTransfers ? "fewest transfers" : "fastest time";
            std::cout << "Route (" << objectiveName << "):\n";
            for (std::size_t i = 0; i < route.stationIds.size(); ++i) {
                const auto* station = graph.findStation(route.stationIds[i]);
                std::cout << "  " << station->name << " [" << station->id << "]";
                if (i + 1 < route.stationIds.size()) std::cout << " ->";
                std::cout << '\n';
            }
            std::cout << "Distance: " << route.totalDistanceKm << " km\n"
                      << "Estimated ride + dwell/line-change allowance (excludes train waits and transfer walking): "
                      << route.estimatedTravelTimeMinutes << " min\n"
                      << "Line changes: " << route.transferCount << "\n";
            if (!route.linesUsed.empty()) {
                std::cout << "Lines: ";
                for (std::size_t i = 0; i < route.linesUsed.size(); ++i)
                    std::cout << (i ? " -> " : "") << route.linesUsed[i];
                std::cout << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Could not load graph: " << error.what() << '\n';
        return 1;
    }
}
