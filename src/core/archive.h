// ============================================================================
// archive.h - 归档抽象接口定义
//
// 定义了所有压缩格式驱动必须实现的统一接口 IArchive。
// 上层 UI 与业务逻辑只依赖此接口，不感知具体格式。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <system_error>

namespace bandzip {

// ---------------------------------------------------------------------------
// 基本类型
// ---------------------------------------------------------------------------
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

#ifdef _WIN32
using tstring = std::wstring;       // Windows 用 wstring
using tchar   = wchar_t;
#else
using tstring = std::string;
using tchar   = char;
#endif

// ---------------------------------------------------------------------------
// 错误码
// ---------------------------------------------------------------------------
enum class ArchiveError {
    Ok = 0,
    OpenFailed,
    ReadFailed,
    WriteFailed,
    BadFormat,
    UnsupportedMethod,
    WrongPassword,
    PasswordRequired,
    Truncated,
    CRCMismatch,
    DiskFull,
    Cancelled,
    FileNotFound,
    InvalidParameter,
    InternalError,
    UnsupportedFeature,
};

const std::error_category& archive_category();
std::error_code make_error_code(ArchiveError e);

// ---------------------------------------------------------------------------
// 压缩格式枚举
// ---------------------------------------------------------------------------
enum class ArchiveFormat {
    Unknown = 0,
    Zip,        // .zip
    SevenZip,   // .7z
    Rar,        // .rar
    Tar,        // .tar
    Gzip,       // .gz
    Bzip2,      // .bz2
    Xz,         // .xz
    Zstd,       // .zst
    Lz4,        // .lz4
    Cab,        // .cab
    Wim,        // .wim
    Iso,        // .iso
    Arj,        // .arj
    Lzh,        // .lzh
};

// 格式与扩展名互转
const tchar* format_to_extension(ArchiveFormat fmt);
ArchiveFormat extension_to_format(const tstring& path);
const tchar* format_display_name(ArchiveFormat fmt);

// ---------------------------------------------------------------------------
// 压缩方法（每种格式支持的算法不同）
// ---------------------------------------------------------------------------
enum class CompressionMethod {
    Copy,           // 仅存储
    Deflate,        // ZIP
    Deflate64,      // ZIP
    Bzip2,          // ZIP/7Z
    Lzma,           // 7Z/ZIP
    Lzma2,          // 7Z/XZ
    Ppmd,           // 7Z/ZIP
    Zstd,           // 7Z/ZIP/独立
    Lz4,            // 独立
    Rar,            // RAR
    Rar5,           // RAR5
    Aes,            // 加密（非压缩）
};

// ---------------------------------------------------------------------------
// 加密算法
// ---------------------------------------------------------------------------
enum class EncryptionMethod {
    None,
    ZipCrypto,      // 传统 ZIP 加密（弱）
    Aes128,
    Aes192,
    Aes256,
    RarAes,         // RAR 私有
};

// ---------------------------------------------------------------------------
// 文件项信息
// ---------------------------------------------------------------------------
struct ArchiveEntry {
    u32 index = 0;                  // 在归档中的索引
    tstring path;                   // 相对路径（含目录分隔符 /）
    tstring name;                   // 文件名（不含路径）
    u64 size = 0;                   // 原始大小（字节）
    u64 packed_size = 0;            // 压缩后大小
    u64 crc32 = 0;                  // CRC32 校验值
    std::time_t modified = 0;       // 修改时间（Unix 时间戳）
    std::time_t created = 0;        // 创建时间
    std::time_t accessed = 0;       // 访问时间
    u32 attributes = 0;             // Windows 文件属性
    bool is_directory = false;      // 是否目录
    bool is_encrypted = false;      // 是否加密
    bool is_sym_link = false;       // 是否符号链接
    CompressionMethod method = CompressionMethod::Copy;
    EncryptionMethod encryption = EncryptionMethod::None;
    tstring comment;                // 文件注释
};

// ---------------------------------------------------------------------------
// 进度回调
// ---------------------------------------------------------------------------
struct ProgressInfo {
    u64 total_bytes = 0;            // 总字节数（0 表示未知）
    u64 completed_bytes = 0;        // 已完成字节数
    u32 total_files = 0;            // 总文件数
    u32 completed_files = 0;        // 已完成文件数
    tstring current_file;          // 当前处理的文件
    u32 file_progress = 0;         // 当前文件进度（0-100）
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point now;
};

// 进度回调返回 false 表示取消操作
using ProgressCallback = std::function<bool(const ProgressInfo&)>;

// ---------------------------------------------------------------------------
// 密码回调（用于解压时按需询问密码）
// ---------------------------------------------------------------------------
struct PasswordRequest {
    tstring archive_path;           // 归档路径
    tstring file_path;              // 触发密码的文件
    bool first_attempt = true;     // 是否首次询问
    bool wrong_password = false;    // 上次密码错误
};

struct PasswordResponse {
    tstring password;               // 用户输入的密码
    bool remember = false;          // 是否记住密码
    bool cancel = false;            // 用户取消
};

using PasswordCallback = std::function<PasswordResponse(const PasswordRequest&)>;

// ---------------------------------------------------------------------------
// 解压选项
// ---------------------------------------------------------------------------
struct ExtractOptions {
    tstring target_dir;             // 目标目录
    bool overwrite = true;          // 覆盖已存在文件
    bool keep_paths = true;         // 保留目录结构
    bool skip_paths_prefix = false;// 跳过公共前缀目录
    bool skip_symlinks = false;     // 跳过符号链接
    bool skip_hardlinks = false;   // 跳过硬链接
    bool create_dirs = true;       // 自动创建目录
    bool test_only = false;        // 仅测试，不解压
    bool keep_broken = false;      // 保留损坏文件
    tstring password;               // 密码（可空）
    tstring charset;                // 强制字符集（空=自动检测）
    ProgressCallback progress;     // 进度回调
    PasswordCallback password_cb;  // 密码回调
    std::vector<u32> selected;     // 仅解压指定索引（空=全部）
};

// ---------------------------------------------------------------------------
// 压缩选项
// ---------------------------------------------------------------------------
struct CompressOptions {
    tstring archive_path;           // 输出归档路径
    ArchiveFormat format = ArchiveFormat::Zip;
    CompressionMethod method = CompressionMethod::Deflate;
    int level = 5;                  // 0=存储 1=最快 6=默认 9=极限
    int dictionary_size = 0;        // 0=自动，单位 KB
    int word_size = 0;              // 0=自动
    bool solid = false;             // 固实压缩（7Z）
    bool create_sfx = false;        // 创建自解压
    u64 volume_size = 0;            // 分卷大小（0=不分卷）
    tstring password;               // 密码（空=不加密）
    EncryptionMethod encryption = EncryptionMethod::None;
    bool encrypt_headers = false;   // 加密文件名（7Z/RAR）
    tstring charset;                // 字符集（默认 UTF-8）
    bool store_paths = true;        // 保留路径
    bool store_attributes = true;   // 保留属性
    bool store_times = true;        // 保留时间
    tstring comment;                // 归档注释
    ProgressCallback progress;      // 进度回调
    std::vector<tstring> exclude;   // 排除模式
};

// ---------------------------------------------------------------------------
// 创建选项（用于"新建压缩包"对话框）
// ---------------------------------------------------------------------------
struct CreateOptions : CompressOptions {
    std::vector<tstring> sources;  // 待压缩的源文件/目录
};

// ---------------------------------------------------------------------------
// 归档接口（抽象基类）
// ---------------------------------------------------------------------------
class IArchive {
public:
    virtual ~IArchive() = default;

