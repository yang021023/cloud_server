#pragma once

#include "sensor_data.h"

#include <mutex>
#include <set>
#include <string>

class Deduplicator {
public:
    bool IsDuplicate(const SensorData& data);

private:
    std::mutex mutex_;
    std::set<std::string> seen_keys_;
};
