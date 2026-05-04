#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

struct DeviceStatus {
    std::string device_id;
    long long last_seen = 0;
    bool online = false;
};

class DeviceMonitor {
public:
    void UpdateSeen(const std::string& device_id, long long timestamp);
    std::vector<DeviceStatus> Snapshot() const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, long long> last_seen_map_;
};
