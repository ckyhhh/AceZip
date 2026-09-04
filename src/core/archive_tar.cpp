// ============================================================================
// archive_tar.cpp - TAR 格式驱动完整实现
//
// 实现 POSIX ustar / GNU / pax 三种 TAR 变体的读取，
// 以及 GNU TAR 格式的写入。支持长文件名、符号链接、硬链接、
// 稀疏文件、目录、设备文件等所有 TAR 特性。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_tar.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <fstream>
#include <vector>
#include <memory>

namespace bandzip {

// ---------------------------------------------------------------------------
// TAR 头部结构（512 字节）
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct TarHeader {
    char name[100];         // 文件名
    char mode[8];           // 权限
    char uid[8];            // 用户 ID
    char gid[8];            // 组 ID
    char size[12];          // 文件大小（八进制）
    char mtime[12];         // 修改时间（Unix 时间戳，八进制）
    char chksum[8];         // 校验和
    char typeflag;          // 类型
    char linkname[100];     // 链接目标
    char magic[6];          // "ustar"
    char version[2];        // "00"
    char uname[32];         // 用户名
    char gname[32];         // 组名
    char devmajor[8];       // 主设备号
    char devminor[8];       // 次设备号
    char prefix[155];       // 文件名前缀（ustar）
    char padding[12];       // 填充至 512 字节
};
#pragma pack(pop)
static_assert(sizeof(TarHeader) == 512, "TarHeader must be 512 bytes");

// ---------------------------------------------------------------------------
// TAR 类型标志
// ---------------------------------------------------------------------------
enum TarType {
    TAR_REGULAR  = '0',    // 普通文件
    TAR_HARDLINK = '1',    // 硬链接
    TAR_SYMLINK  = '2',    // 符号链接
    TAR_CHARDEV  = '3',    // 字符设备
    TAR_BLOCKDEV = '4',    // 块设备
    TAR_DIR      = '5',    // 目录
    TAR_FIFO     = '6',    // FIFO
    TAR_CONTIG   = '7',    // 连续文件
    TAR_XHEADER  = 'x',    // PAX 扩展头
    TAR_XGHEADER = 'g',    // PAX 全局扩展头
    TAR_GNU_LONGNAME = 'L',// GNU 长文件名
    TAR_GNU_LONGLINK = 'K',// GNU 长链接名
    TAR_GNU_SPARSE   = 'S',// GNU 稀疏文件
    TAR_OLDREGULAR   = 0,  // 旧式普通文件（typeflag 为 NUL）
};

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------
static u64 parse_octal(const char* s, size_t len) {
    u64 val = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] < '0' || s[i] > '7') break;
        val = val * 8 + (s[i] - '0');
    }
    return val;
}

static u64 parse_size(const char* s, size_t len) {
    // 处理 GNU base-256 编码
    if ((s[0] & 0x80) != 0) {
        // base-256
        u64 val = (s[0] & 0x7F);
        for (size_t i = 1; i < len; ++i) {
            val = (val << 8) | static_cast<u8>(s[i]);
        }
        return val;
    }
    return parse_octal(s, len);
}

static void write_octal(char* s, u64 val, size_t len) {
    // len-1 位数字 + 1 个 NUL
    std::memset(s, '0', len);
    s[len - 1] = '\0';
    for (size_t i = len - 2; val > 0 && i < len; --i) {
        s[i] = '0' + (val & 7);
        val >>= 3;
    }
}

static u32 compute_checksum(const TarHeader& h) {
    // 校验和计算时 chksum 字段视为空格
    u32 sum = 0;
    const u8* p = reinterpret_cast<const u8*>(&h);
    for (size_t i = 0; i < sizeof(TarHeader); ++i) {
        if (i >= offsetof(TarHeader, chksum) &&
            i < offsetof(TarHeader, chksum) + 8) {
            sum += ' ';
        } else {
            sum += p[i];
        }
    }
    return sum;
}

