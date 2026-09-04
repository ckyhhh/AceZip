// ============================================================================
// command_line.h - 命令行参数解析
//
// 支持的命令：
//   bandzip.exe [archive]                    浏览压缩包
//   bandzip.exe x [archive] [-o<dir>] [-p<pwd>] [-y] [-silent]   解压
//   bandzip.exe a [archive] [files...] [-fmt<zip|7z|...>] [-level<0-9>] [-p<pwd>]
//   bandzip.exe t [archive]                  测试
//   bandzip.exe l [archive]                  列出
//   bandzip.exe --extract-dialog [archive]   显示解压对话框
//   bandzip.exe --compress-dialog [files...] 显示压缩对话框
//   bandzip.exe --register                    注册文件关联
//   bandzip.exe --unregister                  注销文件关联
//   bandzip.exe --register-shell              注册 Shell 扩展
//   bandzip.exe --unregister-shell            注销 Shell 扩展
//   bandzip.exe --help                        帮助
//   bandzip.exe --version                     版本
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <vector>
#include <string>

namespace bandzip {
namespace app {

class CommandLine {
public:
    CommandLine() = default;
    ~CommandLine() = default;

    // 解析命令行
    bool Parse(int argc, tchar* argv[]);

    // 执行命令
    int Extract();
    int Compress();
    int Test();
    int List();

    // 打印帮助
    void PrintHelp() const;

    // 命令
    tstring command;

    // 文件列表
    std::vector<tstring> files;

    // 选项
    tstring output_dir;
    tstring password;
    tstring archive_path;
    tstring format = _T("zip");
    int level = 5;
    u64 volume_size = 0;
    bool yes = false;
    bool silent = false;
    bool solid = false;
    bool encrypt_names = false;
    bool delete_after = false;
    bool open_after = false;
    tstring log_file;
    bool overwrite = true;
    bool keep_broken = false;
    tstring encoding = _T("auto");

private:
    // 解析单个选项
    bool ParseOption(const tstring& opt);

    // 检测格式
    ArchiveFormat DetectFormat(const tstring& path) const;

    // 进度回调
    static void ProgressCallback(const ProgressInfo& info);
    static tstring PasswordCallback(const tstring& path, bool* cancelled);

    // 输出
    void Print(const tstring& msg) const;
    void PrintError(const tstring& msg) const;
};

} // namespace app
} // namespace bandzip
