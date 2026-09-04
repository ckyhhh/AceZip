// ============================================================================
// sfx.h - 自解压模块（SFX）
//
// 创建自解压可执行文件。SFX 文件结构：
//
//   ┌──────────────────────┐
//   │  SFX Stub (EXE 头)   │  ← bandzip.sfx
//   ├──────────────────────┤
//   │  压缩包数据          │  ← ZIP/7Z 数据
//   ├──────────────────────┤
//   │  SFX 元信息（可选）  │  ← 配置（标题、默认目录等）
//   └──────────────────────┘
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <string>

namespace bandzip {
namespace sfx {

// SFX 配置
struct SfxConfig {
    tstring title;              // 窗口标题
    tstring default_dir;        // 默认解压目录
    tstring text;                // 提示文本
    bool overwrite = false;      // 覆盖现有文件
    bool auto_extract = false;   // 自动解压
    bool auto_run = false;       // 解压后自动运行
    tstring run_command;         // 运行命令
    bool show_progress = true;   // 显示进度
    bool silent = false;        // 静默模式
    bool create_shortcut = false; // 创建桌面快捷方式
    tstring shortcut_target;     // 快捷方式目标
    tstring shortcut_name;       // 快捷方式名称
};

// 创建 SFX 文件
// archive_path: 源压缩包路径
// sfx_path: 输出 SFX 文件路径
// config: SFX 配置
std::error_code CreateSfx(const tstring& archive_path,
                          const tstring& sfx_path,
                          const SfxConfig& config);

// 检查文件是否为 SFX
bool IsSfx(const tstring& path);

// 从 SFX 中提取压缩包数据
std::error_code ExtractSfxArchive(const tstring& sfx_path,
                                   const tstring& output_path);

// 获取 SFX 配置
std::error_code GetSfxConfig(const tstring& sfx_path, SfxConfig& config);

// SFX Stub 运行时入口（编译为 sfx_stub.exe）
int SfxMain();

} // namespace sfx
} // namespace bandzip
