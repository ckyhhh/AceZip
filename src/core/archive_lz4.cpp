// ============================================================================
// archive_lz4.cpp - LZ4 单文件压缩驱动完整实现
//
// 基于 LZ4 库实现 LZ4 帧格式（LZ4 Frame）的读写。
// LZ4 Frame magic: 04 22 4D 18（4 字节小端）
// 支持 LZ4 Frame 格式（含 magic、内容大小、校验和）。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_lz4.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <lz4frame.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

namespace bandzip {

// ---------------------------------------------------------------------------
// LZ4 Frame magic
// ---------------------------------------------------------------------------
static const u32 LZ4_MAGIC = 0x184D2204;

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct Lz4Archive::Impl {
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
        if (ext == _T(".lz4")) {
            return base.substr(0, base.length() - ext.length());
        }
        return base + _T(".out");
    }
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
Lz4Archive::Lz4Archive() : impl_(std::make_unique<Impl>()) {}
Lz4Archive::~Lz4Archive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::open(const tstring& path,
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

    // 读取 magic
    u32 magic;
    in.read(reinterpret_cast<char*>(&magic), 4);
    if (in.gcount() != 4) {
        return make_error_code(ArchiveError::BadFormat);
    }
    if (magic != LZ4_MAGIC) {
        return make_error_code(ArchiveError::BadFormat);
    }

    // 读取 frame descriptor
    u8 flg;
    in.read(reinterpret_cast<char*>(&flg), 1);
    if (in.gcount() != 1) {
        return make_error_code(ArchiveError::BadFormat);
    }

    bool has_content_size = (flg & 0x08) != 0;
    u64 content_size = 0;

    u8 bd;
    in.read(reinterpret_cast<char*>(&bd), 1);
    if (in.gcount() != 1) {
        return make_error_code(ArchiveError::BadFormat);
    }

    // 内容大小（8 字节，仅当 has_content_size 为真时存在）
    if (has_content_size) {
        in.read(reinterpret_cast<char*>(&content_size), 8);
        if (in.gcount() != 8) {
            return make_error_code(ArchiveError::BadFormat);
        }
    }

    // HCRC（2 字节，仅当 FLG 中 has_content_size 为真时存在）
    if (flg & 0x04) {
        u16 hcrc;
        in.read(reinterpret_cast<char*>(&hcrc), 2);
        if (in.gcount() != 2) {
            return make_error_code(ArchiveError::BadFormat);
        }
    }

    impl_->cached_entries.clear();

    ArchiveEntry e{};
    e.index = 0;
    e.path = impl_->get_output_name();
    e.name = util::get_filename(e.path);
    e.size = content_size;
    e.modified = 0;
    e.is_directory = false;
    e.method = CompressionMethod::Lz4;
    e.compressed_size = util::file_size(path);
    impl_->cached_entries.push_back(e);
    impl_->entries_cached = true;

    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

std::error_code Lz4Archive::close() {
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    return make_error_code(ArchiveError::Ok);
}

bool Lz4Archive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    entries = impl_->cached_entries;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::extract_entry(u32 index,
                                            const tstring& output_path,
                                            const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index != 0) return make_error_code(ArchiveError::FileNotFound);

    const auto& entry = impl_->cached_entries[0];

    tstring parent = util::get_dirname(output_path);
    if (!parent.empty() && !util::dir_exists(parent)) {
        if (!util::create_dir_recursive(parent)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    std::ifstream in(util::tstring_to_string(impl_->path), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    HANDLE hOut = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        return make_error_code(ArchiveError::WriteFailed);
    }

    // 创建解压上下文
    LZ4F_decompressionContext_t dctx;
    size_t ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(ret)) {
        CloseHandle(hOut);
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u64 total_out = 0;

    impl_->current_file = entry.path;

    while (true) {
        if (impl_->cancelled) {
            LZ4F_freeDecompressionContext(dctx);
            CloseHandle(hOut);
            return make_error_code(ArchiveError::Cancelled);
        }

        in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
        size_t read = static_cast<size_t>(in.gcount());
        if (read == 0) break;

        size_t in_pos = 0;
        while (in_pos < read) {
            size_t out_pos = 0;
            size_t dst_size = BUF_SIZE;
            size_t src_size = read - in_pos;
            ret = LZ4F_decompress(dctx,
                                   out_buf.data(), &dst_size,
                                   in_buf.data() + in_pos, &src_size,
                                   nullptr);
            if (LZ4F_isError(ret)) {
                LZ4F_freeDecompressionContext(dctx);
                CloseHandle(hOut);
                return make_error_code(ArchiveError::BadFormat);
            }

            in_pos += src_size;
            if (dst_size > 0) {
                DWORD written = 0;
                if (!WriteFile(hOut, out_buf.data(),
                              static_cast<DWORD>(dst_size),
                              &written, nullptr) ||
                    written != dst_size) {
                    LZ4F_freeDecompressionContext(dctx);
                    CloseHandle(hOut);
                    return make_error_code(ArchiveError::WriteFailed);
                }
                total_out += dst_size;
            }

            if (ret == 0) break;  // 解压完成
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
    }

    LZ4F_freeDecompressionContext(dctx);
    CloseHandle(hOut);

    impl_->cached_entries[0].size = total_out;

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index != 0) return make_error_code(ArchiveError::FileNotFound);

    std::ifstream in(util::tstring_to_string(impl_->path), std::ios::binary);
    if (!in) return make_error_code(ArchiveError::OpenFailed);

    LZ4F_decompressionContext_t dctx;
    size_t ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(ret)) {
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> in_buf(BUF_SIZE);
    std::vector<u8> out_buf(BUF_SIZE);
    u64 total_out = 0;

    while (true) {
        if (impl_->cancelled) {
            LZ4F_freeDecompressionContext(dctx);
            return make_error_code(ArchiveError::Cancelled);
        }

        in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
        size_t read = static_cast<size_t>(in.gcount());
        if (read == 0) break;

        size_t in_pos = 0;
        while (in_pos < read) {
            size_t dst_size = BUF_SIZE;
            size_t src_size = read - in_pos;
            ret = LZ4F_decompress(dctx,
                                   out_buf.data(), &dst_size,
                                   in_buf.data() + in_pos, &src_size,
                                   nullptr);
            if (LZ4F_isError(ret)) {
                LZ4F_freeDecompressionContext(dctx);
                return make_error_code(ArchiveError::BadFormat);
            }
            in_pos += src_size;
            total_out += dst_size;
            if (ret == 0) break;
        }

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
    }

    LZ4F_freeDecompressionContext(dctx);
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::extract_files(const std::vector<u32>& indices,
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
std::error_code Lz4Archive::test() {
    return test_entry(0);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::create(const CreateOptions& opts) {
    impl_->path = opts.archive_path;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 添加文件
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::add_files(const std::vector<tstring>& files,
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

    // 创建压缩上下文
    LZ4F_compressionContext_t cctx;
    size_t ret = LZ4F_createCompressionContext(&cctx, LZ4F_VERSION);
    if (LZ4F_isError(ret)) {
        return make_error_code(ArchiveError::InternalError);
    }

    // 压缩偏好
    LZ4F_preferences_t prefs = {};
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.blockMode = LZ4F_blockIndependent;
    prefs.frameInfo.contentChecksumFlag = LZ4F_noContentChecksum;
    prefs.frameInfo.contentSize = util::file_size(input);
    prefs.compressionLevel = opts.level;

    // 写入 frame header
    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<u8> out_buf(BUF_SIZE);
    ret = LZ4F_compressBegin(cctx, out_buf.data(), out_buf.size(), &prefs);
    if (LZ4F_isError(ret)) {
        LZ4F_freeCompressionContext(cctx);
        return make_error_code(ArchiveError::InternalError);
    }
    out.write(reinterpret_cast<const char*>(out_buf.data()), ret);

    std::vector<u8> in_buf(BUF_SIZE);
    u64 total_in = 0;
    u64 file_size = util::file_size(input);

    impl_->current_file = input;

    while (true) {
        if (impl_->cancelled) {
            LZ4F_freeCompressionContext(cctx);
            return make_error_code(ArchiveError::Cancelled);
        }

        in.read(reinterpret_cast<char*>(in_buf.data()), BUF_SIZE);
        size_t read = static_cast<size_t>(in.gcount());
        if (read == 0) break;

        total_in += read;

        ret = LZ4F_compressUpdate(cctx,
                                   out_buf.data(), out_buf.size(),
                                   in_buf.data(), read,
                                   nullptr);
        if (LZ4F_isError(ret)) {
            LZ4F_freeCompressionContext(cctx);
            return make_error_code(ArchiveError::InternalError);
        }
        if (ret > 0) {
            out.write(reinterpret_cast<const char*>(out_buf.data()), ret);
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
    }

    // 刷新
    ret = LZ4F_compressEnd(cctx, out_buf.data(), out_buf.size(), nullptr);
    if (LZ4F_isError(ret)) {
        LZ4F_freeCompressionContext(cctx);
        return make_error_code(ArchiveError::InternalError);
    }
    if (ret > 0) {
        out.write(reinterpret_cast<const char*>(out_buf.data()), ret);
    }

    LZ4F_freeCompressionContext(cctx);

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code Lz4Archive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
