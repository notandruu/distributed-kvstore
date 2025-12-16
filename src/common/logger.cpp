#include "logger.h"
#include <cstdio>
#include <chrono>
#include <ctime>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < level_) return;

    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    gmtime_r(&tt, &tm_buf);

    const char* level_str = nullptr;
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO:  level_str = "INFO";  break;
        case LogLevel::WARN:  level_str = "WARN";  break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
    }

    fprintf(stderr,
        "[%04d-%02d-%02d %02d:%02d:%02d.%03lld] [%s] [%s:%d] %s\n",
        tm_buf.tm_year + 1900,
        tm_buf.tm_mon + 1,
        tm_buf.tm_mday,
        tm_buf.tm_hour,
        tm_buf.tm_min,
        tm_buf.tm_sec,
        static_cast<long long>(ms.count()),
        level_str,
        file,
        line,
        msg.c_str());
}
