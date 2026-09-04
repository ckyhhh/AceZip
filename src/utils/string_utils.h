// ============================================================================
// string_utils.h - 字符串工具
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <string>
#include <vector>

namespace bandzip {
namespace util {

// tstring <-> std::string（UTF-8）
std::string  tstring_to_string(const tstring& ts);
tstring      string_to_tstring(const std::string& s);

// tstring <-> std::string（本地 ANSI）
std::string  tstring_to_ansi(const tstring& ts);
tstring      ansi_to_tstring(const std::string& s);

// UTF-8 <-> UTF-16
std::wstring utf8_to_utf16(const std::string& s);
std::string  utf16_to_utf8(const std::wstring& ws);

// GBK <-> UTF-16（用于 ZIP 中文乱码）
std::wstring gbk_to_utf16(const std::string& s);
std::string  utf16_to_gbk(const std::wstring& ws);

// Shift-JIS <-> UTF-16
std::wstring sjis_to_utf16(const std::string& s);

// Big5 <-> UTF-16
std::wstring big5_to_utf16(const std::string& s);

// 通用代码页转换
std::wstring cp_to_utf16(const std::string& s, int codepage);
std::string  utf16_to_cp(const std::wstring& ws, int codepage);

// 大小写转换
tstring to_lower(const tstring& s);
tstring to_upper(const tstring& s);

// 分割
std::vector<tstring> split(const tstring& s, tchar sep);
tstring join(const std::vector<tstring>& v, const tstring& sep);

// 去除空白
tstring trim(const tstring& s);
tstring trim_left(const tstring& s);
tstring trim_right(const tstring& s);

// 替换
tstring replace_all(const tstring& s, const tstring& from, const tstring& to);

// 路径相关
tstring get_extension(const tstring& path);
tstring get_extension_lower(const tstring& path);
tstring get_filename(const tstring& path);
tstring get_basename(const tstring& path);
tstring get_dirname(const tstring& path);

// 格式化
tstring format(const tchar* fmt, ...);

// 数字格式化（带千分位）
tstring format_size(u64 bytes);
tstring format_count(u64 count);

// 时间格式化
tstring format_time(std::time_t t);

} // namespace util
} // namespace bandzip
