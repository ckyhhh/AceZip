// ============================================================================
// shell_extension.cpp - Shell 右键菜单扩展实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "shell_extension.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"
#include "../core/archive.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <strsafe.h>
#include <algorithm>

namespace bandzip {
namespace shell {

// ---------------------------------------------------------------------------
// CLSID
// ---------------------------------------------------------------------------
// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
const CLSID CLSID_BandzipShellExt = {
    0xA1B2C3D4, 0xE5F6, 0x7890,
    { 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90 }
};

// ---------------------------------------------------------------------------
// 命令 ID
// ---------------------------------------------------------------------------
enum CommandId {
    CMD_OPEN = 0,
    CMD_EXTRACT_TO_FOLDER,
    CMD_EXTRACT_HERE,
    CMD_EXTRACT_TO,
    CMD_COMPRESS,
    CMD_COMPRESS_TO_ZIP,
    CMD_COMPRESS_TO_7Z,
    CMD_TEST,
    CMD_SEPARATOR1,
    CMD_SEPARATOR2,
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
ShellExtension::ShellExtension() {
    LOG_INFO(L"ShellExtension created");
}

ShellExtension::~ShellExtension() {
    LOG_INFO(L"ShellExtension destroyed");
}

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------
IFACEMETHODIMP ShellExtension::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (riid == IID_IUnknown || riid == IID_IShellExtInit) {
        *ppv = static_cast<IShellExtInit*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IContextMenu) {
        *ppv = static_cast<IContextMenu*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) ShellExtension::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) ShellExtension::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

// ---------------------------------------------------------------------------
// IShellExtInit
// ---------------------------------------------------------------------------
IFACEMETHODIMP ShellExtension::Initialize(LPCITEMIDLIST pidlFolder,
                                            LPDATAOBJECT pDataObj,
                                            HKEY /*hKeyProgID*/) {
    if (!pDataObj) return E_INVALIDARG;

    selected_files_.clear();
    is_folder_ = false;
    is_archive_ = false;
    is_multi_ = false;

    FORMATETC fmt = {
        CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL
    };
    STGMEDIUM stg = {};

    if (FAILED(pDataObj->GetData(&fmt, &stg))) {
        return E_INVALIDARG;
    }

    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    if (!hDrop) {
        ReleaseStgMedium(&stg);
        return E_INVALIDARG;
    }

    UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (count == 0) {
        GlobalUnlock(stg.hGlobal);
        ReleaseStgMedium(&stg);
        return E_INVALIDARG;
    }

    for (UINT i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH] = {0};
        UINT len = DragQueryFile(hDrop, i, path, MAX_PATH);
        if (len > 0) {
            selected_files_.push_back(path);
        }
    }

    GlobalUnlock(stg.hGlobal);
    ReleaseStgMedium(&stg);

    // 分析选中项
    if (selected_files_.size() == 1) {
        DWORD attr = GetFileAttributes(selected_files_[0].c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                is_folder_ = true;
            } else {
                is_archive_ = is_archive_file(selected_files_[0]);
            }
        }
    } else if (selected_files_.size() > 1) {
        is_multi_ = true;
    }

    LOG_INFO(L"ShellExtension initialized: " << selected_files_.size()
              << L" files, folder=" << is_folder_
              << L" archive=" << is_archive_
              << L" multi=" << is_multi_);

    return S_OK;
}

