#pragma once

#include "sensor_data.h"

#include <string>

class JsonParser {
public:
    static bool ParseSensorData(const std::string& json, SensorData& out_data, std::string& error);
};
