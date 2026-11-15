#include "CsvLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace metromitra {
namespace {
using Row = std::unordered_map<std::string, std::string>;

std::vector<std::string> parseCsvRow(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (quoted && ch == '"' && i + 1 < line.size() && line[i + 1] == '"') {
            field += '"'; ++i;
        } else if (ch == '"') {
            quoted = !quoted;
        } else if (ch == ',' && !quoted) {
            fields.push_back(field); field.clear();
        } else {
            field += ch;
        }
    }
    if (quoted) throw std::runtime_error("unterminated quoted CSV field");
    fields.push_back(field);
    return fields;
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

std::ifstream openCsv(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open CSV: " + path.string());
    return input;
}

std::vector<std::string> readHeader(std::ifstream& input, const std::filesystem::path& path) {
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("Empty CSV: " + path.string());
    if (!line.empty() && line.front() == '\xEF') line.erase(0, 3); // UTF-8 BOM
    auto fields = parseCsvRow(line);
    for (auto& field : fields) field = trim(field);
    return fields;
}

std::size_t column(const std::vector<std::string>& header, const std::string& name,
                   const std::filesystem::path& path) {
    for (std::size_t i = 0; i < header.size(); ++i) if (header[i] == name) return i;
    throw std::runtime_error("Missing column '" + name + "' in " + path.string());
}

std::vector<Row> readRows(const std::filesystem::path& path, std::vector<std::string>& header) {
    auto input = openCsv(path);
    header = readHeader(input, path);
    std::vector<Row> rows;
    std::string line;
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) continue;
        try {
            const auto values = parseCsvRow(line);
            if (values.size() != header.size()) throw std::runtime_error("column count does not match header");
            Row row;
            for (std::size_t i = 0; i < header.size(); ++i) row.emplace(header[i], trim(values[i]));
            rows.push_back(std::move(row));
        } catch (const std::exception& e) {
            throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) + ": " + e.what());
        }
    }
    return rows;
}

double parseNonnegative(const Row& row, const std::string& key) {
    try {
        std::size_t used = 0;
        const double value = std::stod(row.at(key), &used);
        if (used != row.at(key).size() || value < 0.0) throw std::runtime_error("invalid nonnegative number");
        return value;
    } catch (...) {
        throw std::runtime_error("Invalid value for '" + key + "': " + row.at(key));
    }
}

std::optional<double> optionalCoordinate(const Row& row, const std::string& key) {
    if (row.at(key).empty()) return std::nullopt;
    std::size_t used = 0;
    double value;
    try { value = std::stod(row.at(key), &used); }
    catch (...) { throw std::runtime_error("Invalid coordinate for '" + key + "': " + row.at(key)); }
    if (used != row.at(key).size() || (key == "latitude" && (value < -90.0 || value > 90.0)) ||
        (key == "longitude" && (value < -180.0 || value > 180.0)))
        throw std::runtime_error("Coordinate out of range for '" + key + "': " + row.at(key));
    return value;
}
} // namespace

Graph loadGraphCsv(const std::filesystem::path& stationsPath,
                   const std::filesystem::path& connectionsPath) {
    std::vector<std::string> stationHeader, connectionHeader;
    const auto stations = readRows(stationsPath, stationHeader);
    const auto connections = readRows(connectionsPath, connectionHeader);
    for (const auto& name : {"id", "physical_station_id", "name", "line", "latitude", "longitude", "interchange"})
        column(stationHeader, name, stationsPath);
    const bool hasDwellTime = std::find(stationHeader.begin(), stationHeader.end(), "dwell_time_minutes") != stationHeader.end();
    for (const auto& name : {"source_id", "destination_id", "distance_km", "travel_time_minutes", "line", "type"})
        column(connectionHeader, name, connectionsPath);

    Graph graph;
    for (const auto& row : stations) {
        const auto flag = row.at("interchange");
        if (flag != "true" && flag != "false") throw std::runtime_error("Invalid interchange flag: " + flag);
        Station station{row.at("id"), row.at("physical_station_id"), row.at("name"), row.at("line"),
                        optionalCoordinate(row, "latitude"), optionalCoordinate(row, "longitude"), flag == "true",
                        hasDwellTime && !row.at("dwell_time_minutes").empty()
                            ? std::optional<double>(parseNonnegative(row, "dwell_time_minutes"))
                            : std::nullopt};
        if (station.physicalStationId.empty() || !graph.addStation(station))
            throw std::runtime_error("Invalid or duplicate station id: " + station.id);
    }
    for (const auto& row : connections) {
        EdgeType type;
        const auto rawType = row.at("type");
        if (rawType == "ride") type = EdgeType::Ride;
        else if (rawType == "transfer") type = EdgeType::Transfer;
        else if (rawType == "walk") type = EdgeType::Walk;
        else throw std::runtime_error("Invalid edge type: " + rawType);
        if (!graph.addConnection(row.at("source_id"), row.at("destination_id"),
                                 parseNonnegative(row, "distance_km"),
                                 parseNonnegative(row, "travel_time_minutes"), row.at("line"), type))
            throw std::runtime_error("Invalid connection: " + row.at("source_id") + " -> " + row.at("destination_id"));
    }
    return graph;
}

} // namespace metromitra