// ---------------------------------------------------------------------------
// IContextMenu::QueryContextMenu
// ---------------------------------------------------------------------------
IFACEMETHODIMP ShellExtension::QueryContextMenu(HMENU hMenu,
                                                  UINT indexMenu,
                                                  UINT idCmdFirst,
                                                  UINT idCmdLast,
                                                  UINT uFlags) {
    // 忽略背景右键（uFlags 包含 CMF_DEFAULTONLY）
    if (uFlags & CMF_DEFAULTONLY) {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }

    // 如果没有选中文件，不显示菜单
    if (selected_files_.empty()) {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }

    UINT id = idCmdFirst;
    HMENU hSubmenu = nullptr;

    if (is_archive_ && !is_multi_) {
        // 单个压缩包
        std::wstring archive_name = selected_files_[0];
        std::wstring folder_name = get_extract_dir_name(archive_name);

        // "用 BandzipClone 打开"
        InsertMenu(hMenu, indexMenu++, MF_SEPARATOR | MF_BYPOSITION, 0, nullptr);

        std::wstring open_text = L"用 BandzipClone 打开";
        InsertMenu(hMenu, indexMenu++, MF_STRING | MF_BYPOSITION,
                   id + CMD_OPEN, open_text.c_str());

        // 子菜单
        hSubmenu = CreatePopupMenu();

        std::wstring extract_to = L"解压到 \"" + folder_name + L"\\\"";
        AppendMenu(hSubmenu, MF_STRING, id + CMD_EXTRACT_TO_FOLDER,
                   extract_to.c_str());

        AppendMenu(hSubmenu, MF_STRING, id + CMD_EXTRACT_HERE,
                   L"解压到当前位置");

        AppendMenu(hSubmenu, MF_STRING, id + CMD_EXTRACT_TO,
                   L"解压到...");

        AppendMenu(hSubmenu, MF_SEPARATOR, 0, nullptr);

        AppendMenu(hSubmenu, MF_STRING, id + CMD_TEST,
                   L"测试压缩包");

        std::wstring submenu_text = L"BandzipClone";
        MENUITEMINFO mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
        mii.wID = id + CMD_SEPARATOR1;
        mii.hSubMenu = hSubmenu;
        mii.dwTypeData = const_cast<LPWSTR>(submenu_text.c_str());
        InsertMenuItem(hMenu, indexMenu++, TRUE, &mii);

        InsertMenu(hMenu, indexMenu++, MF_SEPARATOR | MF_BYPOSITION, 0, nullptr);

    } else {
        // 文件夹或多文件 -> 压缩菜单
        InsertMenu(hMenu, indexMenu++, MF_SEPARATOR | MF_BYPOSITION, 0, nullptr);

        hSubmenu = CreatePopupMenu();

        AppendMenu(hSubmenu, MF_STRING, id + CMD_COMPRESS,
                   L"添加到压缩包...");

        // 根据选中文件生成默认压缩包名
        std::wstring default_name;
        if (is_folder_ && !is_multi_) {
            default_name = selected_files_[0];
            // 去除末尾分隔符
            if (!default_name.empty() &&
                (default_name.back() == L'\\' || default_name.back() == L'/')) {
                default_name.pop_back();
            }
            size_t pos = default_name.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                default_name = default_name.substr(pos + 1);
            }
        } else if (!is_multi_) {
            // 单文件
            default_name = selected_files_[0];
            size_t pos = default_name.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                default_name = default_name.substr(pos + 1);
            }
            // 去除扩展名
            pos = default_name.find_last_of(L'.');
            if (pos != std::wstring::npos) {
                default_name = default_name.substr(0, pos);
            }
        } else {
            // 多文件，使用父目录名
            std::wstring parent = selected_files_[0];
            size_t pos = parent.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                parent = parent.substr(0, pos);
                pos = parent.find_last_of(L"\\/");
                if (pos != std::wstring::npos) {
                    default_name = parent.substr(pos + 1);
                } else {
                    default_name = parent;
                }
            } else {
                default_name = L"archive";
            }
        }

        std::wstring zip_text = L"添加到 \"" + default_name + L".zip\"";
        AppendMenu(hSubmenu, MF_STRING, id + CMD_COMPRESS_TO_ZIP,
                   zip_text.c_str());

        std::wstring sz_text = L"添加到 \"" + default_name + L".7z\"";
        AppendMenu(hSubmenu, MF_STRING, id + CMD_COMPRESS_TO_7Z,
                   sz_text.c_str());

        std::wstring submenu_text = L"BandzipClone";
        MENUITEMINFO mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
        mii.wID = id + CMD_SEPARATOR1;
        mii.hSubMenu = hSubmenu;
        mii.dwTypeData = const_cast<LPWSTR>(submenu_text.c_str());
        InsertMenuItem(hMenu, indexMenu++, TRUE, &mii);

        InsertMenu(hMenu, indexMenu++, MF_SEPARATOR | MF_BYPOSITION, 0, nullptr);
    }

    // 返回使用的最大命令 ID + 1
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_SEPARATOR1 + 1);
}

