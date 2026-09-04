// ============================================================================
// file_association.cpp - 文件关联实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "file_association.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <shlobj.h>
#include <algorithm>

namespace bandzip {
namespace app {
namespace file_association {

// ---------------------------------------------------------------------------
// 支持的扩展名
// ---------------------------------------------------------------------------
struct ExtensionInfo {
    const tchar* ext;
    const tchar* description;
    int icon_id;
};

static const ExtensionInfo kExtensions[] = {
    { _T(".zip"),    _T("ZIP 压缩包"),          300 },
    { _T(".zipx"),   _T("ZIPX 压缩包"),         300 },
    { _T(".7z"),     _T("7Z 压缩包"),           301 },
    { _T(".rar"),    _T("RAR 压缩包"),          302 },
    { _T(".tar"),    _T("TAR 归档"),            303 },
    { _T(".gz"),     _T("GZ 压缩文件"),         304 },
    { _T(".gzip"),   _T("GZIP 压缩文件"),       304 },
    { _T(".tgz"),    _T("TAR.GZ 压缩包"),       304 },
    { _T(".bz2"),    _T("BZ2 压缩文件"),        305 },
    { _T(".bzip2"),  _T("BZIP2 压缩文件"),      305 },
    { _T(".tbz2"),   _T("TAR.BZ2 压缩包"),      305 },
    { _T(".xz"),     _T("XZ 压缩文件"),         306 },
    { _T(".txz"),    _T("TAR.XZ 压缩包"),       306 },
    { _T(".zst"),    _T("ZSTD 压缩文件"),       307 },
    { _T(".zstd"),   _T("ZSTD 压缩文件"),       307 },
    { _T(".tzst"),   _T("TAR.ZST 压缩包"),      307 },
    { _T(".lz4"),    _T("LZ4 压缩文件"),        308 },
    { _T(".tlz4"),   _T("TAR.LZ4 压缩包"),      308 },
    { _T(".cab"),    _T("CAB 压缩包"),          309 },
    { _T(".001"),    _T("分卷压缩包"),          310 },
};

static const tchar* kProgId = _T("BandzipClone.Archive");

// ---------------------------------------------------------------------------
// 注册所有
// ---------------------------------------------------------------------------
bool RegisterAll() {
    tstring exe_path = util::get_module_path();

    // 注册 ProgID
    tstring progid_key = tstring(_T("Software\\Classes\\")) + kProgId;
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, progid_key.c_str(),
                        0, nullptr, 0, KEY_WRITE, nullptr, &hKey,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }

    // 默认值
    tstring desc = _T("BandzipClone Archive");
    RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                  reinterpret_cast<const BYTE*>(desc.c_str()),
                  static_cast<DWORD>((desc.size() + 1) * sizeof(tchar)));

    // 图标
    HKEY hIconKey;
    tstring icon_key = progid_key + _T("\\DefaultIcon");
    if (RegCreateKeyEx(HKEY_CURRENT_USER, icon_key.c_str(),
                        0, nullptr, 0, KEY_WRITE, nullptr, &hIconKey,
                        nullptr) == ERROR_SUCCESS) {
        tstring icon_val = exe_path + _T(",0");
        RegSetValueEx(hIconKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(icon_val.c_str()),
                      static_cast<DWORD>((icon_val.size() + 1) * sizeof(tchar)));
        RegCloseKey(hIconKey);
    }

    // 打开命令
    HKEY hCmdKey;
    tstring cmd_key = progid_key + _T("\\shell\\open\\command");
    if (RegCreateKeyEx(HKEY_CURRENT_USER, cmd_key.c_str(),
                        0, nullptr, 0, KEY_WRITE, nullptr, &hCmdKey,
                        nullptr) == ERROR_SUCCESS) {
        tstring cmd_val = _T("\"") + exe_path + _T("\" \"%1\"");
        RegSetValueEx(hCmdKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(cmd_val.c_str()),
                      static_cast<DWORD>((cmd_val.size() + 1) * sizeof(tchar)));
        RegCloseKey(hCmdKey);
    }

    RegCloseKey(hKey);

    // 注册每个扩展名
    for (const auto& ext : kExtensions) {
        RegisterFormat(ext.ext, ext.description, ext.icon_id);
    }

    // 通知 Shell 刷新
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    LOG_INFO(_T("File associations registered"));
    return true;
}

// ---------------------------------------------------------------------------
// 注销所有
// ---------------------------------------------------------------------------
bool UnregisterAll() {
    for (const auto& ext : kExtensions) {
        UnregisterFormat(ext.ext);
    }

    // 删除 ProgID
    tstring progid_key = tstring(_T("Software\\Classes\\")) + kProgId;
    SHDeleteKey(HKEY_CURRENT_USER, progid_key.c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    LOG_INFO(_T("File associations unregistered"));
    return true;
}

// ---------------------------------------------------------------------------
// 注册单个格式
// ---------------------------------------------------------------------------
bool RegisterFormat(const tstring& ext, const tstring& description,
                     int icon_id) {
    tstring exe_path = util::get_module_path();

    // .ext -> ProgID
    tstring ext_key = tstring(_T("Software\\Classes\\")) + ext;
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, ext_key.c_str(),
                        0, nullptr, 0, KEY_WRITE, nullptr, &hKey,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }

    RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                  reinterpret_cast<const BYTE*>(kProgId),
                  static_cast<DWORD>((_tcslen(kProgId) + 1) * sizeof(tchar)));

    // 描述
    HKEY hDescKey;
    if (RegCreateKeyEx(hKey, _T("BandzipClone.Description"), 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hDescKey,
                        nullptr) == ERROR_SUCCESS) {
        RegSetValueEx(hDescKey, nullptr, 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(description.c_str()),
                      static_cast<DWORD>((description.size() + 1) * sizeof(tchar)));
        RegCloseKey(hDescKey);
    }

    RegCloseKey(hKey);

    return true;
}

// ---------------------------------------------------------------------------
// 注销单个格式
// ---------------------------------------------------------------------------
bool UnregisterFormat(const tstring& ext) {
    tstring ext_key = tstring(_T("Software\\Classes\\")) + ext;
    SHDeleteKey(HKEY_CURRENT_USER, ext_key.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// 检查是否已注册
// ---------------------------------------------------------------------------
bool IsRegistered(const tstring& ext) {
    tstring ext_key = tstring(_T("Software\\Classes\\")) + ext;
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, ext_key.c_str(),
                     0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    tchar buf[MAX_PATH] = {0};
    DWORD size = sizeof(buf);
    DWORD type;
    bool found = false;
    if (RegQueryValueEx(hKey, nullptr, nullptr, &type,
                         reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS) {
        if (_tcscmp(buf, kProgId) == 0) {
            found = true;
        }
    }

    RegCloseKey(hKey);
    return found;
}

// ---------------------------------------------------------------------------
// 获取所有支持的扩展名
// ---------------------------------------------------------------------------
std::vector<tstring> GetSupportedExtensions() {
    std::vector<tstring> result;
    for (const auto& ext : kExtensions) {
        result.push_back(ext.ext);
    }
    return result;
}

// ---------------------------------------------------------------------------
// 获取格式描述
// ---------------------------------------------------------------------------
tstring GetFormatDescription(const tstring& ext) {
    for (const auto& e : kExtensions) {
        if (_tcsicmp(e.ext, ext.c_str()) == 0) {
            return e.description;
        }
    }
    return _T("压缩包");
}

} // namespace file_association
} // namespace app
} // namespace bandzip
