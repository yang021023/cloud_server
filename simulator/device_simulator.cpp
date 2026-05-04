#include "device_simulator.h"

#include "tcp_client.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

namespace {
double RandomRange(std::mt19937& generator, double min_value, double max_value) {
    std::uniform_real_distribution<double> distribution(min_value, max_value);
    return distribution(generator);
}

long long CurrentUnixTimestamp() {
    return static_cast<long long>(std::time(nullptr));
}

std::string ExtractBody(const std::string& response) {
    const auto pos = response.find("\r\n\r\n");
    return pos == std::string::npos ? std::string{} : response.substr(pos + 4);
}

std::string ExtractJsonField(const std::string& body, const std::string& key) {
    const std::string pattern = "\"" + key + "\":\"";
    const auto start = body.find(pattern);
    if (start == std::string::npos) {
        return "";
    }
    const auto value_start = start + pattern.size();
    const auto value_end = body.find('"', value_start);
    if (value_end == std::string::npos) {
        return "";
    }
    return body.substr(value_start, value_end - value_start);
}

std::string ScenarioModeToString(ScenarioType type) {
    switch (type) {
        case ScenarioType::Random: return "random";
        case ScenarioType::DayNight: return "daynight";
        case ScenarioType::Anomaly: return "anomaly";
        case ScenarioType::DeviceOffline: return "offline";
        case ScenarioType::CsvPlayback: return "csv";
        case ScenarioType::Combined: return "combined";
        default: return "unknown";
    }
}
}

DeviceSimulator::DeviceSimulator(const SimulatorConfig& config)
    : config_(config),
      current_crop_(config.crop_name.empty() ? "default" : config.crop_name),
      scenario_type_(GetScenarioTypeFromString(config_.scenario_mode)),
      csv_data_index_(0),
      is_offline_(false),
      offline_until_(0),
      anomaly_active_(false),
      anomaly_until_(0),
      last_normal_temp_(25.0),
      csv_data_() {
    std::random_device rd;
    scenario_rng_.seed(rd() ^ static_cast<unsigned int>(std::hash<std::string>{}(config.device_id)));
    LoadCsvData();
}

DeviceSimulator::~DeviceSimulator() {
    Stop();
    Wait();
}

void DeviceSimulator::Start() {
    if (running_) {
        return;
    }

    InitializeCropSelection();
    running_ = true;
    crop_sync_worker_ = std::thread(&DeviceSimulator::RunCropSync, this);
    worker_ = std::thread(&DeviceSimulator::RunDevice, this);
}

void DeviceSimulator::Wait() {
    if (worker_.joinable()) {
        worker_.join();
    }
    if (crop_sync_worker_.joinable()) {
        crop_sync_worker_.join();
    }
}

void DeviceSimulator::Stop() {
    running_ = false;
}

void DeviceSimulator::InitializeCropSelection() {
    const std::string crop_from_cloud = FetchCropFromCloud();
    if (!crop_from_cloud.empty()) {
        SetCurrentCrop(crop_from_cloud);
        PersistCurrentCrop(crop_from_cloud);
        std::cout << "[Simulator][CROP] device=" << config_.device_id << " source=cloud crop=" << crop_from_cloud << std::endl;
        return;
    }

    if (!config_.crop_name.empty()) {
        SetCurrentCrop(config_.crop_name);
        PersistCurrentCrop(config_.crop_name);
        std::cout << "[Simulator][CROP] device=" << config_.device_id << " source=config crop=" << config_.crop_name << std::endl;
        return;
    }

    const std::string manual_crop = PromptManualCrop();
    SetCurrentCrop(manual_crop);
    PersistCurrentCrop(manual_crop);
    std::cout << "[Simulator][CROP] device=" << config_.device_id << " source=manual-input crop=" << manual_crop << std::endl;
}

