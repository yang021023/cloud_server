#include "crop_config.h"

#include "logger.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace {
constexpr const char* kDefaultCropConfig =
    "default 18 25 40 70 200 900\n"
    "tomato 20 28 55 75 300 800\n"
    "cucumber 18 26 60 85 250 700\n"
    "pepper 22 30 50 70 280 850\n";
}

CropConfig::CropConfig(std::string file_path)
    : file_path_(std::move(file_path)) {
}

bool CropConfig::Load() {
    if (!EnsureDefaultFile()) {
        Logger::Error("[CropConfig] Failed to prepare crop_config.txt");
        return false;
    }

    std::ifstream input(file_path_);
    if (!input.is_open()) {
        Logger::Error("[CropConfig] Failed to open config file: " + file_path_);
        return false;
    }

    profiles_.clear();
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string crop_name;
        CropProfile profile;
        if (!(iss >> crop_name
                  >> profile.min_temperature >> profile.max_temperature
                  >> profile.min_humidity >> profile.max_humidity
                  >> profile.min_light >> profile.max_light)) {
            Logger::Error("[CropConfig] Ignoring invalid line: " + line);
            continue;
        }

        profiles_[crop_name] = profile;
    }

    if (profiles_.find("default") == profiles_.end()) {
        profiles_["default"] = CropProfile{};
        Logger::Info("[CropConfig] Missing default entry, using fallback default profile");
    }

    Logger::Info("[CropConfig] Loaded crop temperature/humidity/light config from " + file_path_);
    return true;
}

CropProfile CropConfig::GetProfile(const std::string& crop_name) const {
    const auto it = profiles_.find(crop_name);
    if (it != profiles_.end()) {
        return it->second;
    }

    const auto default_it = profiles_.find("default");
    if (default_it != profiles_.end()) {
        return default_it->second;
    }

    return CropProfile{};
}

bool CropConfig::EnsureCropExists(const std::string& crop_name) {
    if (crop_name.empty() || profiles_.find(crop_name) != profiles_.end()) {
        return true;
    }

    return UpsertCropProfile(crop_name, GetProfile("default"));
}

bool CropConfig::UpsertCropProfile(const std::string& crop_name, const CropProfile& profile) {
    if (crop_name.empty()) {
        return false;
    }

    profiles_[crop_name] = profile;
    if (!SaveAll()) {
        Logger::Error("[CropConfig] Failed to save crop profile for: " + crop_name);
        return false;
    }

    Logger::Info("[CropConfig] Upserted crop profile: " + crop_name +
                 " temp=" + std::to_string(profile.min_temperature) + "-" + std::to_string(profile.max_temperature) +
                 " humidity=" + std::to_string(profile.min_humidity) + "-" + std::to_string(profile.max_humidity) +
                 " light=" + std::to_string(profile.min_light) + "-" + std::to_string(profile.max_light));
    return true;
}

const std::string& CropConfig::file_path() const {
    return file_path_;
}

bool CropConfig::EnsureDefaultFile() const {
    std::ifstream existing(file_path_);
    if (existing.good()) {
        return true;
    }

    std::ofstream output(file_path_);
    if (!output.is_open()) {
        return false;
    }

    output << kDefaultCropConfig;
    return true;
}

bool CropConfig::SaveAll() const {
    std::ofstream output(file_path_, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    if (const auto it = profiles_.find("default"); it != profiles_.end()) {
        output << "default "
               << it->second.min_temperature << ' ' << it->second.max_temperature << ' '
               << it->second.min_humidity << ' ' << it->second.max_humidity << ' '
               << it->second.min_light << ' ' << it->second.max_light << '\n';
    }

    for (const auto& [crop_name, profile] : profiles_) {
        if (crop_name == "default") {
            continue;
        }
        output << crop_name << ' '
               << profile.min_temperature << ' ' << profile.max_temperature << ' '
               << profile.min_humidity << ' ' << profile.max_humidity << ' '
               << profile.min_light << ' ' << profile.max_light << '\n';
    }

    return true;
}
