#pragma once

#include "types.h"
#include <string>
#include <mutex>

class Logger {
public:
    static Logger& instance();
    void set_level(LogLevel level);
    void log(LogLevel level, const char* file, int line, const std::string& msg);

private:
    Logger() = default;
    std::mutex mutex_;
    LogLevel level_ = LogLevel::INFO;
};

#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, __FILE__, __LINE__, msg)
#define LOG_INFO(msg) Logger::instance().log(LogLevel::INFO, __FILE__, __LINE__, msg)
#define LOG_WARN(msg) Logger::instance().log(LogLevel::WARN, __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, __FILE__, __LINE__, msg)
