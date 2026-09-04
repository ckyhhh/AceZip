// ============================================================================
// logger.h - 简易日志
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <fstream>
#include <mutex>

namespace bandzip {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel lv) { level_ = lv; }
    LogLevel level() const { return level_; }

    void set_file(const tstring& path);
    void set_console(bool enable) { console_ = enable; }

    void log(LogLevel lv, const tstring& msg);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    std::ofstream file_;
    LogLevel level_ = LogLevel::Info;
    bool console_ = false;
};

#define LOG_TRACE(msg) ::bandzip::Logger::instance().log(::bandzip::LogLevel::Trace, (msg))
#define LOG_DEBUG(msg) ::bandzip::Logger::instance().log(::bandzip::LogLevel::Debug, (msg))
#define LOG_INFO(msg)  ::bandzip::Logger::instance().log(::bandzip::LogLevel::Info,  (msg))
#define LOG_WARN(msg)  ::bandzip::Logger::instance().log(::bandzip::LogLevel::Warn,  (msg))
#define LOG_ERROR(msg) ::bandzip::Logger::instance().log(::bandzip::LogLevel::Error, (msg))
#define LOG_FATAL(msg) ::bandzip::Logger::instance().log(::bandzip::LogLevel::Fatal, (msg))

} // namespace bandzip
