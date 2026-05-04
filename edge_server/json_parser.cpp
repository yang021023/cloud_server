#include "json_parser.h"

#include <cstdlib>
#include <string>

namespace {
bool ExtractStringValue(const std::string& json, const std::string& key, std::string& value) {
    const std::string pattern = "\"" + key + "\":\"";
    const size_t start = json.find(pattern);
    if (start == std::string::npos) {
        return false;
    }

    const size_t value_start = start + pattern.size();
    const size_t value_end = json.find('"', value_start);
    if (value_end == std::string::npos) {
        return false;
    }

    value = json.substr(value_start, value_end - value_start);
    return true;
}

bool ExtractNumberValue(const std::string& json, const std::string& key, std::string& value) {
    const std::string pattern = "\"" + key + "\":";
    const size_t start = json.find(pattern);
    if (start == std::string::npos) {
        return false;
    }

    const size_t value_start = start + pattern.size();
    size_t value_end = value_start;
    while (value_end < json.size() && json[value_end] != ',' && json[value_end] != '}') {
        ++value_end;
    }

    value = json.substr(value_start, value_end - value_start);
    return !value.empty();
}
}

bool JsonParser::ParseSensorData(const std::string& json, SensorData& out_data, std::string& error) {
    std::string device_id;
    std::string crop_name;
    std::string location;
    std::string temperature;
    std::string humidity;
    std::string light;
    std::string timestamp;

    if (!ExtractStringValue(json, "device_id", device_id)) {
        error = "Missing device_id";
        return false;
    }
    ExtractStringValue(json, "crop_name", crop_name);
    ExtractStringValue(json, "location", location);
    if (!ExtractNumberValue(json, "temperature", temperature)) {
        error = "Missing temperature";
        return false;
    }
    if (!ExtractNumberValue(json, "humidity", humidity)) {
        error = "Missing humidity";
        return false;
    }
    if (!ExtractNumberValue(json, "light", light)) {
        error = "Missing light";
        return false;
    }
    if (!ExtractNumberValue(json, "timestamp", timestamp)) {
        error = "Missing timestamp";
        return false;
    }

    try {
        out_data.device_id = device_id;
        out_data.crop_name = crop_name.empty() ? "default" : crop_name;
        out_data.location = location;
        out_data.temperature = std::stod(temperature);
        out_data.humidity = std::stod(humidity);
        out_data.light = std::stod(light);
        out_data.timestamp = std::stoll(timestamp);
        return true;
    } catch (...) {
        error = "Invalid numeric field";
        return false;
    }
}
