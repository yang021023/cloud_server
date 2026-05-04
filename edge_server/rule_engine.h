#pragma once

#include "sensor_data.h"

#include <vector>
#include <string>

class RuleEngine {
public:
    std::vector<std::string> Evaluate(const SensorData& data) const;
};
