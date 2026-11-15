#include "CsvLoader.h"
#include "Dijkstra.h"
#include "JsonOutput.h"
#include "Meetup.h"

using metromitra::jsonString;
using metromitra::writeStationJson;

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
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
            if (argc < 3) throw std::invalid_argument("Meetup requires comma-separated station IDs");
            return writeMeetupJson(graph, argv[2], argc > 3 ? argv[3] : "");
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
