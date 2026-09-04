// ============================================================================
// file_utils.h - 文件操作工具
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <vector>

namespace bandzip {
namespace util {

// 文件/目录存在性
bool file_exists(const tstring& path);
bool dir_exists(const tstring& path);

// 创建/删除
bool create_dir(const tstring& path);
bool create_dir_recursive(const tstring& path);
bool delete_file(const tstring& path);
bool delete_dir(const tstring& path, bool recursive = true);

// 移动/复制
bool move_file(const tstring& from, const tstring& to, bool overwrite = true);
bool copy_file(const tstring& from, const tstring& to, bool overwrite = true);

// 文件大小
u64 file_size(const tstring& path);

// 临时文件
tstring get_temp_path();
tstring create_temp_file(const tstring& prefix = _T("bz"),
                         const tstring& ext = _T(".tmp"));
tstring create_temp_dir(const tstring& prefix = _T("bz"));

// 系统目录
tstring get_appdata_dir();
tstring get_desktop_dir();
tstring get_module_dir();
tstring get_current_dir();
bool set_current_dir(const tstring& dir);

// 遍历目录
struct FindData {
    tstring path;
    tstring name;
    u64 size;
    bool is_directory;
    bool is_readonly;
    bool is_hidden;
    bool is_system;
    std::time_t modified;
    std::time_t created;
    std::time_t accessed;
    u32 attributes;
};

std::vector<FindData> find_files(const tstring& dir,
                                  const tstring& pattern = _T("*"));
bool walk_dir(const tstring& dir,
              const std::function<bool(const FindData&)>& callback,
              bool recursive = true);

// 文件读写
std::vector<u8> read_file_all(const tstring& path);
bool write_file_all(const tstring& path, const void* data, size_t size);

// 路径合法性
bool is_valid_path(const tstring& path);
tstring sanitize_path(const tstring& path);

// 检查路径是否在指定目录下（防止 zip slip 攻击）
bool is_path_under(const tstring& path, const tstring& base);

// 打开文件（ShellExecute）
bool shell_open(const tstring& path, const tstring& verb = _T("open"));
bool shell_open_folder_and_select(const tstring& file);

// 显示属性对话框
void show_file_properties(const tstring& path);

} // namespace util
} // namespace bandzip
