#include "JsonOutput.h"
#include <iostream>

using namespace std;

namespace metromitra {
string jsonString(const string& value) {
    string result = "\"";
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

void writeStationJson(const Station& station) {
    cout << "{\"id\":" << jsonString(station.id)
              << ",\"name\":" << jsonString(station.name)
              << ",\"line\":" << jsonString(station.line)
              << ",\"latitude\":";
    if (station.latitude) cout << *station.latitude;
    else cout << "null";
    cout << ",\"longitude\":";
    if (station.longitude) cout << *station.longitude;
    else cout << "null";
    cout << ",\"interchange\":" << (station.interchange ? "true" : "false") << '}';
}

} // namespace metromitra
