#pragma once

#include "Station.h"
#include <string>

namespace metromitra {
std::string jsonString(const std::string& value);
void writeStationJson(const Station& station);
}