// ---------------------------------------------------------------------------
// IContextMenu::InvokeCommand
// ---------------------------------------------------------------------------
IFACEMETHODIMP ShellExtension::InvokeCommand(LPCMINVOKECOMMANDINFO pici) {
    if (!pici || !pici->lpVerb) return E_INVALIDARG;

    // 检查是否为字符串命令
    if (HIWORD(pici->lpVerb) != 0) {
        // 字符串命令，不支持
        return E_INVALIDARG;
    }

    UINT cmd = LOWORD(pici->lpVerb);

    switch (cmd) {
    case CMD_OPEN:
        if (!selected_files_.empty()) {
            execute_open(selected_files_[0]);
        }
        break;

    case CMD_EXTRACT_TO_FOLDER: {
        if (!selected_files_.empty()) {
            std::wstring dest = get_extract_dir_name(selected_files_[0]);
            execute_extract(selected_files_[0], dest, false);
        }
        break;
    }

    case CMD_EXTRACT_HERE:
        if (!selected_files_.empty()) {
            // 解压到压缩包所在目录
            std::wstring path = selected_files_[0];
            size_t pos = path.find_last_of(L"\\/");
            std::wstring dir = (pos != std::wstring::npos) ?
                path.substr(0, pos) : L".";
            execute_extract(selected_files_[0], dir, false);
        }
        break;

    case CMD_EXTRACT_TO:
        // 显示解压对话框（调用主程序）
        if (!selected_files_.empty()) {
            std::wstring exe = get_exe_path();
            std::wstring params = L"--extract-dialog \"" + selected_files_[0] + L"\"";

            SHELLEXECUTEINFO sei = {};
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpFile = exe.c_str();
            sei.lpParameters = params.c_str();
            sei.nShow = SW_SHOWNORMAL;
            ShellExecuteEx(&sei);
        }
        break;

    case CMD_COMPRESS:
        // 显示压缩对话框
        {
            std::wstring exe = get_exe_path();
            std::wstring params = L"--compress-dialog";
            for (const auto& f : selected_files_) {
                params += L" \"";
                params += f;
                params += L"\"";
            }

            SHELLEXECUTEINFO sei = {};
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpFile = exe.c_str();
            sei.lpParameters = params.c_str();
            sei.nShow = SW_SHOWNORMAL;
            ShellExecuteEx(&sei);
        }
        break;

    case CMD_COMPRESS_TO_ZIP: {
        // 直接压缩为 ZIP
        std::wstring archive;
        if (is_folder_ && !is_multi_) {
            std::wstring folder = selected_files_[0];
            if (!folder.empty() &&
                (folder.back() == L'\\' || folder.back() == L'/')) {
                folder.pop_back();
            }
            archive = folder + L".zip";
        } else if (!is_multi_) {
            std::wstring file = selected_files_[0];
            size_t pos = file.find_last_of(L"\\/");
            std::wstring dir = (pos != std::wstring::npos) ?
                file.substr(0, pos + 1) : L"";
            std::wstring name = (pos != std::wstring::npos) ?
                file.substr(pos + 1) : file;
            pos = name.find_last_of(L'.');
            if (pos != std::wstring::npos) {
                name = name.substr(0, pos);
            }
            archive = dir + name + L".zip";
        } else {
            // 多文件，使用父目录
            std::wstring parent = selected_files_[0];
            size_t pos = parent.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                parent = parent.substr(0, pos);
                pos = parent.find_last_of(L"\\/");
                std::wstring name = (pos != std::wstring::npos) ?
                    parent.substr(pos + 1) : parent;
                archive = parent + L"\\" + name + L".zip";
            } else {
                archive = L"archive.zip";
            }
        }
        execute_compress(selected_files_, archive, false);
        break;
    }

    case CMD_COMPRESS_TO_7Z: {
        std::wstring archive;
        if (is_folder_ && !is_multi_) {
            std::wstring folder = selected_files_[0];
            if (!folder.empty() &&
                (folder.back() == L'\\' || folder.back() == L'/')) {
                folder.pop_back();
            }
            archive = folder + L".7z";
        } else if (!is_multi_) {
            std::wstring file = selected_files_[0];
            size_t pos = file.find_last_of(L"\\/");
            std::wstring dir = (pos != std::wstring::npos) ?
                file.substr(0, pos + 1) : L"";
            std::wstring name = (pos != std::wstring::npos) ?
                file.substr(pos + 1) : file;
            pos = name.find_last_of(L'.');
            if (pos != std::wstring::npos) {
                name = name.substr(0, pos);
            }
            archive = dir + name + L".7z";
        } else {
            std::wstring parent = selected_files_[0];
            size_t pos = parent.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                parent = parent.substr(0, pos);
                pos = parent.find_last_of(L"\\/");
                std::wstring name = (pos != std::wstring::npos) ?
                    parent.substr(pos + 1) : parent;
                archive = parent + L"\\" + name + L".7z";
            } else {
                archive = L"archive.7z";
            }
        }
        execute_compress(selected_files_, archive, false);
        break;
    }

    case CMD_TEST:
        if (!selected_files_.empty()) {
            execute_test(selected_files_[0]);
        }
        break;

    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

