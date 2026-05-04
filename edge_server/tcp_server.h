#pragma once

#include "cache_manager.h"
#include "crop_config.h"
#include "deduplicator.h"
#include "device_monitor.h"
#include "grpc_client_stub.h"
#include "network_monitor.h"
#include "offline_controller.h"
#include "rule_engine.h"

#include <memory>

class TcpServer {
public:
    explicit TcpServer(int port);
    ~TcpServer();

    bool Start();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
