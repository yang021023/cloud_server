#pragma once

#include <string>
#include <vector>

struct SensorDataRecord {
    std::string device_id;
    std::string location;
    double temperature = 0.0;
    double humidity = 0.0;
    double light = 0.0;
    long long timestamp = 0;
};

struct AnalysisRecord {
    std::string device_id;
    long long timestamp = 0;
    bool is_abnormal = false;
    std::string reason;
    std::string suggestion;
};

struct DeviceRecord {
    std::string device_id;
    std::string device_name;
    std::string location;
    bool online = false;
    long long last_seen = 0;
};

struct AlertRecord {
    int id = 0;
    std::string device_id;
    long long timestamp = 0;
    std::string alert_type;
    double value = 0.0;
    std::string message;
    int resolved = 0;
};

struct RuleLogRecord {
    std::string device_id;
    long long timestamp = 0;
    std::string rule_name;
    double trigger_value = 0.0;
    std::string action;
};

struct ControlCommandRecord {
    int id = 0;
    std::string device_id;
    long long timestamp = 0;
    std::string crop_name;
    std::string command;
    std::string reason;
    std::string status;
};

struct CropSelectionRecord {
    std::string device_id;
    std::string crop_name;
    std::string source;
    long long updated_at = 0;
};

struct CropProfileRecord {
    std::string crop_name;
    double min_temperature = 18.0;
    double max_temperature = 25.0;
    double min_humidity = 40.0;
    double max_humidity = 70.0;
    double min_light = 200.0;
    double max_light = 900.0;
};

struct UserRecord {
    std::string username;
    std::string role;
    long long last_login = 0;
};

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();

    bool Open();
    void Close();
    bool Initialize();
    bool SeedDefaultData();

    bool ValidateUser(const std::string& username, const std::string& password_hash, std::string& role);
    bool UpdateLastLogin(const std::string& username);
    bool CreateUser(const std::string& username, const std::string& password_hash, const std::string& role);
    std::string GetUserRole(const std::string& username) const;
    std::vector<UserRecord> GetUsers() const;
    std::vector<std::string> GetAssignedDevices(const std::string& username) const;
    bool SetAssignedDevices(const std::string& username, const std::vector<std::string>& device_ids);
    bool IngestSensorData(const SensorDataRecord& data);

    void SetCurrentCrop(const std::string& device_id, const std::string& crop_name, const std::string& source = "frontend");
    CropSelectionRecord GetCurrentCrop(const std::string& device_id) const;
    std::vector<CropSelectionRecord> GetAllCurrentCrops() const;
    bool UpsertCropProfile(const CropProfileRecord& profile);
    CropProfileRecord GetCropProfile(const std::string& crop_name) const;
    std::vector<CropProfileRecord> GetAllCropProfiles() const;

    std::vector<SensorDataRecord> GetLatestData();
    std::vector<SensorDataRecord> GetHistoryData(const std::string& device_id, int limit);
    std::vector<AnalysisRecord> GetAnalysis(const std::string& device_id);
    std::vector<DeviceRecord> GetDevices();
    std::vector<AlertRecord> GetAlerts(const std::string& device_id);
    std::vector<RuleLogRecord> GetRuleLogs(const std::string& device_id);
    std::vector<ControlCommandRecord> GetControlCommands(const std::string& device_id, const std::string& status = "");
    bool UpdateControlCommandStatus(int id, const std::string& status);
    bool ResolveAlert(int id);
    bool ResolveLatestAlertByDevice(const std::string& device_id);

private:
    std::string db_path_;
    void* db_ = nullptr;

    bool Execute(const std::string& sql);
};