// ---------------------------------------------------------------------------
// IContextMenu::GetCommandString
// ---------------------------------------------------------------------------
IFACEMETHODIMP ShellExtension::GetCommandString(UINT_PTR idCmd,
                                                  UINT uFlags,
                                                  UINT* /*pwReserved*/,
                                                  LPSTR pszName,
                                                  UINT cchMax) {
    if (!pszName || cchMax == 0) return E_INVALIDARG;

    std::wstring help;

    switch (idCmd) {
    case CMD_OPEN:             help = L"用 BandzipClone 打开"; break;
    case CMD_EXTRACT_TO_FOLDER: help = L"解压到新文件夹"; break;
    case CMD_EXTRACT_HERE:     help = L"解压到当前位置"; break;
    case CMD_EXTRACT_TO:       help = L"选择解压目录"; break;
    case CMD_COMPRESS:         help = L"添加到压缩包"; break;
    case CMD_COMPRESS_TO_ZIP:  help = L"快速压缩为 ZIP"; break;
    case CMD_COMPRESS_TO_7Z:   help = L"快速压缩为 7Z"; break;
    case CMD_TEST:             help = L"测试压缩包完整性"; break;
    default:                   return E_INVALIDARG;
    }

    if (uFlags & GCS_HELPTEXTW) {
        wcsncpy_s(reinterpret_cast<LPWSTR>(pszName), cchMax,
                  help.c_str(), _TRUNCATE);
        return S_OK;
    } else if (uFlags & GCS_HELPTEXTA) {
        std::string s = util::tstring_to_ansi(help);
        strncpy_s(pszName, cchMax, s.c_str(), _TRUNCATE);
        return S_OK;
    } else if (uFlags & GCS_VERBW) {
        // 返回动词
        const wchar_t* verb = L"";
        switch (idCmd) {
        case CMD_OPEN:             verb = L"open"; break;
        case CMD_EXTRACT_TO_FOLDER: verb = L"extract_to_folder"; break;
        case CMD_EXTRACT_HERE:     verb = L"extract_here"; break;
        case CMD_EXTRACT_TO:       verb = L"extract_to"; break;
        case CMD_COMPRESS:         verb = L"compress"; break;
        case CMD_COMPRESS_TO_ZIP:  verb = L"compress_zip"; break;
        case CMD_COMPRESS_TO_7Z:   verb = L"compress_7z"; break;
        case CMD_TEST:             verb = L"test"; break;
        }
        wcsncpy_s(reinterpret_cast<LPWSTR>(pszName), cchMax, verb, _TRUNCATE);
        return S_OK;
    }

    return E_INVALIDARG;
}

