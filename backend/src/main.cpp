#include "CsvLoader.h"
#include "Dijkstra.h"
#include "JsonOutput.h"
#include "Meetup.h"


#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace metromitra;

namespace {
RouteObjective parseObjective(const string& value) {
    if (value == "distance") return RouteObjective::Distance;
    if (value == "transfers") return RouteObjective::FewestTransfers;
    if (value.empty() || value == "time") return RouteObjective::TravelTime;
    throw invalid_argument("Mode must be 'distance', 'time', or 'transfers'");
}

string lowercase(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return value;
}

const Station* chooseStation(const Graph& graph, const string& prompt) {
    while (true) {
        cout << prompt << " (type part of the station name): ";
        string query;
        if (!getline(cin, query)) return nullptr;
        query = lowercase(query);
        vector<const Station*> matches;
        for (const auto& id : graph.stationIds()) {
            const auto* station = graph.findStation(id);
            if (lowercase(station->name).find(query) != string::npos) matches.push_back(station);
        }
        if (matches.empty()) {
            cout << "No stations match that search. Try another name.\n";
            continue;
        }
        if (matches.size() > 30) {
            cout << "Too many matches (" << matches.size() << "). Type a more specific name.\n";
            continue;
        }
        for (size_t i = 0; i < matches.size(); ++i) {
            cout << "  " << (i + 1) << ") " << matches[i]->name << " [" << matches[i]->id << "]\n";
        }
        cout << "Choose a number: ";
        string choice;
        if (!getline(cin, choice)) return nullptr;
        try {
            size_t used = 0;
            const auto selected = stoul(choice, &used);
            if (used == choice.size() && selected >= 1 && selected <= matches.size())
                return matches[selected - 1];
        } catch (...) {
        }
        cout << "Please enter one of the listed numbers.\n";
    }
}

int runInteractive(const Graph& graph) {
    cout << "\nMetroMitra station search\n";
    const auto* source = chooseStation(graph, "Starting station");
    if (!source) return 1;
    const auto* destination = chooseStation(graph, "Ending station");
    if (!destination) return 1;

    cout << "Optimize for: 1) Fastest time  2) Shortest distance  3) Fewest transfers\nChoice: ";
    string choice;
    if (!getline(cin, choice)) return 1;
    RouteObjective objective;
    if (choice == "1") objective = RouteObjective::TravelTime;
    else if (choice == "2") objective = RouteObjective::Distance;
    else if (choice == "3") objective = RouteObjective::FewestTransfers;
    else {
        cout << "Invalid choice. Enter 1, 2, or 3.\n";
        return 1;
    }

    const auto route = shortestRoute(graph, source->id, destination->id, objective);
    if (route.stationIds.empty()) {
        cout << "No route found between those stations in this snapshot.\n";
        return 0;
    }
    const char* objectiveName = objective == RouteObjective::Distance ? "shortest distance"
        : objective == RouteObjective::FewestTransfers ? "fewest transfers" : "fastest time";
    cout << "\nRoute (" << objectiveName << "):\n";
    for (size_t i = 0; i < route.stationIds.size(); ++i) {
        const auto* station = graph.findStation(route.stationIds[i]);
        cout << "  " << station->name;
        if (i + 1 < route.stationIds.size()) cout << " ->";
        cout << '\n';
    }
    cout << "Distance: " << route.totalDistanceKm << " km\n"
              << "Estimated edge travel time (no extra waits or dwell allowances): "
              << route.estimatedTravelTimeMinutes << " min\n"
              << "Line changes: " << route.transferCount << "\n";
    if (!route.linesUsed.empty()) {
        cout << "Lines: ";
        for (size_t i = 0; i < route.linesUsed.size(); ++i)
            cout << (i ? " -> " : "") << route.linesUsed[i];
        cout << '\n';
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    const bool interactive = argc > 1 && string(argv[1]) == "--interactive";
    const bool jsonStations = argc > 1 && string(argv[1]) == "--json-stations";
    const bool jsonNetwork = argc > 1 && string(argv[1]) == "--json-network";
    const bool jsonRoute = argc > 1 && string(argv[1]) == "--json-route";
    const bool jsonMeetup = argc > 1 && string(argv[1]) == "--json-meetup";
    const bool routeCommand = argc > 1 && (string(argv[1]) == "--route" || jsonRoute);
    const bool machineCommand = jsonStations || jsonNetwork || jsonRoute || jsonMeetup;
    if (argc > 1 && string(argv[1]) == "--route" && argc < 4) {
        cerr << "Usage: metromitra.exe --route SOURCE_ID DESTINATION_ID [distance|time|transfers]\n";
        return 2;
    }
    const filesystem::path stations = argc > 1 && !interactive && !machineCommand ? argv[1] : "backend/data/stations.csv";
    const filesystem::path connections = argc > 2 && !interactive && !machineCommand ? argv[2] : "backend/data/connections.csv";
    try {
        const auto stationPath = routeCommand ? filesystem::path("backend/data/stations.csv") : stations;
        const auto connectionPath = routeCommand ? filesystem::path("backend/data/connections.csv") : connections;
        const auto graph = loadGraphCsv(stationPath, connectionPath);
        if (jsonStations) {
            cout << '[';
            const auto ids = graph.stationIds();
            for (size_t i = 0; i < ids.size(); ++i) {
                if (i) cout << ',';
                writeStationJson(*graph.findStation(ids[i]));
            }
            cout << "]\n";
            return 0;
        }
        if (jsonNetwork) {
            cout << "{\"stations\":[";
            const auto ids = graph.stationIds();
            for (size_t i = 0; i < ids.size(); ++i) {
                if (i) cout << ',';
                writeStationJson(*graph.findStation(ids[i]));
            }
            cout << "],\"connections\":[";
            bool first = true;
            for (const auto& id : ids) {
                for (const auto& edge : graph.neighbors(id)) {
                    if (id >= edge.destinationId) continue;
                    if (!first) cout << ',';
                    first = false;
                    cout << "{\"from\":" << jsonString(id)
                              << ",\"to\":" << jsonString(edge.destinationId)
                              << ",\"line\":" << jsonString(edge.line) << '}';
                }
            }
            cout << "]}\n";
            return 0;
        }
        if (jsonMeetup) {
            if (argc < 3) throw invalid_argument("Meetup requires comma-separated station IDs");
            return writeMeetupJson(graph, argv[2], argc > 3 ? argv[3] : "");
        }
        if (!jsonRoute) {
            cout << "MetroMitra graph ready: " << graph.stationCount()
                      << " stations loaded from CSV.\n";
        }
        if (interactive) return runInteractive(graph);
        if (routeCommand) {
            if (argc < (jsonRoute ? 5 : 4)) throw invalid_argument("Route command requires source and destination station IDs");
            RouteObjective objective = parseObjective(argc > 4 ? argv[4] : "time");
            const auto route = shortestRoute(graph, argv[2], argv[3], objective);
            if (jsonRoute) {
                cout << setprecision(10) << "{\"ok\":" << (!route.stationIds.empty() ? "true" : "false")
                          << ",\"distanceKm\":" << route.totalDistanceKm
                          << ",\"estimatedTimeMinutes\":" << route.estimatedTravelTimeMinutes
                          << ",\"transfers\":" << route.transferCount << ",\"lines\":[";
                for (size_t i = 0; i < route.linesUsed.size(); ++i)
                    cout << (i ? "," : "") << jsonString(route.linesUsed[i]);
                cout << "],\"stations\":[";
                for (size_t i = 0; i < route.stationIds.size(); ++i) {
                    if (i) cout << ',';
                    writeStationJson(*graph.findStation(route.stationIds[i]));
                }
                cout << "],\"segmentLines\":[";
                for (size_t i = 0; i < route.segmentLines.size(); ++i)
                    cout << (i ? "," : "") << jsonString(route.segmentLines[i]);
                cout << "]}\n";
                return 0;
            }
            if (route.stationIds.empty()) {
                cout << "No route found between those stations in this snapshot.\n";
                return 0;
            }
            const char* objectiveName = objective == RouteObjective::Distance ? "shortest distance"
                : objective == RouteObjective::FewestTransfers ? "fewest transfers" : "fastest time";
            cout << "Route (" << objectiveName << "):\n";
            for (size_t i = 0; i < route.stationIds.size(); ++i) {
                const auto* station = graph.findStation(route.stationIds[i]);
                cout << "  " << station->name << " [" << station->id << "]";
                if (i + 1 < route.stationIds.size()) cout << " ->";
                cout << '\n';
            }
            cout << "Distance: " << route.totalDistanceKm << " km\n"
                      << "Estimated edge travel time (no extra waits or dwell allowances): "
                      << route.estimatedTravelTimeMinutes << " min\n"
                      << "Line changes: " << route.transferCount << "\n";
            if (!route.linesUsed.empty()) {
                cout << "Lines: ";
                for (size_t i = 0; i < route.linesUsed.size(); ++i)
                    cout << (i ? " -> " : "") << route.linesUsed[i];
                cout << '\n';
            }
        }
    } catch (const exception& error) {
        cerr << "Could not load graph: " << error.what() << '\n';
        return 1;
    }
}
