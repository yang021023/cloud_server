#include "config.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string Trim(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }

    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(start, end - start);
}

std::vector<std::string> ReadConfigLines(const std::string& path) {
    namespace fs = std::filesystem;

    std::error_code ec;
    const fs::path current_dir = fs::current_path(ec);
    const fs::path requested_path(path);
    const fs::path config_file = requested_path.is_absolute()
        ? requested_path.lexically_normal()
        : (current_dir / requested_path).lexically_normal();
    const bool exists = fs::exists(config_file, ec);

    FILE* file = nullptr;
    const errno_t open_result = _wfopen_s(&file, config_file.c_str(), L"rb");
    if (open_result != 0 || !file) {
        std::ostringstream oss;
        oss << "Failed to open config file. cwd=" << current_dir.string()
            << ", requested=" << path
            << ", absolute=" << config_file.string()
            << ", exists=" << (exists ? "true" : "false")
            << ", open_result=" << open_result;
        throw std::runtime_error(oss.str());
    }

    std::vector<std::string> lines;
    char buffer[1024] = {0};
    while (std::fgets(buffer, sizeof(buffer), file) != nullptr) {
        lines.emplace_back(buffer);
    }

    std::fclose(file);
    return lines;
}

std::filesystem::path ResolveConfigPath(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path current_dir = fs::current_path(ec);
    const fs::path requested_path(path);
    return requested_path.is_absolute()
        ? requested_path.lexically_normal()
        : (current_dir / requested_path).lexically_normal();
}

void LoadConfigFile(SimulatorConfig& config, const std::string& path) {
    const auto lines = ReadConfigLines(path);

    for (const auto& raw_line : lines) {
        std::string line = Trim(raw_line);
        if (line.empty() || line[0] == '#') continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        const std::string key = Trim(line.substr(0, pos));
        const std::string value = Trim(line.substr(pos + 1));

        if (key == "device_id") config.device_id = value;
        else if (key == "local_ip") config.local_ip = value;
        else if (key == "local_port") config.local_port = std::stoi(value);
        else if (key == "location") config.location = value;
        else if (key == "server_ip") config.server_ip = value;
        else if (key == "server_port") config.server_port = std::stoi(value);
        else if (key == "interval_seconds") config.interval_seconds = std::stoi(value);
        else if (key == "cloud_ip") config.cloud_ip = value;
        else if (key == "cloud_port") config.cloud_port = std::stoi(value);
        else if (key == "crop_name") config.crop_name = value;
        else if (key == "scenario_mode") config.scenario_mode = value;
        else if (key == "csv_file_path") config.csv_file_path = value;
        else if (key == "day_temp_base") config.day_temp_base = std::stod(value);
        else if (key == "night_temp_base") config.night_temp_base = std::stod(value);
        else if (key == "day_light_max") config.day_light_max = std::stod(value);
        else if (key == "night_light_max") config.night_light_max = std::stod(value);
        else if (key == "day_start_hour") config.day_start_hour = std::stoi(value);
        else if (key == "night_start_hour") config.night_start_hour = std::stoi(value);
        else if (key == "anomaly_probability") config.anomaly_probability = std::stod(value);
        else if (key == "anomaly_temp_min") config.anomaly_temp_min = std::stod(value);
        else if (key == "anomaly_temp_max") config.anomaly_temp_max = std::stod(value);
        else if (key == "anomaly_duration_seconds") config.anomaly_duration_seconds = std::stoi(value);
        else if (key == "offline_probability") config.offline_probability = std::stod(value);
        else if (key == "offline_duration_min") config.offline_duration_min = std::stoi(value);
        else if (key == "offline_duration_max") config.offline_duration_max = std::stoi(value);
    }

    config.config_path = ResolveConfigPath(path).string();
}