std::string DeviceSimulator::PromptManualCrop() const {
    std::cout << "[Simulator] Cloud crop sync unavailable. Enter manual crop for device " << config_.device_id << " (press Enter for default): ";
    std::string manual_crop;
    std::getline(std::cin, manual_crop);
    return manual_crop.empty() ? "default" : manual_crop;
}

void DeviceSimulator::RunCropSync() {
    while (running_) {
        const std::string crop_from_cloud = FetchCropFromCloud();
        if (!crop_from_cloud.empty() && crop_from_cloud != GetCurrentCrop()) {
            SetCurrentCrop(crop_from_cloud);
            PersistCurrentCrop(crop_from_cloud);
            std::cout << "[Simulator][CROP] device=" << config_.device_id << " source=cloud crop=" << crop_from_cloud << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void DeviceSimulator::PrintSendLog(const SensorData& data, const std::string& crop_name) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "[Simulator][SEND]"
        << " device=" << data.device_id
        << " crop=" << crop_name
        << " local_ip=" << config_.local_ip
        << " local_port=" << config_.local_port
        << " location=" << config_.location
        << " temp=" << data.temperature << "C"
        << " humidity=" << data.humidity << "%"
        << " light=" << data.light << "lux"
        << " ts=" << data.timestamp;
    std::cout << oss.str() << std::endl;
}

void DeviceSimulator::RunDevice() {
    TcpClient client(config_.server_ip, config_.server_port);
    std::random_device rd;
    std::mt19937 generator(rd() ^ static_cast<unsigned int>(std::hash<std::string>{}(config_.device_id)));

    std::cout << "[Simulator][START] device=" << config_.device_id
              << " scenario=" << config_.scenario_mode
              << " interval=" << config_.interval_seconds << "s" << std::endl;

    while (running_) {
        bool currently_offline = false;

        if (config_.scenario_mode == "offline") {
            UpdateOfflineState();

            if (ShouldGoOffline()) {
                std::uniform_int_distribution<int> duration_dist(
                    config_.offline_duration_min, config_.offline_duration_max);
                offline_until_ = CurrentUnixTimestamp() + duration_dist(scenario_rng_);
                is_offline_ = true;

                std::cout << "[Simulator][OFFLINE] Device going offline until "
                          << offline_until_ << " (" << config_.offline_duration_min << "-"
                          << config_.offline_duration_max << "s)" << std::endl;
            }

            {
                std::lock_guard<std::mutex> lock(offline_mutex_);
                currently_offline = is_offline_;
            }

            if (currently_offline) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
        }

        if (!client.IsConnected()) {
            std::cout << "[Simulator][CONNECT] device=" << config_.device_id << " target="
                      << config_.server_ip << ':' << config_.server_port << std::endl;
            if (!client.Connect()) {
                std::cout << "[Simulator][RETRY] device=" << config_.device_id << " reason=connect_failed wait=5s" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            std::cout << "[Simulator][CONNECTED] device=" << config_.device_id << std::endl;
        }

        SensorData data = GenerateData();

        ScenarioType scenario = GetScenarioType();
        switch (scenario) {
            case ScenarioType::DayNight:
                data = GenerateDayNightData(generator);
                break;
            case ScenarioType::Anomaly:
                data = GenerateAnomalyData(generator);
                break;
            case ScenarioType::CsvPlayback:
                data = GenerateCsvData(generator);
                break;
            case ScenarioType::Combined:
                data = GenerateCombinedData(generator);
                break;
            case ScenarioType::Random:
            default:
                data.temperature = RandomRange(generator, 20.0, 40.0);
                data.humidity = RandomRange(generator, 30.0, 80.0);
                data.light = RandomRange(generator, 100.0, 1000.0);
                data.timestamp = CurrentUnixTimestamp();
                data.scenario_mode = "random";
                break;
        }

        const std::string json = ToJson(data);
        const std::string crop_name = data.crop_name;
        if (client.SendLine(json)) {
            PrintSendLog(data, crop_name);
            std::this_thread::sleep_for(std::chrono::seconds(config_.interval_seconds));
        } else {
            std::cout << "[Simulator][RETRY] device=" << config_.device_id << " reason=send_failed wait=5s" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    client.Close();
}

SensorData DeviceSimulator::GenerateData() const {
    SensorData data;
    data.device_id = config_.device_id;
    data.crop_name = GetCurrentCrop();
    data.location = config_.location;
    return data;
}

std::string DeviceSimulator::ToJson(const SensorData& data) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "{"
        << "\"device_id\":\"" << data.device_id << "\"," 
        << "\"crop_name\":\"" << data.crop_name << "\"," 
        << "\"location\":\"" << data.location << "\"," 
        << "\"temperature\":" << data.temperature << ","
        << "\"humidity\":" << data.humidity << ","
        << "\"light\":" << data.light << ","
        << "\"timestamp\":" << data.timestamp
        << "}";
    return oss.str();
}

std::string DeviceSimulator::GetCurrentCrop() const {
    std::lock_guard<std::mutex> lock(crop_mutex_);
    return current_crop_.empty() ? "default" : current_crop_;
}

void DeviceSimulator::SetCurrentCrop(const std::string& crop_name) {
    std::lock_guard<std::mutex> lock(crop_mutex_);
    current_crop_ = crop_name.empty() ? "default" : crop_name;
}

void DeviceSimulator::PersistCurrentCrop(const std::string& crop_name) {
    const std::lock_guard<std::mutex> lock(config_mutex_);
    const std::string effective_crop = crop_name.empty() ? "default" : crop_name;
    config_.crop_name = effective_crop;
    UpdateConfigCropName(config_.config_path, effective_crop);
}

std::string DeviceSimulator::FetchCropFromCloud() const {
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return "";
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return "";
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<u_short>(config_.cloud_port));
    if (InetPtonA(AF_INET, config_.cloud_ip.c_str(), &server_addr.sin_addr) != 1) {
        closesocket(sock);
        WSACleanup();
        return "";
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return "";
    }

    std::ostringstream req;
    req << "GET /api/crop?device_id=" << config_.device_id << " HTTP/1.1\r\n"
        << "Host: " << config_.cloud_ip << ':' << config_.cloud_port << "\r\n"
        << "Connection: close\r\n\r\n";
    const std::string request = req.str();
    if (send(sock, request.c_str(), static_cast<int>(request.size()), 0) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return "";
    }

    std::string response;
    char buffer[1024] = {0};
    int received = 0;
    do {
        received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            response.append(buffer, buffer + received);
        }
    } while (received > 0);

    closesocket(sock);
    WSACleanup();

    if (response.find("200 OK") == std::string::npos) {
        return "";
    }

    return ExtractJsonField(ExtractBody(response), "crop_name");
}

ScenarioType GetScenarioTypeFromString(const std::string& mode) {
    if (mode == "daynight") return ScenarioType::DayNight;
    if (mode == "anomaly") return ScenarioType::Anomaly;
    if (mode == "offline") return ScenarioType::DeviceOffline;
    if (mode == "csv") return ScenarioType::CsvPlayback;
    if (mode == "combined") return ScenarioType::Combined;
    return ScenarioType::Random;
}

ScenarioType DeviceSimulator::GetScenarioType() const {
    std::lock_guard<std::mutex> lock(scenario_mutex_);
    return scenario_type_;
}

void DeviceSimulator::LoadCsvData() {
    if (config_.scenario_mode != "csv" || config_.csv_file_path.empty()) {
        return;
    }

    std::ifstream file(config_.csv_file_path);
    if (!file.is_open()) {
        std::cerr << "[Simulator][CSV] Failed to open CSV file: " << config_.csv_file_path << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line);  // Skip header

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        SensorData data;
        data.device_id = config_.device_id;
        data.crop_name = GetCurrentCrop();
        data.location = config_.location;

        std::getline(ss, token, ',');
        data.timestamp = std::stoll(token);
        std::getline(ss, token, ',');
        data.temperature = std::stod(token);
        std::getline(ss, token, ',');
        data.humidity = std::stod(token);
        std::getline(ss, token, ',');
        data.light = std::stod(token);

        csv_data_.push_back(data);
    }

    csv_data_index_ = 0;
    std::cout << "[Simulator][CSV] Loaded " << csv_data_.size() << " records from " << config_.csv_file_path << std::endl;
}

double DeviceSimulator::GetTimeOfDayFactor() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = {};
    localtime_s(&local_tm, &now_time);
    int hour = local_tm.tm_hour;
    int minute = local_tm.tm_min;

    double time_factor = hour + minute / 60.0;

    if (config_.scenario_mode == "combined" || config_.scenario_mode == "daynight") {
        if (time_factor >= config_.day_start_hour && time_factor < config_.night_start_hour) {
            double day_progress = (time_factor - config_.day_start_hour) /
                                  (config_.night_start_hour - config_.day_start_hour);
            return day_progress;
        } else {
            if (time_factor >= config_.night_start_hour) {
                double night_progress = (time_factor - config_.night_start_hour) / 24.0;
                return -night_progress;
            } else {
                double night_progress = (time_factor + 24.0 - config_.night_start_hour) / 24.0;
                return -night_progress;
            }
        }
    }

    return 0.5;
}

