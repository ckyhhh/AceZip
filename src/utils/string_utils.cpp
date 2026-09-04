// ============================================================================
// string_utils.cpp - 字符串工具实现
// ============================================================================
#include "string_utils.h"

#include <windows.h>
#include <shlwapi.h>
#include <cstdarg>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace bandzip {
namespace util {

#ifdef _UNICODE
std::string tstring_to_string(const tstring& ts) {
    return utf16_to_utf8(ts);
}
tstring string_to_tstring(const std::string& s) {
    return utf8_to_utf16(s);
}
#else
std::string tstring_to_string(const tstring& ts) { return ts; }
tstring string_to_tstring(const std::string& s) { return s; }
#endif

std::string tstring_to_ansi(const tstring& ts) {
#ifdef _UNICODE
    return utf16_to_cp(ts, CP_ACP);
#else
    return ts;
#endif
}

tstring ansi_to_tstring(const std::string& s) {
#ifdef _UNICODE
    return cp_to_utf16(s, CP_ACP);
#else
    return s;
#endif
}

std::wstring utf8_to_utf16(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                  static_cast<int>(s.size()),
                                  nullptr, 0);
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        static_cast<int>(s.size()), &ws[0], len);
    return ws;
}

std::string utf16_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(),
                                  static_cast<int>(ws.size()),
                                  nullptr, 0, nullptr, nullptr);
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(),
                        static_cast<int>(ws.size()), &s[0], len,
                        nullptr, nullptr);
    return s;
}

std::wstring gbk_to_utf16(const std::string& s) {
    return cp_to_utf16(s, 936);
}

std::string utf16_to_gbk(const std::wstring& ws) {
    return utf16_to_cp(ws, 936);
}

std::wstring sjis_to_utf16(const std::string& s) {
    return cp_to_utf16(s, 932);
}

std::wstring big5_to_utf16(const std::string& s) {
    return cp_to_utf16(s, 950);
}

std::wstring cp_to_utf16(const std::string& s, int codepage) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(codepage, 0, s.c_str(),
                                  static_cast<int>(s.size()),
                                  nullptr, 0);
    std::wstring ws(len, 0);
    MultiByteToWideChar(codepage, 0, s.c_str(),
                        static_cast<int>(s.size()), &ws[0], len);
    return ws;
}

std::string utf16_to_cp(const std::wstring& ws, int codepage) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(codepage, 0, ws.c_str(),
                                  static_cast<int>(ws.size()),
                                  nullptr, 0, nullptr, nullptr);
    std::string s(len, 0);
    WideCharToMultiByte(codepage, 0, ws.c_str(),
                        static_cast<int>(ws.size()), &s[0], len,
                        nullptr, nullptr);
    return s;
}

tstring to_lower(const tstring& s) {
    tstring r = s;
    std::transform(r.begin(), r.end(), r.begin(),
#ifdef _UNICODE
        [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); }
#else
        [](char c) { return static_cast<char>(tolower(c)); }
#endif
    );
    return r;
}

tstring to_upper(const tstring& s) {
    tstring r = s;
    std::transform(r.begin(), r.end(), r.begin(),
#ifdef _UNICODE
        [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); }
#else
        [](char c) { return static_cast<char>(toupper(c)); }
#endif
    );
    return r;
}

std::vector<tstring> split(const tstring& s, tchar sep) {
    std::vector<tstring> v;
    size_t start = 0;
    size_t end = s.find(sep);
    while (end != tstring::npos) {
        v.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(sep, start);
    }
    v.push_back(s.substr(start));
    return v;
}

tstring join(const std::vector<tstring>& v, const tstring& sep) {
    if (v.empty()) return tstring();
    tstring r = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        r += sep + v[i];
    }
    return r;
}

tstring trim_left(const tstring& s) {
    auto it = std::find_if(s.begin(), s.end(),
        [](tchar c) { return !std::isspace(c); });
    return tstring(it, s.end());
}

tstring trim_right(const tstring& s) {
    auto it = std::find_if(s.rbegin(), s.rend(),
        [](tchar c) { return !std::isspace(c); });
    return tstring(s.begin(), it.base());
}

tstring trim(const tstring& s) {
    return trim_left(trim_right(s));
}

tstring replace_all(const tstring& s, const tstring& from, const tstring& to) {
    if (from.empty()) return s;
    tstring r;
    r.reserve(s.size());
    size_t pos = 0, prev = 0;
    while ((pos = s.find(from, prev)) != tstring::npos) {
        r.append(s, prev, pos - prev);
        r.append(to);
        prev = pos + from.size();
    }
    r.append(s, prev, tstring::npos);
    return r;
}

tstring get_extension(const tstring& path) {
    size_t dot = path.find_last_of(_T('.'));
    size_t sep = path.find_last_of(_T("\\/"));
    if (dot == tstring::npos) return tstring();
    if (sep != tstring::npos && dot < sep) return tstring();
    return path.substr(dot);
}

tstring get_extension_lower(const tstring& path) {
    return to_lower(get_extension(path));
}

tstring get_filename(const tstring& path) {
    size_t sep = path.find_last_of(_T("\\/"));
    if (sep == tstring::npos) return path;
    return path.substr(sep + 1);
}

tstring get_basename(const tstring& path) {
    tstring fn = get_filename(path);
    size_t dot = fn.find_last_of(_T('.'));
    if (dot == tstring::npos) return fn;
    return fn.substr(0, dot);
}

tstring get_dirname(const tstring& path) {
    size_t sep = path.find_last_of(_T("\\/"));
    if (sep == tstring::npos) return tstring();
    return path.substr(0, sep);
}

tstring format(const tchar* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    tchar buf[2048];
#ifdef _UNICODE
    vswprintf(buf, sizeof(buf)/sizeof(buf[0]) - 1, fmt, args);
#else
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
#endif
    va_end(args);
    return tstring(buf);
}

tstring format_size(u64 bytes) {
    const u64 KB = 1024;
    const u64 MB = KB * 1024;
    const u64 GB = MB * 1024;
    const u64 TB = GB * 1024;

    if (bytes >= TB) return format(_T("%.2f TB"), static_cast<double>(bytes) / TB);
    if (bytes >= GB) return format(_T("%.2f GB"), static_cast<double>(bytes) / GB);
    if (bytes >= MB) return format(_T("%.2f MB"), static_cast<double>(bytes) / MB);
    if (bytes >= KB) return format(_T("%.2f KB"), static_cast<double>(bytes) / KB);
    return format(_T("%llu B"), bytes);
}

tstring format_count(u64 count) {
    tstring s = std::to_wstring(count);
    // 添加千分位
    tstring r;
    int n = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (n > 0 && n % 3 == 0) r = _T(",") + r;
        r = tstring(1, *it) + r;
        ++n;
    }
    return r;
}

tstring format_time(std::time_t t) {
    if (t == 0) return _T("-");
    struct tm tm;
    localtime_s(&tm, &t);
    tchar buf[64];
    _tcsftime(buf, sizeof(buf)/sizeof(buf[0]), _T("%Y-%m-%d %H:%M:%S"), &tm);
    return tstring(buf);
}

} // namespace util
} // namespace bandzip
