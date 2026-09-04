// ============================================================================
// shell_extension.h - Shell 右键菜单扩展
//
// 实现 IShellExtInit + IContextMenu 接口，将 BandzipClone 集成到
// Windows 资源管理器的右键菜单中。
//
// 菜单项：
// - 用 BandzipClone 打开
// - 解压到 <文件夹名>\
// - 解压到当前位置
// - 解压到...
// - 添加到压缩包...
// - 测试压缩包
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <vector>
#include <string>

namespace bandzip {
namespace shell {

class ShellExtension : public IShellExtInit, public IContextMenu {
public:
    ShellExtension();
    virtual ~ShellExtension();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    IFACEMETHODIMP Initialize(LPCITEMIDLIST pidlFolder,
                              LPDATAOBJECT pDataObj,
                              HKEY hKeyProgID) override;

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu,
                                    UINT idCmdFirst, UINT idCmdLast,
                                    UINT uFlags) override;
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici) override;
    IFACEMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uFlags,
                                    UINT* pwReserved, LPSTR pszName,
                                    UINT cchMax) override;

private:
    LONG ref_count_ = 1;

    // 选中的文件列表
    std::vector<std::wstring> selected_files_;

    // 是否为文件夹
    bool is_folder_ = false;

    // 是否为压缩包
    bool is_archive_ = false;

    // 是否多选
    bool is_multi_ = false;

    // 获取 BandzipClone 可执行文件路径
    std::wstring get_exe_path() const;

    // 检测文件是否为压缩包
    bool is_archive_file(const std::wstring& path) const;

    // 获取建议的解压目录名
    std::wstring get_extract_dir_name(const std::wstring& archive_path) const;

    // 执行命令
    void execute_extract(const std::wstring& archive,
                         const std::wstring& dest_dir,
                         bool silent = false);
    void execute_compress(const std::vector<std::wstring>& files,
                          const std::wstring& archive_path = L"",
                          bool silent = false);
    void execute_test(const std::wstring& archive);
    void execute_open(const std::wstring& archive);
};

// CLSID
// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
extern const CLSID CLSID_BandzipShellExt;

// 注册/注销
HRESULT RegisterShellExtension();
HRESULT UnregisterShellExtension();

} // namespace shell
} // namespace bandzip