void ValidateConfig(const SimulatorConfig& config) {
    if (config.config_path.empty()) throw std::runtime_error("config_path cannot be empty.");
    if (config.device_id.empty()) throw std::runtime_error("device_id must be provided in config or arguments.");
    if (config.local_ip.empty()) throw std::runtime_error("local_ip must be provided in config or arguments.");
    if (config.location.empty()) throw std::runtime_error("location must be provided in config or arguments.");
    if (config.server_ip.empty()) throw std::runtime_error("server_ip must be provided in config or arguments.");
    if (config.cloud_ip.empty()) throw std::runtime_error("cloud_ip must be provided in config or arguments.");
    if (config.server_port <= 0 || config.server_port > 65535) throw std::runtime_error("server_port must be between 1 and 65535.");
    if (config.cloud_port <= 0 || config.cloud_port > 65535) throw std::runtime_error("cloud_port must be between 1 and 65535.");
    if (config.local_port <= 0 || config.local_port > 65535) throw std::runtime_error("local_port must be between 1 and 65535.");
    if (config.interval_seconds <= 0) throw std::runtime_error("interval_seconds must be greater than 0.");

    if (config.scenario_mode == "csv" && config.csv_file_path.empty()) {
        throw std::runtime_error("csv_file_path must be provided when scenario_mode is csv.");
    }

    if (config.offline_probability < 0.0 || config.offline_probability > 1.0) {
        throw std::runtime_error("offline_probability must be between 0.0 and 1.0.");
    }
    if (config.offline_duration_min <= 0 || config.offline_duration_max <= 0) {
        throw std::runtime_error("offline_duration_min and offline_duration_max must be greater than 0.");
    }
    if (config.offline_duration_min > config.offline_duration_max) {
        throw std::runtime_error("offline_duration_min must be less than or equal to offline_duration_max.");
    }

    if (config.anomaly_probability < 0.0 || config.anomaly_probability > 1.0) {
        throw std::runtime_error("anomaly_probability must be between 0.0 and 1.0.");
    }
    if (config.anomaly_temp_min >= config.anomaly_temp_max) {
        throw std::runtime_error("anomaly_temp_min must be less than anomaly_temp_max.");
    }
}
}

bool UpdateConfigCropName(const std::string& config_path, const std::string& crop_name) {
    namespace fs = std::filesystem;

    const fs::path resolved_path = ResolveConfigPath(config_path);
    auto lines = ReadConfigLines(resolved_path.string());

    bool updated = false;
    for (auto& line : lines) {
        const std::string trimmed = Trim(line);
        if (trimmed.rfind("crop_name=", 0) == 0) {
            line = "crop_name=" + crop_name + "\n";
            updated = true;
            break;
        }
    }

    if (!updated) {
        lines.push_back("crop_name=" + crop_name + "\n");
    }

    FILE* file = nullptr;
    const errno_t open_result = _wfopen_s(&file, resolved_path.c_str(), L"wb");
    if (open_result != 0 || !file) {
        return false;
    }

    for (const auto& line : lines) {
        std::fwrite(line.data(), 1, line.size(), file);
    }

    std::fclose(file);
    return true;
}

