#pragma once

#include <string>
#include <unordered_map>

struct CropProfile {
    double min_temperature = 18.0;
    double max_temperature = 25.0;
    double min_humidity = 40.0;
    double max_humidity = 70.0;
    double min_light = 200.0;
    double max_light = 900.0;
};

class CropConfig {
public:
    explicit CropConfig(std::string file_path);

    bool Load();
    CropProfile GetProfile(const std::string& crop_name) const;
    bool EnsureCropExists(const std::string& crop_name);
    bool UpsertCropProfile(const std::string& crop_name, const CropProfile& profile);
    const std::string& file_path() const;

private:
    std::string file_path_;
    std::unordered_map<std::string, CropProfile> profiles_;

    bool EnsureDefaultFile() const;
    bool SaveAll() const;
};
