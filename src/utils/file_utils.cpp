// ============================================================================
// file_utils.cpp - 文件操作工具实现
// ============================================================================
#include "file_utils.h"
#include "string_utils.h"
#include "path_utils.h"
#include "logger.h"

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>

#include <fstream>
#include <random>
#include <chrono>

namespace bandzip {
namespace util {

bool file_exists(const tstring& path) {
    DWORD attr = GetFileAttributes(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool dir_exists(const tstring& path) {
    DWORD attr = GetFileAttributes(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool create_dir(const tstring& path) {
    return CreateDirectory(path.c_str(), nullptr) != 0 ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

bool create_dir_recursive(const tstring& path) {
    if (path.empty()) return false;
    if (dir_exists(path)) return true;

    tstring parent = get_dirname(path);
    if (!parent.empty() && !dir_exists(parent)) {
        if (!create_dir_recursive(parent)) return false;
    }
    return create_dir(path);
}

bool delete_file(const tstring& path) {
    return DeleteFile(path.c_str()) != 0;
}

bool delete_dir(const tstring& path, bool recursive) {
    if (!recursive) {
        return RemoveDirectory(path.c_str()) != 0;
    }

    // 递归删除
    tstring pattern = path + _T("\\*");
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;

    bool ok = true;
    do {
        if (_tcscmp(fd.cFileName, _T(".")) == 0 ||
            _tcscmp(fd.cFileName, _T("..")) == 0) continue;

        tstring full = path + _T("\\") + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!delete_dir(full, true)) ok = false;
        } else {
            if (!DeleteFile(full.c_str())) {
                // 尝试去除只读属性后删除
                SetFileAttributes(full.c_str(), FILE_ATTRIBUTE_NORMAL);
                if (!DeleteFile(full.c_str())) ok = false;
            }
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);

    if (!RemoveDirectory(path.c_str())) ok = false;
    return ok;
}

bool move_file(const tstring& from, const tstring& to, bool overwrite) {
    DWORD flags = MOVEFILE_COPY_ALLOWED;
    if (overwrite) flags |= MOVEFILE_REPLACE_EXISTING;
    return MoveFileEx(from.c_str(), to.c_str(), flags) != 0;
}

bool copy_file(const tstring& from, const tstring& to, bool overwrite) {
    return CopyFile(from.c_str(), to.c_str(), !overwrite) != 0;
}

u64 file_size(const tstring& path) {
    HANDLE h = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) {
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);
    return static_cast<u64>(sz.QuadPart);
}

tstring get_temp_path() {
    tchar buf[MAX_PATH + 1];
    DWORD len = ::GetTempPath(MAX_PATH, buf);
    return tstring(buf, len);
}

tstring create_temp_file(const tstring& prefix, const tstring& ext) {
    tstring tmp = get_temp_path();

    // 生成唯一文件名
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<u64>(now));
    u64 rnd = rng();

    for (int i = 0; i < 10; ++i) {
        tstring name = prefix + format(_T("_%016X"), rnd) + ext;
        tstring path = tmp + name;
        HANDLE h = CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return path;
        }
        rnd = rng();
    }
    return tstring();
}

tstring create_temp_dir(const tstring& prefix) {
    tstring tmp = get_temp_path();
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<u64>(now));

    for (int i = 0; i < 10; ++i) {
        tstring name = prefix + format(_T("_%016X"), rng());
        tstring path = tmp + name;
        if (create_dir(path)) return path;
    }
    return tstring();
}

tstring get_appdata_dir() {
    tchar buf[MAX_PATH + 1];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        return tstring(buf);
    }
    return tstring();
}

tstring get_desktop_dir() {
    tchar buf[MAX_PATH + 1];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, buf))) {
        return tstring(buf);
    }
    return tstring();
}

tstring get_module_dir() {
    tchar buf[MAX_PATH + 1];
    DWORD len = GetModuleFileName(nullptr, buf, MAX_PATH);
    if (len == 0) return tstring();
    tstring path(buf, len);
    return get_dirname(path);
}

