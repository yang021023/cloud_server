#include "rule_engine.h"

std::vector<std::string> RuleEngine::Evaluate(const SensorData& data) const {
    std::vector<std::string> actions;

    if (data.temperature > 30.0) {
        actions.emplace_back("FAN_ON");
    }
    if (data.humidity < 40.0) {
        actions.emplace_back("IRRIGATION_ON");
    }

    return actions;
}
