// ============================================================================
// path_utils.h - 路径处理工具
// ============================================================================
#pragma once

#include "../core/archive.h"

namespace bandzip {
namespace util {

// 路径分隔符规范化（统一为 \）
tstring normalize_path(const tstring& path);

// 获取绝对路径
tstring get_full_path(const tstring& path);

// 路径拼接
tstring join_path(const tstring& a, const tstring& b);
tstring join_path(const std::vector<tstring>& parts);

// 路径相对化
tstring make_relative(const tstring& from, const tstring& to);

// 归档内路径处理
// 将 Windows 风格路径转为归档内风格（用 / 分隔）
tstring to_archive_path(const tstring& windows_path);

// 将归档内路径转为 Windows 风格
tstring to_windows_path(const tstring& archive_path);

// 提取公共前缀目录
tstring common_prefix(const std::vector<tstring>& paths);

// 检测路径是否包含 .. （zip slip）
bool has_parent_ref(const tstring& path);

// 检测路径是否绝对
bool is_absolute(const tstring& path);

// 检测路径是否为 UNC
bool is_unc(const tstring& path);

// FILETIME <-> Unix 时间戳
std::time_t filetime_to_unix(const FILETIME& ft);
FILETIME unix_to_filetime(std::time_t t);

// DOS 时间 <-> Unix 时间戳
std::time_t dostime_to_unix(u16 dos_date, u16 dos_time);
void unix_to_dostime(std::time_t t, u16& dos_date, u16& dos_time);

// Windows 文件属性 <-> 归档属性
u32 win_attr_to_archive(u32 win_attr);
u32 archive_attr_to_win(u32 archive_attr);

} // namespace util
} // namespace bandzip
