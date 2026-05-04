#pragma once

#include "config.h"

#include <atomic>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

enum class ScenarioType {
    Random,           // 随机数据（原有模式）
    DayNight,         // 白天/黑夜模拟
    Anomaly,          // 温度突变模拟
    DeviceOffline,    // 设备掉线模拟
    CsvPlayback,      // CSV 数据回放
    Combined          // 组合模式（白天/黑夜 + 突变）
};

struct SensorData {
    std::string device_id;
    std::string crop_name;
    std::string location;
    double temperature = 0.0;
    double humidity = 0.0;
    double light = 0.0;
    long long timestamp = 0;
    std::string scenario_mode;  // 当前场景模式标签
};

class DeviceSimulator {
public:
    explicit DeviceSimulator(const SimulatorConfig& config);
    ~DeviceSimulator();

    void Start();
    void Wait();
    void Stop();

private:
    SimulatorConfig config_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::thread crop_sync_worker_;
    mutable std::mutex crop_mutex_;
    mutable std::mutex config_mutex_;
    std::string current_crop_;

    // 场景模拟相关
    ScenarioType scenario_type_;
    mutable std::mutex scenario_mutex_;
    mutable std::mt19937 scenario_rng_;

    // CSV 回放相关
    std::vector<SensorData> csv_data_;
    mutable size_t csv_data_index_;
    mutable std::mutex csv_mutex_;

    // 设备掉线相关
    mutable bool is_offline_;
    mutable long long offline_until_;
    mutable std::mutex offline_mutex_;

    // 温度突变相关
    mutable bool anomaly_active_;
    mutable long long anomaly_until_;
    mutable std::mutex anomaly_mutex_;
    mutable double last_normal_temp_;
    mutable std::mutex last_temp_mutex_;

    void InitializeCropSelection();
    std::string PromptManualCrop() const;
    void PrintSendLog(const SensorData& data, const std::string& crop_name) const;
    void RunDevice();
    void RunCropSync();
    SensorData GenerateData() const;
    std::string ToJson(const SensorData& data) const;
    std::string GetCurrentCrop() const;
    void SetCurrentCrop(const std::string& crop_name);
    void PersistCurrentCrop(const std::string& crop_name);
    std::string FetchCropFromCloud() const;

    // 场景模拟方法
    ScenarioType GetScenarioType() const;
    void LoadCsvData();
    double GetTimeOfDayFactor() const;
    bool IsNight() const;
    SensorData GenerateDayNightData(std::mt19937& generator) const;
    SensorData GenerateAnomalyData(std::mt19937& generator);
    SensorData GenerateOfflineData() const;
    SensorData GenerateCsvData(std::mt19937& generator);
    SensorData GenerateCombinedData(std::mt19937& generator);
    void UpdateAnomalyState();
    bool ShouldGoOffline() const;
    void UpdateOfflineState();
};

ScenarioType GetScenarioTypeFromString(const std::string& mode);
