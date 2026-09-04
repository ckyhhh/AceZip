// ============================================================================
// archive_xz.cpp - XZ 单文件压缩驱动完整实现
//
// 基于 liblzma 实现 XZ/LZMA 格式的读写。
// XZ 格式头部：FD 37 7A 58 5A 00（6 字节 magic）
// 支持 LZMA2 算法，可选 BCJ 过滤器。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_xz.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <lzma.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

namespace bandzip {

// ---------------------------------------------------------------------------
// XZ magic
// ---------------------------------------------------------------------------
static const u8 XZ_MAGIC[6] = { 0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00 };

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct XzArchive::Impl {
    tstring path;
    tstring password;
    OpenMode mode = OpenMode::Closed;
    bool is_open = false;

    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    ProgressCallback progress_cb;
    std::atomic<bool> cancelled{false};
    tstring current_file;

    tstring get_output_name() const {
        tstring base = util::get_basename(path);
        tstring ext = util::get_extension_lower(base);
        if (ext == _T(".xz")) {
            return base.substr(0, base.length() - ext.length());
        }
        if (ext == _T(".lzma")) {
            return base.substr(0, base.length() - ext.length());
        }
        return base + _T(".out");
    }
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
XzArchive::XzArchive() : impl_(std::make_unique<Impl>()) {}
XzArchive::~XzArchive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code XzArchive::open(const tstring& path,
                                  const tstring& password,
                                  OpenMode mode) {
    if (mode != OpenMode::Read) {
        return make_error_code(ArchiveError::UnsupportedFeature);
    }

    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;

    // 读取头部验证
    std::ifstream in(util::tstring_to_string(path), std::ios::binary);
    if (!in) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    u8 magic[6];
    in.read(reinterpret_cast<char*>(magic), 6);
    if (in.gcount() != 6) {
        return make_error_code(ArchiveError::BadFormat);
    }

    // 检查 XZ 或 LZMA magic
    bool is_xz = (memcmp(magic, XZ_MAGIC, 6) == 0);
    bool is_lzma = (magic[0] == 0x5D && magic[1] == 0x00 && magic[2] == 0x00);
    if (!is_xz && !is_lzma) {
        return make_error_code(ArchiveError::BadFormat);
    }

    impl_->cached_entries.clear();

    ArchiveEntry e{};
    e.index = 0;
    e.path = impl_->get_output_name();
    e.name = util::get_filename(e.path);
    e.size = 0;  // 未知
    e.modified = 0;
    e.is_directory = false;
    e.method = CompressionMethod::Lzma2;
    e.compressed_size = util::file_size(path);
    impl_->cached_entries.push_back(e);
    impl_->entries_cached = true;

    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

std::error_code XzArchive::close() {
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    return make_error_code(ArchiveError::Ok);
}

bool XzArchive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code XzArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    entries = impl_->cached_entries;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code XzArchive::extract_entry(u32 index,
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

    // 打开输出
    HANDLE hOut = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        return make_error_code(ArchiveError::WriteFailed);
    }

    // 初始化解码器
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
    if (ret != LZMA_OK) {
        CloseHandle(hOut);
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u64 total_out = 0;

    impl_->current_file = entry.path;

    lzma_action action = LZMA_RUN;

    while (true) {
        if (impl_->cancelled) {
            lzma_end(&strm);
            CloseHandle(hOut);
            return make_error_code(ArchiveError::Cancelled);
        }

        if (strm.avail_in == 0) {
            in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
            strm.avail_in = static_cast<size_t>(in.gcount());
            if (strm.avail_in == 0) {
                action = LZMA_FINISH;
            }
            strm.next_in = in_buf.data();
        }

        strm.next_out = out_buf.data();
        strm.avail_out = BUF_SIZE;

        ret = lzma_code(&strm, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            lzma_end(&strm);
            CloseHandle(hOut);
            return make_error_code(ArchiveError::BadFormat);
        }

        size_t have = BUF_SIZE - strm.avail_out;
        if (have > 0) {
            DWORD written = 0;
            if (!WriteFile(hOut, out_buf.data(), static_cast<DWORD>(have),
                          &written, nullptr) || written != have) {
                lzma_end(&strm);
                CloseHandle(hOut);
                return make_error_code(ArchiveError::WriteFailed);
            }
            total_out += have;
        }

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

        if (ret == LZMA_STREAM_END) break;
    }

    lzma_end(&strm);
    CloseHandle(hOut);

    impl_->cached_entries[0].size = total_out;

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code XzArchive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index != 0) return make_error_code(ArchiveError::FileNotFound);

    std::ifstream in(util::tstring_to_string(impl_->path), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
    if (ret != LZMA_OK) {
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u64 total_out = 0;

    lzma_action action = LZMA_RUN;

    while (true) {
        if (impl_->cancelled) {
            lzma_end(&strm);
            return make_error_code(ArchiveError::Cancelled);
        }

        if (strm.avail_in == 0) {
            in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
            strm.avail_in = static_cast<size_t>(in.gcount());
            if (strm.avail_in == 0) action = LZMA_FINISH;
            strm.next_in = in_buf.data();
        }

        strm.next_out = out_buf.data();
        strm.avail_out = BUF_SIZE;
        ret = lzma_code(&strm, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            lzma_end(&strm);
            return make_error_code(ArchiveError::BadFormat);
        }

        total_out += BUF_SIZE - strm.avail_out;

        if (impl_->progress_cb) {
            ProgressInfo info{};
            info.current_file = impl_->current_file;
            info.bytes_processed = total_out;
            info.bytes_total = impl_->cached_entries[0].size;
            info.percent = 0;
            info.cancelled = false;
            impl_->progress_cb(info);
            impl_->cancelled = info.cancelled;
        }

        if (ret == LZMA_STREAM_END) break;
    }

    lzma_end(&strm);
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code XzArchive::extract_files(const std::vector<u32>& indices,
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
std::error_code XzArchive::test() {
    return test_entry(0);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code XzArchive::create(const CreateOptions& opts) {
    impl_->path = opts.archive_path;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 添加文件
// ---------------------------------------------------------------------------
std::error_code XzArchive::add_files(const std::vector<tstring>& files,
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

    std::ifstream in(util::tstring_to_string(input), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    std::ofstream out(util::tstring_to_string(impl_->path),
                      std::ios::binary | std::ios::trunc);
    if (!out) return make_error_code(ArchiveError::WriteFailed);

    // 初始化编码器
    lzma_stream strm = LZMA_STREAM_INIT;

    int level = opts.level;
    if (level < 0 || level > 9) level = 6;

    // 使用 XZ 容器（默认）
    uint32_t preset = static_cast<uint32_t>(level);
    lzma_ret ret = lzma_easy_encoder(&strm, preset, LZMA_CHECK_CRC64);
    if (ret != LZMA_OK) {
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u64 total_in = 0;
    u64 file_size = util::file_size(input);

    impl_->current_file = input;

    lzma_action action = LZMA_RUN;

    while (true) {
        if (impl_->cancelled) {
            lzma_end(&strm);
            return make_error_code(ArchiveError::Cancelled);
        }

        if (strm.avail_in == 0) {
            in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
            strm.avail_in = static_cast<size_t>(in.gcount());
            if (strm.avail_in == 0) action = LZMA_FINISH;
            strm.next_in = in_buf.data();
            total_in += strm.avail_in;
        }

        strm.next_out = out_buf.data();
        strm.avail_out = BUF_SIZE;

        ret = lzma_code(&strm, action);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            lzma_end(&strm);
            return make_error_code(ArchiveError::InternalError);
        }

        size_t have = BUF_SIZE - strm.avail_out;
        if (have > 0) {
            out.write(reinterpret_cast<const char*>(out_buf.data()), have);
        }

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

        if (ret == LZMA_STREAM_END) break;
    }

    lzma_end(&strm);

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code XzArchive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
