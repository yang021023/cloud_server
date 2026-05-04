#pragma once

#include "sensor_data.h"

#include <memory>
#include <string>
#include <vector>

struct ControlCommandDto {
    int id = 0;
    std::string device_id;
    long long timestamp = 0;
    std::string crop_name;
    std::string command;
    std::string reason;
    std::string status;
};

struct CropProfileDto {
    std::string crop_name = "default";
    double min_temperature = 18.0;
    double max_temperature = 25.0;
    double min_humidity = 40.0;
    double max_humidity = 70.0;
    double min_light = 200.0;
    double max_light = 900.0;
};

class GrpcClientStub {
public:
    GrpcClientStub();
    bool UploadSensorData(const SensorData& data) const;
    bool SendHeartbeat() const;
    bool SetCurrentCrop(const std::string& device_id, const std::string& crop_name) const;
    std::string GetCurrentCrop(const std::string& device_id) const;
    CropProfileDto GetCropProfile(const std::string& crop_name) const;
    std::vector<CropProfileDto> GetAllCropProfiles() const;
    std::vector<ControlCommandDto> GetPendingControlCommands() const;
    bool UpdateControlCommandStatus(int id, const std::string& status) const;
    bool ResolveLatestAlertByDevice(const std::string& device_id) const;

private:
    class GrpcClientImpl;
    std::unique_ptr<GrpcClientImpl> impl_;
};