tstring get_current_dir() {
    tchar buf[MAX_PATH + 1];
    DWORD len = GetCurrentDirectory(MAX_PATH, buf);
    return tstring(buf, len);
}

bool set_current_dir(const tstring& dir) {
    return SetCurrentDirectory(dir.c_str()) != 0;
}

std::vector<FindData> find_files(const tstring& dir, const tstring& pattern) {
    std::vector<FindData> result;
    tstring search = dir + _T("\\") + pattern;

    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return result;

    do {
        if (_tcscmp(fd.cFileName, _T(".")) == 0 ||
            _tcscmp(fd.cFileName, _T("..")) == 0) continue;

        FindData d;
        d.path = dir + _T("\\") + fd.cFileName;
        d.name = fd.cFileName;
        d.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        d.is_readonly  = (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
        d.is_hidden    = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        d.is_system    = (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
        d.attributes = fd.dwFileAttributes;

        ULARGE_INTEGER ul;
        ul.LowPart = fd.nFileSizeLow;
        ul.HighPart = fd.nFileSizeHigh;
        d.size = ul.QuadPart;

        d.modified = filetime_to_unix(fd.ftLastWriteTime);
        d.created  = filetime_to_unix(fd.ftCreationTime);
        d.accessed  = filetime_to_unix(fd.ftLastAccessTime);

        result.push_back(std::move(d));
    } while (FindNextFile(h, &fd));
    FindClose(h);
    return result;
}

bool walk_dir(const tstring& dir,
              const std::function<bool(const FindData&)>& callback,
              bool recursive) {
    auto files = find_files(dir);
    for (const auto& f : files) {
        if (!callback(f)) return false;
        if (recursive && f.is_directory) {
            if (!walk_dir(f.path, callback, true)) return false;
        }
    }
    return true;
}

std::vector<u8> read_file_all(const tstring& path) {
    std::ifstream f(tstring_to_string(path), std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    if (size <= 0) return {};
    f.seekg(0);
    std::vector<u8> data(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool write_file_all(const tstring& path, const void* data, size_t size) {
    std::ofstream f(tstring_to_string(path), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return f.good();
}

bool is_valid_path(const tstring& path) {
    if (path.empty()) return false;
    // 禁止字符
    for (tchar c : path) {
        if (c == _T('<') || c == _T('>') || c == _T(':') || c == _T('"') ||
            c == _T('|') || c == _T('?') || c == _T('*')) {
            // 驱动器号允许一个冒号
            if (c == _T(':') && path.length() >= 2 &&
                path[1] == _T(':') && &c == &path[1]) continue;
            return false;
        }
    }
    return true;
}

tstring sanitize_path(const tstring& path) {
    tstring r;
    r.reserve(path.size());
    for (tchar c : path) {
        if (c == _T('<') || c == _T('>')) continue;
        if (c == _T(':') && r.length() >= 2 && r[1] == _T(':')) {
            r += c;
        } else if (c == _T(':')) {
            r += _T('_');
        } else if (c == _T('"') || c == _T('|') || c == _T('?') || c == _T('*')) {
            r += _T('_');
        } else {
            r += c;
        }
    }
    return r;
}

bool is_path_under(const tstring& path, const tstring& base) {
    tstring full = util::get_full_path(path);
    tstring fullBase = util::get_full_path(base);
    if (full.length() < fullBase.length()) return false;
    return _tcsnicmp(full.c_str(), fullBase.c_str(), fullBase.length()) == 0;
}

bool shell_open(const tstring& path, const tstring& verb) {
    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOZONECHECKS;
    sei.lpVerb = verb.c_str();
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteEx(&sei) != 0;
}

bool shell_open_folder_and_select(const tstring& file) {
    PIDL pidl = ILCreateFromPath(file.c_str());
    if (!pidl) return false;
    HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
    ILFree(pidl);
    return SUCCEEDED(hr);
}

void show_file_properties(const tstring& path) {
    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = _T("properties");
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

} // namespace util
} // namespace bandzip
