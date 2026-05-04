#include "offline_controller.h"

#include "logger.h"

#include <fstream>
#include <sstream>
#include <utility>

OfflineController::OfflineController(const CropConfig& crop_config, std::string log_path)
    : crop_config_(crop_config), log_path_(std::move(log_path)) {
}

std::vector<std::string> OfflineController::HandleData(const SensorData& data, const std::string& crop_name) const {
    const CropProfile profile = crop_config_.GetProfile(crop_name);
    std::vector<std::string> actions;

    if (data.temperature < profile.min_temperature) {
        actions.emplace_back("HEATER_ON");
    } else if (data.temperature > profile.max_temperature) {
        actions.emplace_back("FAN_ON");
    }

    if (data.humidity < profile.min_humidity) {
        actions.emplace_back("IRRIGATION_ON");
    } else if (data.humidity > profile.max_humidity) {
        actions.emplace_back("DEHUMIDIFIER_ON");
    }

    if (data.light < profile.min_light) {
        actions.emplace_back("GROW_LIGHT_ON");
    } else if (data.light > profile.max_light) {
        actions.emplace_back("SHADE_ON");
    }

    if (actions.empty()) {
        actions.emplace_back("ENVIRONMENT_STABLE");
    }

    AppendOperationLog(data, crop_name, profile, actions);
    return actions;
}

void OfflineController::AppendOperationLog(const SensorData& data, const std::string& crop_name, const CropProfile& profile,
                                           const std::vector<std::string>& actions) const {
    std::ofstream out(log_path_, std::ios::app);
    if (!out.is_open()) {
        Logger::Error("[OfflineController] Failed to open operation log: " + log_path_);
        return;
    }

    std::ostringstream action_stream;
    for (size_t i = 0; i < actions.size(); ++i) {
        if (i > 0) {
            action_stream << '|';
        }
        action_stream << actions[i];
    }

    out << "timestamp=" << data.timestamp
        << ",device_id=" << data.device_id
        << ",crop=" << crop_name
        << ",temperature=" << data.temperature
        << ",humidity=" << data.humidity
        << ",light=" << data.light
        << ",target_temperature=" << profile.min_temperature << '-' << profile.max_temperature
        << ",target_humidity=" << profile.min_humidity << '-' << profile.max_humidity
        << ",target_light=" << profile.min_light << '-' << profile.max_light
        << ",actions=" << action_stream.str()
        << '\n';
}
