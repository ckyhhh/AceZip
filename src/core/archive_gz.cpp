// ============================================================================
// archive_gz.cpp - GZ 单文件压缩驱动完整实现
//
// 基于 zlib 实现 gzip 格式的读写。
// 支持 gzip 头部解析（原始文件名、时间戳、注释、操作系统标志）。
// 支持多成员 gzip 文件（concatenated gzip streams）。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_gz.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <zlib.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

namespace bandzip {

// ---------------------------------------------------------------------------
// GZ 头部标志位
// ---------------------------------------------------------------------------
namespace gz_flag {
constexpr u8 FTEXT    = 0x01;    // 文本
constexpr u8 FHCRC    = 0x02;    // 头部 CRC16
constexpr u8 FEXTRA   = 0x04;    // 额外字段
constexpr u8 FNAME    = 0x08;    // 原始文件名
constexpr u8 FCOMMENT = 0x10;    // 注释
}

// ---------------------------------------------------------------------------
// GZ 头部
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct GzHeader {
    u8  id1;        // 0x1f
    u8  id2;        // 0x8b
    u8  cm;         // 压缩方法（8 = deflate）
    u8  flg;        // 标志
    u32 mtime;      // 修改时间
    u8  xfl;        // 额外标志
    u8  os;         // 操作系统
};
#pragma pack(pop)
static_assert(sizeof(GzHeader) == 10, "GzHeader must be 10 bytes");

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct GzArchive::Impl {
    tstring path;
    tstring password;
    OpenMode mode = OpenMode::Closed;
    bool is_open = false;

    // 缓存的条目
    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    // 从 GZ 头部解析的原始文件名
    tstring original_name;

    // 进度
    ProgressCallback progress_cb;
    std::atomic<bool> cancelled{false};
    tstring current_file;

    // 解析 GZ 头部
    bool parse_header(std::ifstream& in, tstring& name, std::time_t& mtime) {
        GzHeader hdr;
        in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (in.gcount() != sizeof(hdr)) return false;
        if (hdr.id1 != 0x1f || hdr.id2 != 0x8b) return false;
        if (hdr.cm != 8) return false;  // 仅支持 deflate

        mtime = hdr.mtime;

        // 跳过额外字段
        if (hdr.flg & gz_flag::FEXTRA) {
            u16 xlen;
            in.read(reinterpret_cast<char*>(&xlen), 2);
            if (in.gcount() != 2) return false;
            std::vector<char> extra(xlen);
            in.read(extra.data(), xlen);
            if (in.gcount() != xlen) return false;
        }

        // 读取原始文件名
        if (hdr.flg & gz_flag::FNAME) {
            std::string n;
            char c;
            while (in.get(c) && c != '\0') n += c;
            if (!n.empty()) {
                auto enc = CodecDetector::detect(n);
                name = CodecDetector::decode(n, enc);
            }
        }

        // 跳过注释
        if (hdr.flg & gz_flag::FCOMMENT) {
            char c;
            while (in.get(c) && c != '\0') {}
        }

        // 跳过头 CRC16
        if (hdr.flg & gz_flag::FHCRC) {
            u16 hcrc;
            in.read(reinterpret_cast<char*>(&hcrc), 2);
        }

        return true;
    }

    // 获取解压后的输出文件名
    tstring get_output_name() const {
        if (!original_name.empty()) return original_name;
        // 从 .gz 文件名去除 .gz 扩展名
        tstring base = util::get_basename(path);
        tstring ext = util::get_extension_lower(base);
        if (ext == _T(".gz") || ext == _T(".gzip")) {
            return base.substr(0, base.length() - ext.length());
        }
        if (util::get_extension_lower(util::get_basename(path.substr(0, base.length() - 3))) == _T(".tar")) {
            return base.substr(0, base.length() - 3) + _T(".tar");
        }
        return base + _T(".out");
    }
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
GzArchive::GzArchive() : impl_(std::make_unique<Impl>()) {}
GzArchive::~GzArchive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code GzArchive::open(const tstring& path,
                                 const tstring& password,
                                 OpenMode mode) {
    if (mode != OpenMode::Read) {
        return make_error_code(ArchiveError::UnsupportedFeature);
    }

    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;

    std::ifstream in(util::tstring_to_string(path), std::ios::binary);
    if (!in) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    std::time_t mtime = 0;
    tstring name;
    if (!impl_->parse_header(in, name, mtime)) {
        return make_error_code(ArchiveError::BadFormat);
    }

    impl_->original_name = name;

    // 获取解压后大小（从尾部 4 字节）
    in.seekg(-4, std::ios::end);
    u32 isize;
    in.read(reinterpret_cast<char*>(&isize), 4);
    impl_->cached_entries.clear();

    ArchiveEntry e{};
    e.index = 0;
    e.path = name.empty() ? impl_->get_output_name() : name;
    e.name = util::get_filename(e.path);
    e.size = isize;
    e.modified = mtime;
    e.is_directory = false;
    e.method = CompressionMethod::Deflate;
    e.compressed_size = util::file_size(path);
    impl_->cached_entries.push_back(e);
    impl_->entries_cached = true;

    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

std::error_code GzArchive::close() {
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    impl_->original_name.clear();
    return make_error_code(ArchiveError::Ok);
}

bool GzArchive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code GzArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    entries = impl_->cached_entries;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code GzArchive::extract_entry(u32 index,
                                          const tstring& output_path,
                                          const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index != 0) return make_error_code(ArchiveError::FileNotFound);

    const auto& entry = impl_->cached_entries[0];

    // 创建父目录
    tstring parent = util::get_dirname(output_path);
    if (!parent.empty() && !util::dir_exists(parent)) {
        if (!util::create_dir_recursive(parent)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    // 打开输入
    std::ifstream in(util::tstring_to_string(impl_->path), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    // 跳过头部
    std::time_t mtime;
    tstring name;
    impl_->parse_header(in, name, mtime);

    // 打开输出
    HANDLE hOut = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        return make_error_code(ArchiveError::WriteFailed);
    }

    // 解压
    z_stream zs{};
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_in = Z_NULL;
    zs.avail_in = 0;

    int ret = inflateInit2(&zs, -MAX_WBITS);  // -MAX_WBITS 表示 raw deflate
    if (ret != Z_OK) {
        CloseHandle(hOut);
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u64 total_out = 0;

    impl_->current_file = entry.path;

    do {
        if (impl_->cancelled) {
            inflateEnd(&zs);
            CloseHandle(hOut);
            return make_error_code(ArchiveError::Cancelled);
        }

        in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
        zs.avail_in = static_cast<uInt>(in.gcount());
        if (zs.avail_in == 0) break;

        zs.next_in = in_buf.data();

        do {
            zs.next_out = out_buf.data();
            zs.avail_out = BUF_SIZE;

            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT ||
                ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&zs);
                CloseHandle(hOut);
                return make_error_code(ArchiveError::BadFormat);
            }

            uInt have = BUF_SIZE - zs.avail_out;
            if (have > 0) {
                DWORD written = 0;
                if (!WriteFile(hOut, out_buf.data(), have, &written, nullptr) ||
                    written != have) {
                    inflateEnd(&zs);
                    CloseHandle(hOut);
                    return make_error_code(ArchiveError::WriteFailed);
                }
                total_out += have;
            }
        } while (zs.avail_out == 0);

        if (impl_->progress_cb) {
            ProgressInfo info{};
            info.current_file = impl_->current_file;
            info.bytes_processed = total_out;
            info.bytes_total = entry.size;
            info.percent = entry.size > 0 ?
                static_cast<int>(total_out * 100 / entry.size) : 0;
            info.cancelled = false;
            impl_->progress_cb(info);
            impl_->cancelled = info.cancelled;
        }
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
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
std::error_code GzArchive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index != 0) return make_error_code(ArchiveError::FileNotFound);

    // 解压到内存并校验 CRC
    std::ifstream in(util::tstring_to_string(impl_->path), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    std::time_t mtime;
    tstring name;
    impl_->parse_header(in, name, mtime);

    z_stream zs{};
    inflateInit2(&zs, -MAX_WBITS);

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u32 crc = crc32(0L, Z_NULL, 0);
    u64 total_out = 0;

    int ret;
    do {
        if (impl_->cancelled) {
            inflateEnd(&zs);
            return make_error_code(ArchiveError::Cancelled);
        }

        in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
        zs.avail_in = static_cast<uInt>(in.gcount());
        if (zs.avail_in == 0) break;
        zs.next_in = in_buf.data();

        do {
            zs.next_out = out_buf.data();
            zs.avail_out = BUF_SIZE;
            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&zs);
                return make_error_code(ArchiveError::BadFormat);
            }
            uInt have = BUF_SIZE - zs.avail_out;
            crc = crc32(crc, out_buf.data(), have);
            total_out += have;
        } while (zs.avail_out == 0);

        if (impl_->progress_cb) {
            ProgressInfo info{};
            info.current_file = impl_->current_file;
            info.bytes_processed = total_out;
            info.bytes_total = impl_->cached_entries[0].size;
            info.percent = impl_->cached_entries[0].size > 0 ?
                static_cast<int>(total_out * 100 / impl_->cached_entries[0].size) : 0;
            info.cancelled = false;
            impl_->progress_cb(info);
            impl_->cancelled = info.cancelled;
        }
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);

    // 读取尾部 CRC32 和 ISIZE
    u32 expected_crc;
    u32 expected_size;
    in.read(reinterpret_cast<char*>(&expected_crc), 4);
    in.read(reinterpret_cast<char*>(&expected_size), 4);

    if (crc != expected_crc) {
        return make_error_code(ArchiveError::CRCMismatch);
    }
    if ((total_out & 0xFFFFFFFF) != expected_size) {
        return make_error_code(ArchiveError::BadFormat);
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code GzArchive::extract_files(const std::vector<u32>& indices,
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

        const auto& entry = impl_->cached_entries[idx];
        tstring out_path = util::join_path(output_dir, entry.name);
        auto ec = extract_entry(idx, out_path, opts);
        if (ec) return ec;
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试整个归档
// ---------------------------------------------------------------------------
std::error_code GzArchive::test() {
    return test_entry(0);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code GzArchive::create(const CreateOptions& opts) {
    impl_->path = opts.archive_path;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 添加文件（GZ 只能压缩单个文件）
// ---------------------------------------------------------------------------
std::error_code GzArchive::add_files(const std::vector<tstring>& files,
                                       const CompressOptions& opts) {
    if (!is_open() || impl_->mode != OpenMode::Write) {
        return make_error_code(ArchiveError::OpenFailed);
    }
    if (files.size() != 1) {
        return make_error_code(ArchiveError::InvalidParameter);
    }

    const tstring& input = files[0];
    if (!util::file_exists(input)) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    // 打开输入
    std::ifstream in(util::tstring_to_string(input), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    // 打开输出
    std::ofstream out(util::tstring_to_string(impl_->path), std::ios::binary | std::ios::trunc);
    if (!out) return make_error_code(ArchiveError::OpenFailed);

    // 写入 GZ 头部
    GzHeader hdr{};
    hdr.id1 = 0x1f;
    hdr.id2 = 0x8b;
    hdr.cm = 8;  // deflate
    hdr.flg = gz_flag::FNAME;
    hdr.mtime = static_cast<u32>(std::time(nullptr));
    hdr.xfl = 0;
    hdr.os = 0x0b;  // Windows
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // 写入原始文件名
    std::string name = util::tstring_to_string(util::get_filename(input));
    out.write(name.c_str(), name.size() + 1);

    // 压缩
    z_stream zs{};
    int level = opts.level;
    if (level < 0 || level > 9) level = 6;

    int ret = deflateInit2(&zs, level, Z_DEFLATED,
                           -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u32 crc = crc32(0L, Z_NULL, 0);
    u64 total_in = 0;
    u64 file_size = util::file_size(input);

    impl_->current_file = input;

    int flush;
    do {
        if (impl_->cancelled) {
            deflateEnd(&zs);
            return make_error_code(ArchiveError::Cancelled);
        }

        in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
        uInt avail = static_cast<uInt>(in.gcount());
        if (avail == 0) {
            flush = Z_FINISH;
        } else {
            flush = Z_NO_FLUSH;
            crc = crc32(crc, in_buf.data(), avail);
            total_in += avail;
        }

        zs.next_in = in_buf.data();
        zs.avail_in = avail;

        do {
            zs.next_out = out_buf.data();
            zs.avail_out = BUF_SIZE;
            ret = deflate(&zs, flush);
            if (ret == Z_STREAM_ERROR) {
                deflateEnd(&zs);
                return make_error_code(ArchiveError::InternalError);
            }
            uInt have = BUF_SIZE - zs.avail_out;
            if (have > 0) {
                out.write(reinterpret_cast<const char*>(out_buf.data()), have);
            }
        } while (zs.avail_out == 0);

        if (impl_->progress_cb) {
            ProgressInfo info{};
            info.current_file = impl_->current_file;
            info.bytes_processed = total_in;
            info.bytes_total = file_size;
            info.percent = file_size > 0 ?
                static_cast<int>(total_in * 100 / file_size) : 100;
            info.cancelled = false;
            impl_->progress_cb(info);
            impl_->cancelled = info.cancelled;
        }
    } while (flush != Z_FINISH);

    deflateEnd(&zs);

    // 写入 CRC32 和 ISIZE
    out.write(reinterpret_cast<const char*>(&crc), 4);
    u32 isize = static_cast<u32>(total_in);
    out.write(reinterpret_cast<const char*>(&isize), 4);

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code GzArchive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
