#include "ai_analyzer.h"

#include "logger.h"
#include "qwen_client.h"

namespace {
AiResult AnalyzeByRules(const std::string& crop_name, double temperature, double humidity, double light) {
    AiResult result;

    double high_temperature = 32.0;
    double low_humidity = 35.0;
    if (crop_name == "tomato") {
        high_temperature = 30.0;
        low_humidity = 50.0;
    } else if (crop_name == "cucumber") {
        high_temperature = 28.0;
        low_humidity = 60.0;
    } else if (crop_name == "pepper") {
        high_temperature = 31.0;
        low_humidity = 45.0;
    }

    if (temperature > high_temperature) {
        result.is_abnormal = true;
        result.reason = "HIGH_TEMPERATURE_FOR_" + crop_name;
        result.suggestion = "TURN_ON_FAN_FOR_" + crop_name;
        return result;
    }

    if (humidity < low_humidity) {
        result.is_abnormal = true;
        result.reason = "LOW_HUMIDITY_FOR_" + crop_name;
        result.suggestion = "START_IRRIGATION_FOR_" + crop_name;
        return result;
    }

    if (light < 150.0) {
        result.is_abnormal = true;
        result.reason = "LOW_LIGHT";
        result.suggestion = "TURN_ON_GROW_LIGHT";
        return result;
    }

    result.is_abnormal = false;
    result.reason = "NORMAL_FOR_" + crop_name;
    result.suggestion = "KEEP_CURRENT_ENVIRONMENT";
    return result;
}
}

AiResult AiAnalyzer::Analyze(double temperature, double humidity, double light) const {
    return AnalyzeByRules("default", temperature, humidity, light);
}

AiResult AiAnalyzer::AnalyzeWithDevice(const std::string& device_id, const std::string& crop_name, double temperature, double humidity, double light) const {
    QwenClient client;
    const QwenAnalysisResult qwen = client.Analyze(device_id, crop_name, temperature, humidity, light);
    if (qwen.success) {
        Logger::Info("[AI] using qwen for " + device_id + " crop=" + crop_name);
        AiResult result;
        result.is_abnormal = qwen.is_abnormal;
        result.reason = qwen.reason;
        result.suggestion = qwen.suggestion;
        return result;
    }

    Logger::Info("[AI] fallback to local rules for " + device_id + " crop=" + crop_name);
    return AnalyzeByRules(crop_name, temperature, humidity, light);
}
