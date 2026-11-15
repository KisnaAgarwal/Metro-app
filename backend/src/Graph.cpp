#include "Graph.h"

#include <stdexcept>
#include <algorithm>

using namespace std;

namespace metromitra {

bool Graph::addStation(const Station& station) {
    if (station.id.empty() || station.name.empty() || station.line.empty()) {
        return false;
    }
    const auto inserted = stations_.emplace(station.id, station).second;
    if (inserted) adjacency_.emplace(station.id, vector<Edge>{});
    return inserted;
}

bool Graph::addConnection(const string& fromId, const string& toId,
                          double distanceKm, double travelTimeMinutes,
                          const string& line, EdgeType type) {
    if (fromId == toId || stations_.count(fromId) == 0 || stations_.count(toId) == 0 ||
        distanceKm < 0.0 || travelTimeMinutes < 0.0 || line.empty()) {
        return false;
    }
    adjacency_.at(fromId).push_back({toId, distanceKm, travelTimeMinutes, line, type});
    adjacency_.at(toId).push_back({fromId, distanceKm, travelTimeMinutes, line, type});
    return true;
}

const Station* Graph::findStation(const string& id) const {
    const auto it = stations_.find(id);
    return it == stations_.end() ? nullptr : &it->second;
}

const vector<Edge>& Graph::neighbors(const string& id) const {
    const auto it = adjacency_.find(id);
    if (it == adjacency_.end()) throw out_of_range("Unknown station id: " + id);
    return it->second;
}

size_t Graph::stationCount() const noexcept { return stations_.size(); }

vector<string> Graph::stationIds() const {
    vector<string> ids;
    ids.reserve(stations_.size());
    for (const auto& entry : stations_) ids.push_back(entry.first);
    sort(ids.begin(), ids.end());
    return ids;
}

} // namespace metromitra