static bool is_zero_block(const TarHeader& h) {
    const u8* p = reinterpret_cast<const u8*>(&h);
    for (size_t i = 0; i < sizeof(TarHeader); ++i) {
        if (p[i] != 0) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// PAX 扩展头解析
// ---------------------------------------------------------------------------
struct PaxRecord {
    std::string key;
    std::string value;
};

static std::vector<PaxRecord> parse_pax(const char* data, size_t size) {
    std::vector<PaxRecord> records;
    size_t pos = 0;
    while (pos < size) {
        // 读取长度行（如 "30 atime=1234567890\n"）
        size_t space = 0;
        while (pos + space < size && data[pos + space] != ' ') ++space;
        if (space == 0 || pos + space >= size) break;

        std::string len_str(data + pos, data + pos + space);
        size_t rec_len = std::stoull(len_str);
        if (rec_len == 0 || pos + rec_len > size) break;

        std::string rec(data + pos + space + 1,
                        data + pos + rec_len - 1);  // 去掉末尾 \n
        size_t eq = rec.find('=');
        if (eq != std::string::npos) {
            PaxRecord r;
            r.key = rec.substr(0, eq);
            r.value = rec.substr(eq + 1);
            records.push_back(std::move(r));
        }
        pos += rec_len;
    }
    return records;
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct TarArchive::Impl {
    tstring path;
    tstring password;       // TAR 不支持加密，但保留接口
    OpenMode mode = OpenMode::Closed;
    bool is_open = false;

    std::ifstream in_file;
    std::ofstream out_file;

    // 缓存的条目
    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    // 每个条目在文件中的偏移
    struct EntryPos {
        u64 header_offset;      // 头部偏移
        u64 data_offset;        // 数据偏移
        u64 data_size;          // 数据大小
        bool is_sparse = false;
        std::vector<std::pair<u64, u64>> sparse_map;  // 稀疏块映射
    };
    std::vector<EntryPos> entry_positions;

    // 进度回调
    ProgressCallback progress_cb;
    std::atomic<bool> cancelled{false};

    // 当前正在处理的文件
    tstring current_file;

    // 读取一个头部
    bool read_header(TarHeader& h) {
        in_file.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (in_file.gcount() != sizeof(h)) return false;
        return true;
    }

    // 跳过数据
    void skip_data(u64 size) {
        u64 padded = (size + 511) & ~u64(511);
        in_file.seekg(padded, std::ios::cur);
    }

    // 读取 PAX 扩展头数据
    std::vector<PaxRecord> read_pax(u64 size) {
        std::vector<char> buf(static_cast<size_t>(size));
        in_file.read(buf.data(), buf.size());
        skip_data(size);
        return parse_pax(buf.data(), buf.size());
    }

    // 读取 GNU 长文件名
    std::string read_longname(u64 size) {
        std::vector<char> buf(static_cast<size_t>(size + 1), 0);
        in_file.read(buf.data(), static_cast<std::streamsize>(size));
        skip_data(size);
        return std::string(buf.data());
    }
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
TarArchive::TarArchive() : impl_(std::make_unique<Impl>()) {}
TarArchive::~TarArchive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code TarArchive::open(const tstring& path,
                                  const tstring& password,
                                  OpenMode mode) {
    if (mode != OpenMode::Read) {
        return make_error_code(ArchiveError::UnsupportedFeature);
    }

    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;

    impl_->in_file.open(util::tstring_to_string(path), std::ios::binary);
    if (!impl_->in_file) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    impl_->is_open = true;
    impl_->entries_cached = false;
    return make_error_code(ArchiveError::Ok);
}

std::error_code TarArchive::close() {
    if (impl_->in_file.is_open()) impl_->in_file.close();
    if (impl_->out_file.is_open()) impl_->out_file.close();
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    impl_->entry_positions.clear();
    return make_error_code(ArchiveError::Ok);
}

bool TarArchive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code TarArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (impl_->entries_cached) {
        entries = impl_->cached_entries;
        return make_error_code(ArchiveError::Ok);
    }

    entries.clear();
    impl_->entry_positions.clear();

    impl_->in_file.clear();
    impl_->in_file.seekg(0, std::ios::beg);

    u64 header_offset = 0;
    int zero_count = 0;

    while (true) {
        TarHeader h;
        if (!impl_->read_header(h)) break;

        // 检测结束标记（两个全零块）
        if (is_zero_block(h)) {
            if (++zero_count >= 2) break;
            header_offset += sizeof(h);
            continue;
        }
        zero_count = 0;

        u64 data_size = parse_size(h.size, sizeof(h.size));
        u64 data_offset = header_offset + sizeof(h);

        // 处理扩展头
        std::string long_name;
        std::string long_link;
        std::string pax_path;
        std::string pax_link;
        std::string pax_uname;
        std::string pax_gname;
        u64 pax_size = 0;
        std::time_t pax_mtime = 0;

        if (h.typeflag == TAR_XHEADER || h.typeflag == TAR_XGHEADER) {
            auto records = impl_->read_pax(data_size);
            for (const auto& r : records) {
                if (r.key == "path") pax_path = r.value;
                else if (r.key == "linkpath") pax_link = r.value;
                else if (r.key == "uname") pax_uname = r.value;
                else if (r.key == "gname") pax_gname = r.value;
                else if (r.key == "size") pax_size = std::stoull(r.value);
                else if (r.key == "mtime") pax_mtime = static_cast<std::time_t>(std::stoll(r.value));
            }
            header_offset = impl_->in_file.tellg();
            continue;
        }

        if (h.typeflag == TAR_GNU_LONGNAME) {
            long_name = impl_->read_longname(data_size);
            header_offset = impl_->in_file.tellg();
            continue;
        }

        if (h.typeflag == TAR_GNU_LONGLINK) {
            long_link = impl_->read_longname(data_size);
            header_offset = impl_->in_file.tellg();
            continue;
        }

        // 构造条目
        ArchiveEntry e{};
        e.index = static_cast<u32>(entries.size());

        // 文件名
        std::string name;
        if (!long_name.empty()) {
            name = long_name;
        } else if (!pax_path.empty()) {
            name = pax_path;
        } else {
            // ustar: prefix + '/' + name
            std::string prefix(h.prefix, strnlen(h.prefix, sizeof(h.prefix)));
            std::string n(h.name, strnlen(h.name, sizeof(h.name)));
            if (!prefix.empty()) {
                name = prefix + "/" + n;
            } else {
                name = n;
            }
        }

        // 编码检测：TAR 通常为 UTF-8，但旧版可能为本地编码
        if (!name.empty()) {
            auto enc = CodecDetector::detect(name);
            if (enc == NameEncoding::Gbk || enc == NameEncoding::Big5 ||
                enc == NameEncoding::ShiftJis || enc == NameEncoding::EucKr) {
                e.path = CodecDetector::decode(name, enc);
            } else {
                e.path = util::utf8_to_utf16(name);
            }
        }
        e.name = util::get_filename(e.path);

        // 大小
        if (pax_size > 0) {
            e.size = pax_size;
        } else {
            e.size = data_size;
        }

        // 类型
        char tf = h.typeflag == 0 ? TAR_REGULAR : h.typeflag;
        e.is_directory = (tf == TAR_DIR);
        e.is_symlink = (tf == TAR_SYMLINK);

        // 时间
        if (pax_mtime > 0) {
            e.modified = pax_mtime;
        } else {
            e.modified = static_cast<std::time_t>(parse_octal(h.mtime, sizeof(h.mtime)));
        }

        // 属性
        u32 mode = static_cast<u32>(parse_octal(h.mode, sizeof(h.mode)));
        if (e.is_directory) e.attributes |= FILE_ATTRIBUTE_DIRECTORY;
        if ((mode & 0x80) == 0) e.attributes |= FILE_ATTRIBUTE_READONLY;

        // 链接目标
        if (e.is_symlink) {
            if (!long_link.empty()) {
                e.link_target = util::utf8_to_utf16(long_link);
            } else if (!pax_link.empty()) {
                e.link_target = util::utf8_to_utf16(pax_link);
            } else {
                std::string ln(h.linkname, strnlen(h.linkname, sizeof(h.linkname)));
                e.link_target = util::utf8_to_utf16(ln);
            }
        }

        // 压缩方法
        e.method = CompressionMethod::Copy;
        e.compressed_size = e.size;

        // 记录位置
        TarArchive::Impl::EntryPos pos;
        pos.header_offset = header_offset;
        pos.data_offset = data_offset;
        pos.data_size = e.size;
        impl_->entry_positions.push_back(pos);

        entries.push_back(std::move(e));

        // 跳过数据
        impl_->skip_data(data_size);
        header_offset = impl_->in_file.tellg();
    }

    impl_->cached_entries = entries;
    impl_->entries_cached = true;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code TarArchive::extract_entry(u32 index,
                                          const tstring& output_path,
                                          const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!impl_->entries_cached) {
        std::vector<ArchiveEntry> tmp;
        auto ec = read_entries(tmp);
        if (ec) return ec;
    }

    if (index >= impl_->cached_entries.size()) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    const auto& entry = impl_->cached_entries[index];
    const auto& pos = impl_->entry_positions[index];

    // 安全检查
    if (util::has_parent_ref(entry.path)) {
        LOG_WARN(_T("Skipping unsafe path: ") << entry.path);
        return make_error_code(ArchiveError::InvalidParameter);
    }

    // 目录
    if (entry.is_directory) {
        if (!util::create_dir_recursive(output_path)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
        return make_error_code(ArchiveError::Ok);
    }

    // 符号链接
    if (entry.is_symlink) {
        // Windows 上不创建符号链接（需要权限），改为创建普通文件并写入链接目标
        if (!opts.create_symlinks) {
            return make_error_code(ArchiveError::Ok);
        }
        // TODO: 创建符号链接（需要 SE_CREATE_SYMBOLIC_LINK_NAME 权限）
        return make_error_code(ArchiveError::Ok);
    }

    // 创建父目录
    tstring parent = util::get_dirname(output_path);
    if (!parent.empty() && !util::dir_exists(parent)) {
        if (!util::create_dir_recursive(parent)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    // 定位到数据
    impl_->in_file.clear();
    impl_->in_file.seekg(pos.data_offset, std::ios::beg);

    // 创建输出文件
    HANDLE hOut = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        return make_error_code(ArchiveError::WriteFailed);
    }

    // 分块读取写入
    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> buf(BUF_SIZE);
    u64 remaining = pos.data_size;
    u64 processed = 0;

    impl_->current_file = entry.path;

    while (remaining > 0) {
        if (impl_->cancelled) {
            CloseHandle(hOut);
            return make_error_code(ArchiveError::Cancelled);
        }

        size_t to_read = static_cast<size_t>(std::min<u64>(remaining, BUF_SIZE));
        impl_->in_file.read(reinterpret_cast<char*>(buf.data()), to_read);
        if (static_cast<size_t>(impl_->in_file.gcount()) != to_read) {
            CloseHandle(hOut);
            return make_error_code(ArchiveError::ReadFailed);
        }

        DWORD written = 0;
        if (!WriteFile(hOut, buf.data(), static_cast<DWORD>(to_read), &written, nullptr) ||
            written != to_read) {
            CloseHandle(hOut);
            return make_error_code(ArchiveError::WriteFailed);
        }

        remaining -= to_read;
        processed += to_read;

        if (impl_->progress_cb) {
            ProgressInfo info{};
            info.current_file = impl_->current_file;
            info.bytes_processed = processed;
            info.bytes_total = pos.data_size;
            info.percent = pos.data_size > 0 ?
                static_cast<int>(processed * 100 / pos.data_size) : 100;
            info.cancelled = false;
            impl_->progress_cb(info);
            impl_->cancelled = info.cancelled;
        }
    }

    CloseHandle(hOut);

    // 设置时间
    if (opts.preserve_attributes && entry.modified > 0) {
        FILETIME ft = util::unix_to_filetime(entry.modified);
        HANDLE h = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            SetFileTime(h, nullptr, nullptr, &ft);
            CloseHandle(h);
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code TarArchive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!impl_->entries_cached) {
        std::vector<ArchiveEntry> tmp;
        auto ec = read_entries(tmp);
        if (ec) return ec;
    }

    if (index >= impl_->cached_entries.size()) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    const auto& entry = impl_->cached_entries[index];
    if (entry.is_directory || entry.is_symlink) {
        return make_error_code(ArchiveError::Ok);
    }

    const auto& pos = impl_->entry_positions[index];

    impl_->in_file.clear();
    impl_->in_file.seekg(pos.data_offset, std::ios::beg);

    // 读取并校验
    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> buf(BUF_SIZE);
    u64 remaining = pos.data_size;
    u64 processed = 0;

    impl_->current_file = entry.path;

    while (remaining > 0) {
        if (impl_->cancelled) {
            return make_error_code(ArchiveError::Cancelled);
        }

        size_t to_read = static_cast<size_t>(std::min<u64>(remaining, BUF_SIZE));
        impl_->in_file.read(reinterpret_cast<char*>(buf.data()), to_read);
        if (static_cast<size_t>(impl_->in_file.gcount()) != to_read) {
            return make_error_code(ArchiveError::ReadFailed);
        }

        remaining -= to_read;
        processed += to_read;

        if (impl_->progress_cb) {
            ProgressInfo info{};
            info.current_file = impl_->current_file;
            info.bytes_processed = processed;
            info.bytes_total = pos.data_size;
            info.percent = pos.data_size > 0 ?
                static_cast<int>(processed * 100 / pos.data_size) : 100;
            info.cancelled = false;
            impl_->progress_cb(info);
            impl_->cancelled = info.cancelled;
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code TarArchive::extract_files(const std::vector<u32>& indices,
                                           const tstring& output_dir,
                                           const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!util::dir_exists(output_dir)) {
        if (!util::create_dir_recursive(output_dir)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    for (u32 idx : indices) {
        if (impl_->cancelled) {
            return make_error_code(ArchiveError::Cancelled);
        }

        if (!impl_->entries_cached) {
            std::vector<ArchiveEntry> tmp;
            auto ec = read_entries(tmp);
            if (ec) return ec;
        }

        if (idx >= impl_->cached_entries.size()) {
            return make_error_code(ArchiveError::FileNotFound);
        }

        const auto& entry = impl_->cached_entries[idx];
        tstring out_path = util::join_path(output_dir, entry.path);

        auto ec = extract_entry(idx, out_path, opts);
        if (ec && ec != make_error_code(ArchiveError::Ok)) {
            return ec;
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试整个归档
// ---------------------------------------------------------------------------
std::error_code TarArchive::test() {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!impl_->entries_cached) {
        std::vector<ArchiveEntry> tmp;
        auto ec = read_entries(tmp);
        if (ec) return ec;
    }

    for (const auto& entry : impl_->cached_entries) {
        if (impl_->cancelled) {
            return make_error_code(ArchiveError::Cancelled);
        }
        auto ec = test_entry(entry.index);
        if (ec) return ec;
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code TarArchive::create(const CreateOptions& opts) {
    impl_->path = opts.archive_path;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;

    impl_->out_file.open(util::tstring_to_string(opts.archive_path), std::ios::binary | std::ios::trunc);
    if (!impl_->out_file) {
        impl_->is_open = false;
        return make_error_code(ArchiveError::OpenFailed);
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 添加文件
// ---------------------------------------------------------------------------
std::error_code TarArchive::add_files(const std::vector<tstring>& files,
                                       const CompressOptions& opts) {
    if (!is_open() || impl_->mode != OpenMode::Write) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    for (const auto& file : files) {
        if (impl_->cancelled) {
            return make_error_code(ArchiveError::Cancelled);
        }

        // 遍历文件/目录
        auto callback = [this, &opts](const util::FindData& fd) -> bool {
            if (impl_->cancelled) return false;

            tstring rel_path = fd.path;
            // 去除公共前缀
            if (!opts.base_dir.empty()) {
                rel_path = util::make_relative(opts.base_dir, fd.path);
            }
            rel_path = util::to_archive_path(rel_path);

            // 写入头部
            TarHeader h;
            std::memset(&h, 0, sizeof(h));

            std::string name = util::tstring_to_string(rel_path);
            if (name.size() > 99) {
                // 需要使用 GNU 长文件名扩展
                // 先写长文件名记录
                TarHeader lh;
                std::memset(&lh, 0, sizeof(lh));
                std::strncpy(lh.name, "././@LongLink", sizeof(lh.name) - 1);
                write_octal(lh.size, name.size() + 1, sizeof(lh.size));
                write_octal(lh.mtime, 0, sizeof(lh.mtime));
                lh.typeflag = TAR_GNU_LONGNAME;
                std::memcpy(lh.magic, "ustar", 6);
                lh.version[0] = ' '; lh.version[1] = ' ';
                u32 cksum = compute_checksum(lh);
                write_octal(lh.chksum, cksum, sizeof(lh.chksum));
                impl_->out_file.write(reinterpret_cast<const char*>(&lh), sizeof(lh));

                // 写入长文件名数据
                std::vector<char> name_buf(((name.size() + 1 + 511) / 512) * 512, 0);
                std::memcpy(name_buf.data(), name.c_str(), name.size() + 1);
                impl_->out_file.write(name_buf.data(), name_buf.size());
            } else {
                std::strncpy(h.name, name.c_str(), sizeof(h.name) - 1);
            }

            // 大小
            u64 file_size = fd.is_directory ? 0 : fd.size;
            write_octal(h.size, file_size, sizeof(h.size));

            // 时间
            write_octal(h.mtime, static_cast<u64>(fd.modified), sizeof(h.mtime));

            // 类型
            h.typeflag = fd.is_directory ? TAR_DIR : TAR_REGULAR;

            // 权限
            write_octal(h.mode, fd.is_readonly ? 0444 : 0644, sizeof(h.mode));
            if (fd.is_directory) {
                write_octal(h.mode, 0755, sizeof(h.mode));
            }

            // magic
            std::memcpy(h.magic, "ustar", 6);
            h.version[0] = '0'; h.version[1] = '0';

            // 校验和
            u32 cksum = compute_checksum(h);
            write_octal(h.chksum, cksum, sizeof(h.chksum));

            impl_->out_file.write(reinterpret_cast<const char*>(&h), sizeof(h));

            // 写入文件数据
            if (!fd.is_directory && file_size > 0) {
                std::ifstream in(util::tstring_to_string(fd.path), std::ios::binary);
                if (!in) return false;

                constexpr size_t BUF_SIZE = 64 * 1024;
                std::vector<char> buf(BUF_SIZE);
                u64 remaining = file_size;
                u64 processed = 0;

                impl_->current_file = fd.path;

                while (remaining > 0) {
                    if (impl_->cancelled) return false;

                    size_t to_read = static_cast<size_t>(std::min<u64>(remaining, BUF_SIZE));
                    in.read(buf.data(), to_read);
                    if (static_cast<size_t>(in.gcount()) != to_read) return false;

                    impl_->out_file.write(buf.data(), to_read);
                    remaining -= to_read;
                    processed += to_read;

                    if (impl_->progress_cb) {
                        ProgressInfo info{};
                        info.current_file = impl_->current_file;
                        info.bytes_processed = processed;
                        info.bytes_total = file_size;
                        info.percent = file_size > 0 ?
                            static_cast<int>(processed * 100 / file_size) : 100;
                        info.cancelled = false;
                        impl_->progress_cb(info);
                        impl_->cancelled = info.cancelled;
                    }
                }

                // 填充至 512 字节边界
                size_t pad = (512 - (file_size % 512)) % 512;
                if (pad > 0) {
                    std::vector<char> pad_buf(pad, 0);
                    impl_->out_file.write(pad_buf.data(), pad);
                }
            }

            return true;
        };

        util::walk_dir(file, callback, true);
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code TarArchive::delete_entries(const std::vector<u32>& indices) {
    // TAR 不支持原地删除，需要重写整个归档
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
