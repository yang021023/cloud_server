#include "cache_manager.h"

#include <fstream>

CacheManager::CacheManager(const std::string& file_path)
    : file_path_(file_path) {
}

void CacheManager::Push(const SensorData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(data);
    AppendToFile(data);
}

bool CacheManager::Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

size_t CacheManager::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void CacheManager::AppendToFile(const SensorData& data) {
    std::ofstream out(file_path_, std::ios::app);
    if (!out.is_open()) {
        return;
    }

    out << "{\"device_id\":\"" << data.device_id
        << "\",\"temperature\":" << data.temperature
        << ",\"humidity\":" << data.humidity
        << ",\"light\":" << data.light
        << ",\"timestamp\":" << data.timestamp
        << "}" << '\n';
}