// ---------------------------------------------------------------------------
// 辅助函数
// ---------------------------------------------------------------------------
std::wstring ShellExtension::get_exe_path() const {
    wchar_t path[MAX_PATH] = {0};
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\BandzipClone", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(path);
        DWORD type = 0;
        if (RegQueryValueEx(hKey, L"ExePath", nullptr, &type,
                            reinterpret_cast<LPBYTE>(path), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return path;
        }
        RegCloseKey(hKey);
    }

    // 默认路径
    GetModuleFileName(nullptr, path, MAX_PATH);
    wchar_t* p = wcsrchr(path, L'\\');
    if (p) {
        *(p + 1) = 0;
        wcscat_s(path, MAX_PATH, L"bandzip.exe");
    }
    return path;
}

bool ShellExtension::is_archive_file(const std::wstring& path) const {
    std::wstring ext = path;
    size_t pos = ext.find_last_of(L'.');
    if (pos == std::wstring::npos) return false;
    ext = ext.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    static const std::vector<std::wstring> exts = {
        L".zip", L".zipx", L".7z", L".rar", L".tar",
        L".gz", L".gzip", L".bz2", L".bzip2", L".xz",
        L".zst", L".zstd", L".lz4", L".cab",
        L".tgz", L".tbz2", L".txz", L".tzst", L".tlz4",
    };

    for (const auto& e : exts) {
        if (ext == e) return true;
    }
    return false;
}

std::wstring ShellExtension::get_extract_dir_name(
    const std::wstring& archive_path) const {

    std::wstring name = archive_path;
    size_t pos = name.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        name = name.substr(pos + 1);
    }
    // 去除扩展名
    pos = name.find_last_of(L'.');
    if (pos != std::wstring::npos) {
        name = name.substr(0, pos);
    }
    return name;
}

