#include "network_monitor.h"

#include "grpc_client_stub.h"
#include "logger.h"

#include <chrono>

NetworkMonitor::NetworkMonitor(const GrpcClientStub& client)
    : client_(client) {
}

NetworkMonitor::~NetworkMonitor() {
    Stop();
}

void NetworkMonitor::Start() {
    if (running_.exchange(true)) {
        return;
    }

    worker_ = std::thread(&NetworkMonitor::Run, this);
}

void NetworkMonitor::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

bool NetworkMonitor::IsOnline() const {
    return online_.load();
}

std::string NetworkMonitor::CurrentMode() const {
    return IsOnline() ? "ONLINE" : "OFFLINE";
}

void NetworkMonitor::Run() {
    Logger::Info("[NetworkMonitor] Started heartbeat detection, interval=10s, failure_threshold=3");

    while (running_.load()) {
        const bool success = client_.SendHeartbeat();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (success) {
                if (!online_.load()) {
                    online_ = true;
                    Logger::Info("[NetworkMonitor] Cloud heartbeat restored, switched to ONLINE mode");
                }
                consecutive_failures_ = 0;
            } else {
                ++consecutive_failures_;
                Logger::Error("[NetworkMonitor] Heartbeat failed, consecutive_failures=" + std::to_string(consecutive_failures_));
                if (consecutive_failures_ >= 3 && online_.load()) {
                    online_ = false;
                    Logger::Info("[NetworkMonitor] Cloud unreachable, switched to OFFLINE mode");
                }
            }
        }

        for (int i = 0; i < 10 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    Logger::Info("[NetworkMonitor] Stopped.");
}
