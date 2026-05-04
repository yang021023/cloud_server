#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class GrpcClientStub;

class NetworkMonitor {
public:
    explicit NetworkMonitor(const GrpcClientStub& client);
    ~NetworkMonitor();

    void Start();
    void Stop();
    bool IsOnline() const;
    std::string CurrentMode() const;

private:
    void Run();

    const GrpcClientStub& client_;
    std::atomic<bool> running_{false};
    std::atomic<bool> online_{true};
    std::thread worker_;
    mutable std::mutex mutex_;
    int consecutive_failures_ = 0;
};
