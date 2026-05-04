#include "device_monitor.h"

#include <ctime>

void DeviceMonitor::UpdateSeen(const std::string& device_id, long long timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_seen_map_[device_id] = timestamp;
}

std::vector<DeviceStatus> DeviceMonitor::Snapshot() const {
    std::vector<DeviceStatus> statuses;
    const long long now = static_cast<long long>(std::time(nullptr));

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [device_id, last_seen] : last_seen_map_) {
        DeviceStatus status;
        status.device_id = device_id;
        status.last_seen = last_seen;
        status.online = (now - last_seen) <= 10;
        statuses.push_back(status);
    }

    return statuses;
}
