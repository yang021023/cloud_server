#include "logger.h"

#include <iostream>

std::mutex Logger::mutex_;

void Logger::Info(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << message << std::endl;
}

void Logger::Error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << message << std::endl;
}
