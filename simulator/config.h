#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct SimulatorConfig {
    std::string config_path = "../../../simulator/config/device_004.conf";
    std::string device_id;
    std::string local_ip;
    int local_port = 0;
    std::string location;
    std::string server_ip;
    int server_port = 0;
    int interval_seconds = 0;
    std::string cloud_ip;
    int cloud_port = 0;
    std::string crop_name;

    // 场景模拟配置
    std::string scenario_mode = "random";  // random, daynight, anomaly, offline, csv, combined
    std::string csv_file_path;              // CSV 文件路径（用于 csv 模式）

    // 白天/黑夜配置
    double day_temp_base = 28.0;           // 白天基础温度
    double night_temp_base = 18.0;         // 夜间基础温度
    double day_light_max = 800.0;          // 白天最大光照
    double night_light_max = 50.0;         // 夜间最大光照
    int day_start_hour = 6;                // 白天开始时间（小时）
    int night_start_hour = 18;             // 夜间开始时间（小时）

    // 温度突变配置
    double anomaly_probability = 0.05;     // 突变发生概率 (0.0-1.0)
    double anomaly_temp_min = 5.0;          // 低温突变最低值
    double anomaly_temp_max = 50.0;         // 高温突变最高值
    int anomaly_duration_seconds = 30;      // 突变持续时间

    // 设备掉线配置
    double offline_probability = 0.03;      // 掉线概率 (0.0-1.0)
    int offline_duration_min = 10;           // 掉线最短持续时间（秒）
    int offline_duration_max = 60;          // 掉线最长持续时间（秒）
};

SimulatorConfig ParseArguments(int argc, char* argv[]);
void PrintUsage();
bool UpdateConfigCropName(const std::string& config_path, const std::string& crop_name);
