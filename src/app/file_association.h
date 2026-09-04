// ============================================================================
// file_association.h - 文件关联
//
// 将 BandzipClone 注册为支持格式的默认程序。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"

namespace bandzip {
namespace app {
namespace file_association {

// 注册所有支持的格式
bool RegisterAll();

// 注销所有支持的格式
bool UnregisterAll();

// 注册单个格式
bool RegisterFormat(const tstring& ext, const tstring& description,
                     int icon_id = 0);

// 注销单个格式
bool UnregisterFormat(const tstring& ext);

// 检查是否已注册
bool IsRegistered(const tstring& ext);

// 获取所有支持的扩展名
std::vector<tstring> GetSupportedExtensions();

// 获取格式描述
tstring GetFormatDescription(const tstring& ext);

} // namespace file_association
} // namespace app
} // namespace bandzip
