#include "JsonOutput.h"
#include <iostream>

namespace metromitra {
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

} // namespace metromitra
