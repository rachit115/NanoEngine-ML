#pragma once

#include <iostream>
#include <string>
#include <mutex>

namespace nanoengine {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static void Log(LogLevel level, const std::string& message) {
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);
        
        switch (level) {
            case LogLevel::DEBUG:
                std::cout << "[DEBUG] " << message << "\n";
                break;
            case LogLevel::INFO:
                std::cout << "\033[1;32m[INFO]\033[0m " << message << "\n";
                break;
            case LogLevel::WARN:
                std::cout << "\033[1;33m[WARN]\033[0m " << message << "\n";
                break;
            case LogLevel::ERROR:
                std::cerr << "\033[1;31m[ERROR]\033[0m " << message << "\n";
                break;
        }
    }
};

} // namespace nanoengine

#define LOG_INFO(msg) nanoengine::Logger::Log(nanoengine::LogLevel::INFO, msg)
#define LOG_WARN(msg) nanoengine::Logger::Log(nanoengine::LogLevel::WARN, msg)
#define LOG_ERROR(msg) nanoengine::Logger::Log(nanoengine::LogLevel::ERROR, msg)
#define LOG_DEBUG(msg) nanoengine::Logger::Log(nanoengine::LogLevel::DEBUG, msg)
