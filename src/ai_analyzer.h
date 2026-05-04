#pragma once

#include <string>

struct AiResult {
    bool is_abnormal = false;
    std::string reason;
    std::string suggestion;
};

class AiAnalyzer {
public:
    AiResult Analyze(double temperature, double humidity, double light) const;
    AiResult AnalyzeWithDevice(const std::string& device_id, const std::string& crop_name, double temperature, double humidity, double light) const;
};
