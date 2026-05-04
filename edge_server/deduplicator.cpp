#include "deduplicator.h"

bool Deduplicator::IsDuplicate(const SensorData& data) {
    const std::string key = data.device_id + "_" + std::to_string(data.timestamp);
    std::lock_guard<std::mutex> lock(mutex_);
    const auto [it, inserted] = seen_keys_.insert(key);
    return !inserted;
}
