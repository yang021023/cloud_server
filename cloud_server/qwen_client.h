#pragma once

#include <string>

struct QwenAnalysisResult {
    bool success = false;
    bool is_abnormal = false;
    std::string reason;
    std::string suggestion;
};

struct CropValidationResult {
    bool success = false;
    bool exists = false;
    std::string crop_name;
    std::string message;
    double min_temperature = 18.0;
    double max_temperature = 25.0;
    double min_humidity = 40.0;
    double max_humidity = 70.0;
    double min_light = 200.0;
    double max_light = 900.0;
};

class QwenClient {
public:
    QwenAnalysisResult Analyze(const std::string& device_id, const std::string& crop_name, double temperature, double humidity, double light) const;
    CropValidationResult ValidateCrop(const std::string& crop_name) const;

private:
    std::string GetApiKey() const;
    std::string BuildRequestBody(const std::string& device_id, const std::string& crop_name, double temperature, double humidity, double light) const;
    std::string BuildCropValidationRequestBody(const std::string& crop_name) const;
    std::string PostJson(const std::string& body, const std::string& api_key) const;
    QwenAnalysisResult ParseResponse(const std::string& response) const;
    CropValidationResult ParseCropValidationResponse(const std::string& response) const;
    std::string ExtractJsonString(const std::string& body, const std::string& key) const;
    bool ExtractJsonBool(const std::string& body, const std::string& key, bool default_value) const;
    double ExtractJsonDouble(const std::string& body, const std::string& key, double default_value) const;
};