    // 打开归档（只读）
    virtual std::error_code open(const tstring& path,
                                const tstring& password = tstring()) = 0;

    // 关闭归档
    virtual void close() = 0;

    // 是否已打开
    virtual bool is_open() const = 0;

    // 获取格式
    virtual ArchiveFormat format() const = 0;

    // 获取归档路径
    virtual const tstring& path() const = 0;

    // 获取文件项数量
    virtual u32 entry_count() const = 0;

    // 获取指定索引的文件项
    virtual const ArchiveEntry& entry(u32 index) const = 0;

    // 获取所有文件项
    virtual const std::vector<ArchiveEntry>& entries() const = 0;

    // 是否需要密码
    virtual bool needs_password() const = 0;

    // 设置密码（用于后续解压）
    virtual std::error_code set_password(const tstring& password) = 0;

    // 解压单个文件到指定路径
    virtual std::error_code extract(u32 index,
                                    const tstring& target_path,
                                    const ExtractOptions& opts) = 0;

    // 解压多个文件
    virtual std::error_code extract_multi(const std::vector<u32>& indices,
                                          const ExtractOptions& opts) = 0;

    // 测试压缩包完整性
    virtual std::error_code test(const ProgressCallback& cb = {}) = 0;

    // 获取归档注释
    virtual tstring comment() const = 0;

    // 是否支持写入
    virtual bool can_write() const = 0;

    // 创建新归档并添加文件
    virtual std::error_code create(const CreateOptions& opts) = 0;

    // 添加文件到已存在的归档（若支持）
    virtual std::error_code add_files(const std::vector<tstring>& files,
                                      const CompressOptions& opts) = 0;

    // 删除归档中的文件（若支持）
    virtual std::error_code delete_entries(const std::vector<u32>& indices) = 0;
};

// ---------------------------------------------------------------------------
// 工厂函数
// ---------------------------------------------------------------------------
std::unique_ptr<IArchive> create_archive(ArchiveFormat fmt);
std::unique_ptr<IArchive> open_archive(const tstring& path,
                                       const tstring& password = tstring(),
                                       std::error_code* ec = nullptr);

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------
namespace util {

// 检测文件格式（基于魔数）
ArchiveFormat detect_format(const tstring& path);

// 获取格式支持的所有扩展名
std::vector<tstring> get_extensions(ArchiveFormat fmt);

// 格式是否支持写入
bool format_supports_write(ArchiveFormat fmt);

// 格式是否支持加密
bool format_supports_encryption(ArchiveFormat fmt);

// 格式是否支持分卷
bool format_supports_volumes(ArchiveFormat fmt);

// 格式是否支持固实压缩
bool format_supports_solid(ArchiveFormat fmt);

// 获取格式支持的压缩方法
std::vector<CompressionMethod> get_supported_methods(ArchiveFormat fmt);

} // namespace util

} // namespace bandzip

// 为 ArchiveError 启用 std::error_code 隐式转换
namespace std {
template<>
struct is_error_code_enum<bandzip::ArchiveError> : true_type {};
}