void ShellExtension::execute_extract(const std::wstring& archive,
                                       const std::wstring& dest_dir,
                                       bool silent) {
    std::wstring exe = get_exe_path();
    std::wstring params = L"x \"" + archive + L"\" -o\"" + dest_dir + L"\"";
    if (silent) params += L" -y -silent";

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

void ShellExtension::execute_compress(const std::vector<std::wstring>& files,
                                         const std::wstring& archive_path,
                                         bool silent) {
    std::wstring exe = get_exe_path();
    std::wstring params = L"a \"" + archive_path + L"\"";
    for (const auto& f : files) {
        params += L" \"";
        params += f;
        params += L"\"";
    }
    if (silent) params += L" -y -silent";

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

void ShellExtension::execute_test(const std::wstring& archive) {
    std::wstring exe = get_exe_path();
    std::wstring params = L"t \"" + archive + L"\"";

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

void ShellExtension::execute_open(const std::wstring& archive) {
    std::wstring exe = get_exe_path();
    std::wstring params = L"\"" + archive + L"\"";

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

// ---------------------------------------------------------------------------
// 注册/注销
// ---------------------------------------------------------------------------
HRESULT RegisterShellExtension() {
    // 注册 COM 服务器
    std::wstring clsid_str = L"{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}";

    // HKCR\CLSID\{...}
    std::wstring key = L"CLSID\\" + clsid_str;
    HKEY hKey = nullptr;
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, key.c_str(), 0, nullptr,
                        0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return E_FAIL;
    }
    RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                  reinterpret_cast<const BYTE*>(L"BandzipClone Shell Extension"),
                  sizeof(L"BandzipClone Shell Extension"));
    RegCloseKey(hKey);

    // InprocServer32
    std::wstring inproc = key + L"\\InprocServer32";
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT, inproc.c_str(), 0, nullptr,
                        0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return E_FAIL;
    }

    // 获取 DLL 路径
    wchar_t dll_path[MAX_PATH] = {0};
    GetModuleFileName(g_hInstance, dll_path, MAX_PATH);

    RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                  reinterpret_cast<const BYTE*>(dll_path),
                  (wcslen(dll_path) + 1) * sizeof(wchar_t));
    RegSetValueEx(hKey, L"ThreadingModel", 0, REG_SZ,
                  reinterpret_cast<const BYTE*>(L"Apartment"),
                  sizeof(L"Apartment"));
    RegCloseKey(hKey);

    // 注册右键菜单
    // 文件类型
    static const std::vector<std::wstring> types = {
        L"*", L"Directory", L"Folder",
        L"Drive",
        L".zip", L".zipx", L".7z", L".rar", L".tar",
        L".gz", L".gzip", L".bz2", L".bzip2", L".xz",
        L".zst", L".zstd", L".lz4", L".cab",
        L"SystemFileAssociations\\archive",
    };

    for (const auto& type : types) {
        std::wstring shell_key = type + L"\\shellex\\ContextMenuHandlers\\BandzipClone";
        if (RegCreateKeyEx(HKEY_CLASSES_ROOT, shell_key.c_str(), 0, nullptr,
                            0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueEx(hKey, nullptr, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(clsid_str.c_str()),
                          (clsid_str.size() + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
        }
    }

    // 注册 Approved Shell Extensions（防止被禁用）
    std::wstring approved = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, approved.c_str(), 0, nullptr,
                        0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        std::wstring val = L"BandzipClone Shell Extension";
        RegSetValueEx(hKey, clsid_str.c_str(), 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(val.c_str()),
                      (val.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    // 通知 Shell 刷新
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}

HRESULT UnregisterShellExtension() {
    std::wstring clsid_str = L"{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}";

    // 删除右键菜单注册
    static const std::vector<std::wstring> types = {
        L"*", L"Directory", L"Folder",
        L"Drive",
        L".zip", L".zipx", L".7z", L".rar", L".tar",
        L".gz", L".gzip", L".bz2", L".bzip2", L".xz",
        L".zst", L".zstd", L".lz4", L".cab",
        L"SystemFileAssociations\\archive",
    };

    for (const auto& type : types) {
        std::wstring shell_key = type + L"\\shellex\\ContextMenuHandlers\\BandzipClone";
        SHDeleteKey(HKEY_CLASSES_ROOT, shell_key.c_str());
    }

    // 删除 Approved
    std::wstring approved = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, approved.c_str(), 0,
                     KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValue(hKey, clsid_str.c_str());
        RegCloseKey(hKey);
    }

    // 删除 CLSID
    std::wstring key = L"CLSID\\" + clsid_str;
    SHDeleteKey(HKEY_CLASSES_ROOT, key.c_str());

    // 通知 Shell 刷新
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}

} // namespace shell
} // namespace bandzip

// ---------------------------------------------------------------------------
// DLL 导出函数
// ---------------------------------------------------------------------------
HINSTANCE g_hInstance = nullptr;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

extern "C" STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (rclsid == bandzip::shell::CLSID_BandzipShellExt) {
        // 简单的类工厂
        class ClassFactory : public IClassFactory {
        public:
            IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
                if (riid == IID_IUnknown || riid == IID_IClassFactory) {
                    *ppv = this;
                    return S_OK;
                }
                *ppv = nullptr;
                return E_NOINTERFACE;
            }
            IFACEMETHODIMP_(ULONG) AddRef() override { return 1; }
            IFACEMETHODIMP_(ULONG) Release() override { return 1; }

            IFACEMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid,
                                            void** ppv) override {
                if (pOuter) return CLASS_E_NOAGGREGATION;
                auto* obj = new bandzip::shell::ShellExtension();
                HRESULT hr = obj->QueryInterface(riid, ppv);
                obj->Release();
                return hr;
            }

            IFACEMETHODIMP LockServer(BOOL) override {
                return S_OK;
            }
        };

        static ClassFactory factory;
        *ppv = &factory;
        factory.AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" STDAPI DllRegisterServer() {
    return bandzip::shell::RegisterShellExtension();
}

extern "C" STDAPI DllUnregisterServer() {
    return bandzip::shell::UnregisterShellExtension();
}
