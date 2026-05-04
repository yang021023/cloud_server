#pragma once

#include <string>

struct SensorData {
    std::string device_id;
    std::string crop_name;
    std::string location;
    double temperature = 0.0;
    double humidity = 0.0;
    double light = 0.0;
    long long timestamp = 0;
};
