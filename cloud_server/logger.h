#pragma once

#include <mutex>
#include <string>

class Logger {
public:
    static void Info(const std::string& message);
    static void Error(const std::string& message);

private:
    static std::mutex mutex_;
};
