#pragma once

#include "crop_config.h"
#include "sensor_data.h"

#include <string>
#include <vector>

class OfflineController {
public:
    OfflineController(const CropConfig& crop_config, std::string log_path);

    std::vector<std::string> HandleData(const SensorData& data, const std::string& crop_name) const;

private:
    const CropConfig& crop_config_;
    std::string log_path_;

    void AppendOperationLog(const SensorData& data, const std::string& crop_name, const CropProfile& profile,
                            const std::vector<std::string>& actions) const;
};
