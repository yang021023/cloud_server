#pragma once

#include "sensor_data.h"

#include <mutex>
#include <queue>
#include <string>

class CacheManager {
public:
    explicit CacheManager(const std::string& file_path);

    void Push(const SensorData& data);
    bool Empty() const;
    size_t Size() const;

private:
    std::string file_path_;
    mutable std::mutex mutex_;
    std::queue<SensorData> queue_;

    void AppendToFile(const SensorData& data);
};
