// ============================================================================
// path_utils.cpp - 路径处理工具实现
// ============================================================================
#include "path_utils.h"
#include "string_utils.h"

#include <windows.h>
#include <algorithm>
#include <ctime>

namespace bandzip {
namespace util {

tstring normalize_path(const tstring& path) {
    tstring r;
    r.reserve(path.size());
    for (tchar c : path) {
        if (c == _T('/')) r += _T('\\');
        else r += c;
    }
    // 去除末尾分隔符（除非是根目录如 C:\）
    while (r.length() > 3 && r.back() == _T('\\')) r.pop_back();
    return r;
}

tstring get_full_path(const tstring& path) {
    tchar buf[MAX_PATH + 1];
    DWORD len = GetFullPathName(path.c_str(), MAX_PATH, buf, nullptr);
    if (len == 0 || len >= MAX_PATH) return path;
    return tstring(buf, len);
}

tstring join_path(const tstring& a, const tstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (is_absolute(b) || is_unc(b)) return b;

    tstring r = a;
    if (r.back() != _T('\\') && r.back() != _T('/')) r += _T('\\');
    r += b;
    return normalize_path(r);
}

tstring join_path(const std::vector<tstring>& parts) {
    tstring r;
    for (const auto& p : parts) {
        if (r.empty()) r = p;
        else r = join_path(r, p);
    }
    return r;
}

tstring make_relative(const tstring& from, const tstring& to) {
    // 简化实现：若 to 以 from 开头，则截取
    tstring f = normalize_path(from);
    tstring t = normalize_path(to);
    if (f.back() != _T('\\')) f += _T('\\');
    if (_tcsnicmp(t.c_str(), f.c_str(), f.length()) == 0) {
        return t.substr(f.length());
    }
    return to;
}

tstring to_archive_path(const tstring& windows_path) {
    tstring r;
    r.reserve(windows_path.size());
    for (tchar c : windows_path) {
        if (c == _T('\\')) r += _T('/');
        else r += c;
    }
    // 去除开头的 ./
    if (r.length() >= 2 && r[0] == _T('.') && r[1] == _T('/')) {
        r = r.substr(2);
    }
    // 去除开头的 /
    while (!r.empty() && r[0] == _T('/')) r.erase(r.begin());
    return r;
}

tstring to_windows_path(const tstring& archive_path) {
    tstring r;
    r.reserve(archive_path.size());
    for (tchar c : archive_path) {
        if (c == _T('/')) r += _T('\\');
        else r += c;
    }
    return r;
}

tstring common_prefix(const std::vector<tstring>& paths) {
    if (paths.empty()) return tstring();
    tstring prefix = paths[0];
    for (size_t i = 1; i < paths.size(); ++i) {
        size_t j = 0;
        const auto& p = paths[i];
        size_t min_len = std::min(prefix.length(), p.length());
        while (j < min_len && _totlower(prefix[j]) == _totlower(p[j])) ++j;
        prefix = prefix.substr(0, j);
    }
    // 截到最后一个分隔符
    size_t pos = prefix.find_last_of(_T("\\/"));
    if (pos != tstring::npos) prefix = prefix.substr(0, pos + 1);
    return prefix;
}

bool has_parent_ref(const tstring& path) {
    // 检测 .. 组件
    tstring p = normalize_path(path);
    auto parts = split(p, _T('\\'));
    for (const auto& part : parts) {
        if (part == _T("..")) return true;
    }
    return false;
}

bool is_absolute(const tstring& path) {
    if (path.empty()) return false;
    // C:\ 或 C:/ 形式
    if (path.length() >= 3 && _istalpha(path[0]) && path[1] == _T(':') &&
        (path[2] == _T('\\') || path[2] == _T('/'))) return true;
    // \path 形式
    if (path[0] == _T('\\') || path[0] == _T('/')) return true;
    return false;
}

bool is_unc(const tstring& path) {
    return path.length() >= 2 && path[0] == _T('\\') && path[1] == _T('\\');
}

std::time_t filetime_to_unix(const FILETIME& ft) {
    ULARGE_INTEGER ul;
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;
    // FILETIME 是 100ns 间隔，从 1601-01-01 起
    // Unix 是从 1970-01-01 起
    if (ul.QuadPart == 0) return 0;
    static const u64 EPOCH_DIFF = 116444736000000000ULL;
    if (ul.QuadPart < EPOCH_DIFF) return 0;
    return static_cast<std::time_t>((ul.QuadPart - EPOCH_DIFF) / 10000000ULL);
}

FILETIME unix_to_filetime(std::time_t t) {
    static const u64 EPOCH_DIFF = 116444736000000000ULL;
    u64 v = static_cast<u64>(t) * 10000000ULL + EPOCH_DIFF;
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(v & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>(v >> 32);
    return ft;
}

std::time_t dostime_to_unix(u16 dos_date, u16 dos_time) {
    if (dos_date == 0 && dos_time == 0) return 0;
    struct tm tm = {};
    tm.tm_year = ((dos_date >> 9) & 0x7F) + 1980 - 1900;
    tm.tm_mon  = ((dos_date >> 5) & 0x0F) - 1;
    tm.tm_mday = (dos_date & 0x1F);
    tm.tm_hour = (dos_time >> 11) & 0x1F;
    tm.tm_min  = (dos_time >> 5) & 0x3F;
    tm.tm_sec  = (dos_time & 0x1F) * 2;
    tm.tm_isdst = -1;
    return _mkgmtime(&tm);
}

void unix_to_dostime(std::time_t t, u16& dos_date, u16& dos_time) {
    if (t == 0) {
        dos_date = 0;
        dos_time = 0;
        return;
    }
    struct tm tm;
    gmtime_s(&tm, &t);
    dos_date = static_cast<u16>(((tm.tm_year + 1900 - 1980) << 9) |
                                 ((tm.tm_mon + 1) << 5) |
                                  tm.tm_mday);
    dos_time = static_cast<u16>((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
}

u32 win_attr_to_archive(u32 win_attr) {
    u32 a = 0;
    if (win_attr & FILE_ATTRIBUTE_DIRECTORY) a |= 0x10;  // FILE_ATTRIBUTE_DIRECTORY
    if (win_attr & FILE_ATTRIBUTE_READONLY)  a |= 0x01;
    if (win_attr & FILE_ATTRIBUTE_HIDDEN)    a |= 0x02;
    if (win_attr & FILE_ATTRIBUTE_SYSTEM)    a |= 0x04;
    if (win_attr & FILE_ATTRIBUTE_ARCHIVE)   a |= 0x20;
    return a;
}

u32 archive_attr_to_win(u32 archive_attr) {
    u32 a = 0;
    if (archive_attr & 0x10) a |= FILE_ATTRIBUTE_DIRECTORY;
    if (archive_attr & 0x01) a |= FILE_ATTRIBUTE_READONLY;
    if (archive_attr & 0x02) a |= FILE_ATTRIBUTE_HIDDEN;
    if (archive_attr & 0x04) a |= FILE_ATTRIBUTE_SYSTEM;
    if (archive_attr & 0x20) a |= FILE_ATTRIBUTE_ARCHIVE;
    return a;
}

} // namespace util
} // namespace bandzip
