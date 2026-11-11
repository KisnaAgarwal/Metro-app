#pragma once

#include "Station.h"

#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace metromitra {

enum class EdgeType { Ride, Transfer, Walk };

struct Edge {
    std::string destinationId;
    double distanceKm = 0.0;
    double travelTimeMinutes = 0.0;
    std::string line;
    EdgeType type = EdgeType::Ride;
};

class Graph {
public:
    bool addStation(const Station& station);
    bool addConnection(const std::string& fromId, const std::string& toId,
                       double distanceKm, double travelTimeMinutes,
                       const std::string& line, EdgeType type = EdgeType::Ride);

    const Station* findStation(const std::string& id) const;
    const std::vector<Edge>& neighbors(const std::string& id) const;
    std::size_t stationCount() const noexcept;
    std::vector<std::string> stationIds() const;

private:
    std::unordered_map<std::string, Station> stations_;
    std::unordered_map<std::string, std::vector<Edge>> adjacency_;
};

} // namespace metromitra
