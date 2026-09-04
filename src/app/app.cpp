// ============================================================================
// app.cpp - 应用程序入口实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "app.h"
#include "command_line.h"
#include "file_association.h"
#include "../ui/main_window.h"
#include "../ui/dialogs.h"
#include "../core/archive_manager.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <commctrl.h>

namespace bandzip {
namespace app {

Application& Application::instance() {
    static Application inst;
    return inst;
}

bool Application::Init(HINSTANCE hInstance, int nCmdShow) {
    hInstance_ = hInstance;
    nCmdShow_ = nCmdShow;

    // 初始化 COM
    HRESULT hr = OleInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(_T("OleInitialize failed: ") << hr);
        return false;
    }

    // 初始化公共控件
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES |
                ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    // 设置 Duilib 实例
    DuiLib::CPaintManagerUI::SetInstance(hInstance);
    DuiLib::CPaintManagerUI::SetResourcePath(
        util::get_module_dir().c_str());
    DuiLib::CPaintManagerUI::SetResourceType(DuiLib::UILIB_ZIPRESOURCE);

    // 加载配置
    ArchiveManager::instance().load_config();

    // 设置全局回调
    ArchiveManager::instance().set_password_callback(
        [](const tstring& archive_path, bool* cancelled) -> tstring {
            return ui::dialogs::ShowPasswordDialog(nullptr, archive_path, cancelled);
        });

    LOG_INFO(_T("Application initialized"));
    return true;
}

void Application::Run() {
    LOG_INFO(_T("Application running"));

    // 消息循环
    DuiLib::CPaintManagerUI::MessageLoop();
}

void Application::Shutdown() {
    LOG_INFO(_T("Application shutting down"));

    ArchiveManager::instance().save_config();

    OleUninitialize();
}

// ---------------------------------------------------------------------------
// 命令行处理
// ---------------------------------------------------------------------------
int Application::ProcessCommandLine(int argc, tchar* argv[]) {
    CommandLine cmd;
    if (!cmd.Parse(argc, argv)) {
        return 1;
    }

    // 如果没有命令，启动 GUI
    if (cmd.command.empty()) {
        auto* wnd = new ui::MainWindow();
        if (!wnd->Create()) {
            return 1;
        }
        wnd->Show();

        // 如果有文件参数，打开它
        if (!cmd.files.empty()) {
            wnd->OpenArchive(cmd.files[0]);
        }

        Run();
        Shutdown();
        return 0;
    }

    // 处理命令
    if (cmd.command == _T("x") || cmd.command == _T("e")) {
        // 解压
        return cmd.Extract();
    } else if (cmd.command == _T("a")) {
        // 压缩
        return cmd.Compress();
    } else if (cmd.command == _T("t")) {
        // 测试
        return cmd.Test();
    } else if (cmd.command == _T("l") || cmd.command == _T("list")) {
        // 列出
        return cmd.List();
    } else if (cmd.command == _T("--extract-dialog")) {
        // 显示解压对话框
        if (cmd.files.empty()) return 1;
        auto* wnd = new ui::MainWindow();
        wnd->Create();
        wnd->Show();
        wnd->ShowExtractDialog(cmd.files[0]);
        Run();
        Shutdown();
        return 0;
    } else if (cmd.command == _T("--compress-dialog")) {
        // 显示压缩对话框
        auto* wnd = new ui::MainWindow();
        wnd->Create();
        wnd->Show();
        wnd->ShowCompressDialog(cmd.files);
        Run();
        Shutdown();
        return 0;
    } else if (cmd.command == _T("--register")) {
        // 注册文件关联
        RegisterFileAssociations();
        return 0;
    } else if (cmd.command == _T("--unregister")) {
        // 注销文件关联
        UnregisterFileAssociations();
        return 0;
    } else if (cmd.command == _T("--register-shell")) {
        RegisterShellExtension();
        return 0;
    } else if (cmd.command == _T("--unregister-shell")) {
        UnregisterShellExtension();
        return 0;
    } else if (cmd.command == _T("--help") || cmd.command == _T("-h") ||
               cmd.command == _T("/?")) {
        cmd.PrintHelp();
        return 0;
    } else if (cmd.command == _T("--version")) {
        _tprintf(_T("BandzipClone 0.9.0\n"));
        _tprintf(_T("Copyright (C) 2024 BandzipClone Contributors\n"));
        _tprintf(_T("License: AGPLv3\n"));
        return 0;
    } else {
        // 未知命令，当作文件名打开
        auto* wnd = new ui::MainWindow();
        wnd->Create();
        wnd->Show();
        wnd->OpenArchive(cmd.command);
        Run();
        Shutdown();
        return 0;
    }
}

// ---------------------------------------------------------------------------
// 文件关联
// ---------------------------------------------------------------------------
bool Application::RegisterFileAssociations() {
    return file_association::RegisterAll();
}

bool Application::UnregisterFileAssociations() {
    return file_association::UnregisterAll();
}

// ---------------------------------------------------------------------------
// Shell 扩展
// ---------------------------------------------------------------------------
bool Application::RegisterShellExtension() {
    // 调用 regsvr32 注册 DLL
    tstring module_dir = util::get_module_dir();
    tstring dll_path = module_dir + _T("\\bzshell.dll");

    tstring cmd = _T("regsvr32 /s \"") + dll_path + _T("\"");

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = _T("runas");
    sei.lpFile = _T("regsvr32");
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_HIDE;
    return ShellExecuteEx(&sei) != 0;
}

bool Application::UnregisterShellExtension() {
    tstring module_dir = util::get_module_dir();
    tstring dll_path = module_dir + _T("\\bzshell.dll");

    tstring cmd = _T("regsvr32 /u /s \"") + dll_path + _T("\"");

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = _T("runas");
    sei.lpFile = _T("regsvr32");
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_HIDE;
    return ShellExecuteEx(&sei) != 0;
}

} // namespace app
} // namespace bandzip

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow) {
    // 解析命令行
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW) return 1;

    // 转换参数
    std::vector<std::wstring> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(argvW[i]);
    }
    LocalFree(argvW);

    // 初始化
    if (!bandzip::app::Application::instance().Init(hInstance, nCmdShow)) {
        return 1;
    }

    // 处理命令行
    int ret = bandzip::app::Application::instance().ProcessCommandLine(
        static_cast<int>(args.size()),
        const_cast<tchar**>(args.data()));

    return ret;
}
