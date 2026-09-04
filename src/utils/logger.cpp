// ============================================================================
// logger.cpp - 简易日志实现
// ============================================================================
#include "logger.h"
#include "string_utils.h"
#include "file_utils.h"
#include "path_utils.h"

#include <windows.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace bandzip {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() {
    // 默认日志文件位置
    tstring dir = util::get_appdata_dir() + _T("\\BandzipClone\\logs");
    if (!util::dir_exists(dir)) util::create_dir_recursive(dir);

    time_t now = time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    tchar buf[32];
    _tcsftime(buf, sizeof(buf)/sizeof(buf[0]), _T("%Y%m%d"), &tm);
    tstring path = dir + _T("\\bandzip_") + buf + _T(".log");
    set_file(path);
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.close();
}

void Logger::set_file(const tstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.close();
    file_.open(util::tstring_to_string(path), std::ios::app);
}

void Logger::log(LogLevel lv, const tstring& msg) {
    if (lv < level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 时间戳
    time_t now = time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    tchar ts[32];
    _tcsftime(ts, sizeof(ts)/sizeof(ts[0]), _T("%H:%M:%S"), &tm);

    const tchar* level_str = _T("?");
    switch (lv) {
    case LogLevel::Trace: level_str = _T("TRACE"); break;
    case LogLevel::Debug: level_str = _T("DEBUG"); break;
    case LogLevel::Info:  level_str = _T("INFO");  break;
    case LogLevel::Warn:  level_str = _T("WARN");  break;
    case LogLevel::Error: level_str = _T("ERROR"); break;
    case LogLevel::Fatal: level_str = _T("FATAL"); break;
    }

    std::basic_ostringstream<tchar> ss;
    ss << ts << _T(" [") << level_str << _T("] ") << msg << _T("\n");

    if (file_.is_open()) {
        file_ << util::tstring_to_string(ss.str());
        file_.flush();
    }
    if (console_) {
        OutputDebugString(ss.str().c_str());
    }
}

} // namespace bandzip