bool DeviceSimulator::IsNight() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = {};
    localtime_s(&local_tm, &now_time);
    int hour = local_tm.tm_hour;
    return hour < config_.day_start_hour || hour >= config_.night_start_hour;
}

SensorData DeviceSimulator::GenerateDayNightData(std::mt19937& generator) const {
    SensorData data;
    data.device_id = config_.device_id;
    data.crop_name = GetCurrentCrop();
    data.location = config_.location;
    data.timestamp = CurrentUnixTimestamp();

    double time_factor = GetTimeOfDayFactor();

    if (time_factor >= 0) {
        double progress = time_factor;
        data.temperature = config_.day_temp_base - 3.0 + progress * 6.0;
        data.light = config_.day_light_max * (0.3 + progress * 0.7);
    } else {
        data.temperature = config_.night_temp_base + RandomRange(generator, -1.0, 1.0);
        data.light = config_.night_light_max + RandomRange(generator, -10.0, 10.0);
    }

    data.humidity = RandomRange(generator, 55.0, 75.0);

    data.temperature += RandomRange(generator, -1.5, 1.5);
    data.humidity += RandomRange(generator, -3.0, 3.0);
    data.light += RandomRange(generator, -20.0, 20.0);

    data.scenario_mode = "daynight";

    std::cout << "[Simulator][DAY/NIGHT] mode=" << (IsNight() ? "night" : "day")
              << " time_factor=" << std::fixed << std::setprecision(2) << time_factor << std::endl;

    return data;
}

