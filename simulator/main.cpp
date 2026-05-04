#include "config.h"
#include "device_simulator.h"

#include <csignal>
#include <exception>
#include <iostream>
#include <memory>

namespace {
std::unique_ptr<DeviceSimulator> g_simulator;

void SignalHandler(int) {
    if (g_simulator) {
        std::cout << "\n[Simulator] Stopping simulator..." << std::endl;
        g_simulator->Stop();
    }
}
}

int main(int argc, char* argv[]) {
    try {
        const SimulatorConfig config = ParseArguments(argc, argv);

        std::cout << "[Simulator] Starting single-device greenhouse simulator" << std::endl;
        std::cout << "[Simulator] Config file: " << config.config_path << std::endl;
        std::cout << "[Simulator] Device ID: " << config.device_id << std::endl;
        std::cout << "[Simulator] Scenario mode: " << config.scenario_mode << std::endl;
        std::cout << "[Simulator] Local endpoint: " << config.local_ip << ':' << config.local_port << std::endl;
        std::cout << "[Simulator] Location: " << config.location << std::endl;
        std::cout << "[Simulator] Edge server: " << config.server_ip << ':' << config.server_port << std::endl;
        std::cout << "[Simulator] Interval: " << config.interval_seconds << " seconds" << std::endl;
       
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        g_simulator = std::make_unique<DeviceSimulator>(config);
        g_simulator->Start();
        g_simulator->Wait();

        std::cout << "[Simulator] Exit." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[Simulator] Error: " << ex.what() << std::endl;
        PrintUsage();
        return 1;
    }
}
