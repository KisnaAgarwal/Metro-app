#pragma once

#include "Graph.h"

#include <filesystem>

namespace metromitra {

// Loads two related CSV files. Returns a graph or throws std::runtime_error
// with file/row context if the data is malformed or references unknown IDs.
Graph loadGraphCsv(const std::filesystem::path& stationsPath,
                   const std::filesystem::path& connectionsPath);

} // namespace metromitra