SensorData DeviceSimulator::GenerateAnomalyData(std::mt19937& generator) {
    std::lock_guard<std::mutex> lock(anomaly_mutex_);

    UpdateAnomalyState();

    SensorData data;
    data.device_id = config_.device_id;
    data.crop_name = GetCurrentCrop();
    data.location = config_.location;
    data.timestamp = CurrentUnixTimestamp();
    data.humidity = RandomRange(generator, 30.0, 80.0);
    data.light = RandomRange(generator, 100.0, 1000.0);

    if (anomaly_active_) {
        data.temperature = RandomRange(generator, config_.anomaly_temp_min, config_.anomaly_temp_max);
        data.scenario_mode = "anomaly";

        std::cout << "[Simulator][ANOMALY] ACTIVE temp=" << std::fixed << std::setprecision(2)
                  << data.temperature << "C (until " << anomaly_until_ << ")" << std::endl;
    } else {
        {
            std::lock_guard<std::mutex> temp_lock(last_temp_mutex_);
            last_normal_temp_ = RandomRange(generator, 20.0, 40.0);
            data.temperature = last_normal_temp_;
        }
        data.scenario_mode = "normal";
    }

    return data;
}

void DeviceSimulator::UpdateAnomalyState() {
    long long now = CurrentUnixTimestamp();

    if (anomaly_active_ && now >= anomaly_until_) {
        anomaly_active_ = false;
        std::cout << "[Simulator][ANOMALY] Ended" << std::endl;
    }

    if (!anomaly_active_) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double roll = dist(scenario_rng_);
        if (roll < config_.anomaly_probability) {
            anomaly_active_ = true;
            anomaly_until_ = now + config_.anomaly_duration_seconds;
            std::cout << "[Simulator][ANOMALY] Triggered! Will last " << config_.anomaly_duration_seconds << "s" << std::endl;
        }
    }
}

