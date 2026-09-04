// ============================================================================
// app.h - 应用程序入口
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

namespace bandzip {
namespace app {

class Application {
public:
    static Application& instance();

    // 初始化
    bool Init(HINSTANCE hInstance, int nCmdShow);
    void Run();
    void Shutdown();

    // 命令行处理
    int ProcessCommandLine(int argc, tchar* argv[]);

    // 文件关联
    bool RegisterFileAssociations();
    bool UnregisterFileAssociations();

    // Shell 扩展注册
    bool RegisterShellExtension();
    bool UnregisterShellExtension();

    HINSTANCE instance() const { return hInstance_; }
    int cmd_show() const { return nCmdShow_; }

private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    HINSTANCE hInstance_ = nullptr;
    int nCmdShow_ = SW_SHOWNORMAL;
};

} // namespace app
} // namespace bandzip
