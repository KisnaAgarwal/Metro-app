#pragma once

#include <optional>
#include <string>

namespace metromitra {

// In the current GTFS import a vertex is a physical stop and `line` is the
// pipe-separated set of service lines using it. `physicalStationId` preserves
// the upstream stop ID. Future platform-level data can use separate vertices.
struct Station {
    std::string id;
    std::string physicalStationId;
    std::string name;
    std::string line;
    std::optional<double> latitude;
    std::optional<double> longitude;
    bool interchange = false;
    std::optional<double> dwellTimeMinutes;
};

} // namespace metromitra