SensorData DeviceSimulator::GenerateOfflineData() const {
    SensorData data;
    data.device_id = config_.device_id;
    data.crop_name = GetCurrentCrop();
    data.location = config_.location;
    data.timestamp = 0;
    data.temperature = 0;
    data.humidity = 0;
    data.light = 0;
    data.scenario_mode = "offline";
    return data;
}

SensorData DeviceSimulator::GenerateCsvData(std::mt19937& generator) {
    std::lock_guard<std::mutex> lock(csv_mutex_);

    if (csv_data_.empty()) {
        SensorData data;
        data.device_id = config_.device_id;
        data.crop_name = GetCurrentCrop();
        data.location = config_.location;
        data.timestamp = CurrentUnixTimestamp();
        data.temperature = RandomRange(generator, 20.0, 40.0);
        data.humidity = RandomRange(generator, 30.0, 80.0);
        data.light = RandomRange(generator, 100.0, 1000.0);
        data.scenario_mode = "fallback-random";
        return data;
    }

    SensorData data = csv_data_[csv_data_index_];
    csv_data_index_ = (csv_data_index_ + 1) % csv_data_.size();
    data.scenario_mode = "csv";
    data.timestamp = CurrentUnixTimestamp();

    std::cout << "[Simulator][CSV] Playback [" << csv_data_index_ << "/" << csv_data_.size()
              << "] temp=" << std::fixed << std::setprecision(2) << data.temperature << "C" << std::endl;

    return data;
}

SensorData DeviceSimulator::GenerateCombinedData(std::mt19937& generator) {
    SensorData data = GenerateDayNightData(generator);

    {
        std::lock_guard<std::mutex> lock(anomaly_mutex_);
        UpdateAnomalyState();

        if (anomaly_active_) {
            data.temperature = RandomRange(generator, config_.anomaly_temp_min, config_.anomaly_temp_max);
            data.scenario_mode = "combined+anomaly";
            std::cout << "[Simulator][COMBINED][ANOMALY] temp=" << std::fixed << std::setprecision(2)
                      << data.temperature << "C" << std::endl;
        } else {
            data.scenario_mode = "combined";
        }
    }

    return data;
}

bool DeviceSimulator::ShouldGoOffline() const {
    std::lock_guard<std::mutex> lock(offline_mutex_);
    long long now = CurrentUnixTimestamp();

    if (is_offline_ && now >= offline_until_) {
        is_offline_ = false;
        std::cout << "[Simulator][OFFLINE] Device back online" << std::endl;
        return false;
    }

    if (is_offline_) {
        return true;
    }

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double roll = dist(scenario_rng_);
    if (roll < config_.offline_probability) {
        return true;
    }

    return false;
}

void DeviceSimulator::UpdateOfflineState() {
    std::lock_guard<std::mutex> lock(offline_mutex_);
    long long now = CurrentUnixTimestamp();

    if (is_offline_ && now >= offline_until_) {
        is_offline_ = false;
        std::cout << "[Simulator][OFFLINE] Device back online" << std::endl;
    }
}