void PrintUsage() {
    std::cout << "Usage:\n"
              << "  greenhouse_simulator [--config ../../../simulator/config/device_001.conf] [--device-id device_001] [--local-ip 127.0.0.1] [--local-port 9001] [--location ZoneA] [--ip 127.0.0.1] [--port 8888] [--interval 10] [--cloud-ip 127.0.0.1] [--cloud-port 8080] [--crop tomato]\n\n"
              << "Options:\n"
              << "  --config      Config file path, default: ../../../simulator/config/device_001.conf\n"
              << "  --device-id   Override device id from config\n"
              << "  --local-ip    Override local simulator IP from config\n"
              << "  --local-port  Override local simulator port from config\n"
              << "  --location    Override device location from config\n"
              << "  --ip          Override edge server IP from config\n"
              << "  --port        Override edge server TCP port from config\n"
              << "  --interval    Override data send interval from config\n"
              << "  --cloud-ip    Override cloud server REST IP from config\n"
              << "  --cloud-port  Override cloud server REST port from config\n"
              << "  --crop        Override crop name from config\n\n"
              << "Scenario Modes:\n"
              << "  --scenario    Simulation mode: random, daynight, anomaly, offline, csv, combined\n"
              << "  --csv-path    CSV file path for csv playback mode\n\n"
              << "Day/Night Settings:\n"
              << "  --day-temp    Day base temperature (default: 28.0)\n"
              << "  --night-temp  Night base temperature (default: 18.0)\n"
              << "  --day-light   Day max light (default: 800.0)\n"
              << "  --night-light Night max light (default: 50.0)\n"
              << "  --day-start   Day start hour (default: 6)\n"
              << "  --night-start Night start hour (default: 18)\n\n"
              << "Anomaly Settings:\n"
              << "  --anomaly-prob  Anomaly probability 0.0-1.0 (default: 0.05)\n"
              << "  --anomaly-min   Min anomaly temperature (default: 5.0)\n"
              << "  --anomaly-max   Max anomaly temperature (default: 50.0)\n"
              << "  --anomaly-dur   Anomaly duration in seconds (default: 30)\n\n"
              << "Offline Settings:\n"
              << "  --offline-prob  Offline probability 0.0-1.0 (default: 0.03)\n"
              << "  --offline-min   Min offline duration in seconds (default: 10)\n"
              << "  --offline-max   Max offline duration in seconds (default: 60)\n";
}

SimulatorConfig ParseArguments(int argc, char* argv[]) {
    SimulatorConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for argument: " + name);
            }
            return argv[++i];
        };

        if (arg == "--config") {
            config.config_path = require_value(arg);
        }
    }

    LoadConfigFile(config, config.config_path);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for argument: " + name);
            }
            return argv[++i];
        };

        if (arg == "--config") {
            ++i;
        } else if (arg == "--device-id") {
            config.device_id = require_value(arg);
        } else if (arg == "--local-ip") {
            config.local_ip = require_value(arg);
        } else if (arg == "--local-port") {
            config.local_port = std::stoi(require_value(arg));
        } else if (arg == "--location") {
            config.location = require_value(arg);
        } else if (arg == "--ip") {
            config.server_ip = require_value(arg);
        } else if (arg == "--port") {
            config.server_port = std::stoi(require_value(arg));
        } else if (arg == "--interval") {
            config.interval_seconds = std::stoi(require_value(arg));
        } else if (arg == "--cloud-ip") {
            config.cloud_ip = require_value(arg);
        } else if (arg == "--cloud-port") {
            config.cloud_port = std::stoi(require_value(arg));
        } else if (arg == "--crop") {
            config.crop_name = require_value(arg);
        } else if (arg == "--scenario") {
            config.scenario_mode = require_value(arg);
        } else if (arg == "--csv-path") {
            config.csv_file_path = require_value(arg);
        } else if (arg == "--day-temp") {
            config.day_temp_base = std::stod(require_value(arg));
        } else if (arg == "--night-temp") {
            config.night_temp_base = std::stod(require_value(arg));
        } else if (arg == "--day-light") {
            config.day_light_max = std::stod(require_value(arg));
        } else if (arg == "--night-light") {
            config.night_light_max = std::stod(require_value(arg));
        } else if (arg == "--day-start") {
            config.day_start_hour = std::stoi(require_value(arg));
        } else if (arg == "--night-start") {
            config.night_start_hour = std::stoi(require_value(arg));
        } else if (arg == "--anomaly-prob") {
            config.anomaly_probability = std::stod(require_value(arg));
        } else if (arg == "--anomaly-min") {
            config.anomaly_temp_min = std::stod(require_value(arg));
        } else if (arg == "--anomaly-max") {
            config.anomaly_temp_max = std::stod(require_value(arg));
        } else if (arg == "--anomaly-dur") {
            config.anomaly_duration_seconds = std::stoi(require_value(arg));
        } else if (arg == "--offline-prob") {
            config.offline_probability = std::stod(require_value(arg));
        } else if (arg == "--offline-min") {
            config.offline_duration_min = std::stoi(require_value(arg));
        } else if (arg == "--offline-max") {
            config.offline_duration_max = std::stoi(require_value(arg));
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    ValidateConfig(config);
    return config;
}
