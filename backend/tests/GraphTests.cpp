#include "CsvLoader.h"
#include "Dijkstra.h"
#include "Traversal.h"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace metromitra;
    try {
        Graph graph;
        for (const auto& id : {"a", "b", "c", "isolated"})
            require(graph.addStation({id, id, id, "L", {}, {}, false, {}}), "station insert failed");
        require(!graph.addStation({"a", "a", "duplicate", "L", {}, {}, false, {}}), "duplicate station accepted");
        require(graph.addConnection("a", "b", 1.0, 2.0, "L"), "edge insert failed");
        require(graph.addConnection("b", "c", 1.0, 2.0, "L"), "edge insert failed");
        require(graph.findStation("c") != nullptr && graph.findStation("missing") == nullptr, "lookup failed");
        require(graph.neighbors("b").size() == 2, "undirected adjacency failed");
        require(breadthFirstTraversal(graph, "a") == std::vector<std::string>({"a", "b", "c"}), "BFS order/connectivity failed");
        require(depthFirstTraversal(graph, "a") == std::vector<std::string>({"a", "b", "c"}), "DFS order/connectivity failed");
        require(breadthFirstTraversal(graph, "isolated") == std::vector<std::string>({"isolated"}), "isolated BFS failed");
        bool rejected = false;
        try { breadthFirstTraversal(graph, "missing"); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "unknown traversal start accepted");

        Graph weighted;
        for (const auto& id : {"s", "x", "y", "t", "island"})
            require(weighted.addStation({id, id, id, "L", {}, {}, false, {}}), "weighted station insert failed");
        weighted.addConnection("s", "x", 1.0, 9.0, "L");
        weighted.addConnection("x", "t", 1.0, 9.0, "L");
        weighted.addConnection("s", "y", 4.0, 2.0, "L");
        weighted.addConnection("y", "t", 4.0, 2.0, "L", EdgeType::Transfer);
        weighted.addConnection("s", "t", 5.0, 15.0, "L");
        const auto byDistance = shortestRoute(weighted, "s", "t", RouteObjective::Distance);
        require(byDistance.stationIds == std::vector<std::string>({"s", "x", "t"}), "distance Dijkstra path failed");
        require(byDistance.totalDistanceKm == 2.0 && byDistance.estimatedTravelTimeMinutes == 18.75, "distance route metrics failed");
        const auto byTime = shortestRoute(weighted, "s", "t", RouteObjective::TravelTime);
        require(byTime.stationIds == std::vector<std::string>({"s", "y", "t"}), "time Dijkstra path failed");
        require(byTime.estimatedTravelTimeMinutes == 4.0 && byTime.totalDistanceKm == 8.0, "time route metrics failed");
        require(byTime.transferCount == 1, "transfer count failed");
        const auto fewest = shortestRoute(weighted, "s", "t", RouteObjective::FewestTransfers);
        require(fewest.stationIds == std::vector<std::string>({"s", "t"}) && fewest.transferCount == 0,
                "fewest-transfers objective failed");
        require(shortestRoute(weighted, "s", "s", RouteObjective::Distance).stationIds == std::vector<std::string>({"s"}), "same-station route failed");
        require(shortestRoute(weighted, "s", "island", RouteObjective::Distance).stationIds.empty(), "unreachable route should be empty");

        Graph lineSwitch;
        lineSwitch.addStation({"p", "p", "Before interchange", "BLUE", {}, {}, false, {}});
        lineSwitch.addStation({"hub", "hub", "Interchange", "BLUE|YELLOW", {}, {}, true, {}});
        lineSwitch.addStation({"q", "q", "After interchange", "YELLOW", {}, {}, false, {}});
        lineSwitch.addConnection("p", "hub", 1.0, 2.0, "BLUE");
        lineSwitch.addConnection("hub", "q", 1.0, 2.0, "YELLOW");
        const auto switched = shortestRoute(lineSwitch, "p", "q", RouteObjective::TravelTime);
        require(switched.transferCount == 1 && switched.linesUsed == std::vector<std::string>({"BLUE", "YELLOW"}) &&
                    switched.estimatedTravelTimeMinutes == 6.0,
                "line-switch transfer accounting failed");

        Graph largeInterchange;
        largeInterchange.addStation({"before", "before", "Before", "BLUE", {}, {}, false, {}});
        largeInterchange.addStation({"hub3", "hub3", "Three-line interchange", "BLUE|PINK|YELLOW", {}, {}, true, {}});
        largeInterchange.addStation({"after", "after", "After", "YELLOW", {}, {}, false, {}});
        largeInterchange.addConnection("before", "hub3", 1.0, 2.0, "BLUE");
        largeInterchange.addConnection("hub3", "after", 1.0, 2.0, "YELLOW");
        require(shortestRoute(largeInterchange, "before", "after", RouteObjective::TravelTime)
                    .estimatedTravelTimeMinutes == 7.0,
                "large interchange should use three-minute allowance");

        Graph dwellGraph;
        dwellGraph.addStation({"a", "a", "Origin", "L", {}, {}, false, 50.0});
        dwellGraph.addStation({"b", "b", "Long dwell", "L", {}, {}, false, 15.0});
        dwellGraph.addStation({"c", "c", "Short dwell", "L", {}, {}, false, 0.0});
        dwellGraph.addStation({"d", "d", "Destination", "L", {}, {}, false, 99.0});
        dwellGraph.addConnection("a", "b", 1.0, 1.0, "L");
        dwellGraph.addConnection("b", "d", 1.0, 1.0, "L");
        dwellGraph.addConnection("a", "c", 3.0, 3.0, "L");
        dwellGraph.addConnection("c", "d", 3.0, 3.0, "L");
        const auto withDwell = shortestRoute(dwellGraph, "a", "d", RouteObjective::TravelTime);
        require(withDwell.stationIds == std::vector<std::string>({"a", "c", "d"}), "intermediate dwell not used in time routing");
        require(withDwell.estimatedTravelTimeMinutes == 6.0, "origin/destination dwell should not be included");

        const auto root = std::filesystem::path(METROMITRA_SOURCE_DIR) / "backend" / "data";
        const auto sample = loadGraphCsv(root / "stations_sample.csv", root / "connections_sample.csv");
        const auto loaded = loadGraphCsv(root / "stations.csv", root / "connections.csv");
        require(sample.stationCount() == 3, "synthetic sample loading failed");
        require(loaded.stationCount() == 262, "2023 network station count mismatch");
        require(loaded.findStation("dmrc_5")->interchange, "GTFS interchange station not marked");
        require(!loaded.findStation("dmrc_5")->dwellTimeMinutes.has_value(), "GTFS dwell should use the configured 45-second default");
        require(shortestRoute(loaded, "dmrc_1", "dmrc_2", RouteObjective::Distance).stationIds ==
                    std::vector<std::string>({"dmrc_1", "dmrc_2"}), "real network route failed");
        require(!sample.findStation("demo_a")->latitude.has_value(), "blank synthetic coordinate should remain absent");
        std::cout << "All graph, traversal, and CSV tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
