// ============================================================================
// archive_bz2.cpp - BZ2 单文件压缩驱动完整实现
//
// 基于 bzip2 库实现 BZ2 格式的读写。
// BZ2 格式头部：'B' 'Z' + 1 字节 block_size (1-9) + magic 'h' + magic 'r'
// 完整 magic: "BZh" + level_digit
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_bz2.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <bzlib.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

namespace bandzip {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct Bz2Archive::Impl {
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
        if (ext == _T(".bz2") || ext == _T(".bzip2")) {
            return base.substr(0, base.length() - ext.length());
        }
        return base + _T(".out");
    }
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
Bz2Archive::Bz2Archive() : impl_(std::make_unique<Impl>()) {}
Bz2Archive::~Bz2Archive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::open(const tstring& path,
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

    char magic[4];
    in.read(magic, 4);
    if (in.gcount() != 4 || magic[0] != 'B' || magic[1] != 'Z' ||
        magic[2] != 'h' || magic[3] < '1' || magic[3] > '9') {
        return make_error_code(ArchiveError::BadFormat);
    }

    // BZ2 不存储原始文件名，需要从 .bz2 文件名推断
    // 解压后大小也未知，需要解压后才能知道
    impl_->cached_entries.clear();

    ArchiveEntry e{};
    e.index = 0;
    e.path = impl_->get_output_name();
    e.name = util::get_filename(e.path);
    e.size = 0;  // 未知
    e.modified = 0;
    e.is_directory = false;
    e.method = CompressionMethod::Bzip2;
    e.compressed_size = util::file_size(path);
    impl_->cached_entries.push_back(e);
    impl_->entries_cached = true;

    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

std::error_code Bz2Archive::close() {
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    return make_error_code(ArchiveError::Ok);
}

bool Bz2Archive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    entries = impl_->cached_entries;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::extract_entry(u32 index,
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
    FILE* fin = _wfopen(impl_->path.c_str(), L"rb");
    if (!fin) return make_error_code(ArchiveError::OpenFailed);

    // 打开输出
    FILE* fout = _wfopen(output_path.c_str(), L"wb");
    if (!fout) {
        fclose(fin);
        return make_error_code(ArchiveError::WriteFailed);
    }

    int bzerr;
    BZFILE* bz = BZ2_bzReadOpen(&bzerr, fin, 0, 0, nullptr, 0);
    if (bzerr != BZ_OK) {
        BZ2_bzReadClose(&bzerr, bz);
        fclose(fin);
        fclose(fout);
        return make_error_code(ArchiveError::BadFormat);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<char> buf(BUF_SIZE);
    u64 total_out = 0;

    impl_->current_file = entry.path;

    while (true) {
        if (impl_->cancelled) {
            BZ2_bzReadClose(&bzerr, bz);
            fclose(fin);
            fclose(fout);
            return make_error_code(ArchiveError::Cancelled);
        }

        int n = BZ2_bzRead(&bzerr, bz, buf.data(), static_cast<int>(BUF_SIZE));
        if (bzerr == BZ_STREAM_END) {
            if (n > 0) {
                fwrite(buf.data(), 1, static_cast<size_t>(n), fout);
                total_out += n;
            }
            break;
        }
        if (bzerr != BZ_OK) {
            BZ2_bzReadClose(&bzerr, bz);
            fclose(fin);
            fclose(fout);
            return make_error_code(ArchiveError::BadFormat);
        }
        if (n > 0) {
            fwrite(buf.data(), 1, static_cast<size_t>(n), fout);
            total_out += n;
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

    BZ2_bzReadClose(&bzerr, bz);
    fclose(fin);
    fclose(fout);

    // 更新条目大小
    impl_->cached_entries[0].size = total_out;

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index != 0) return make_error_code(ArchiveError::FileNotFound);

    FILE* fin = _wfopen(impl_->path.c_str(), L"rb");
    if (!fin) return make_error_code(ArchiveError::OpenFailed);

    int bzerr;
    BZFILE* bz = BZ2_bzReadOpen(&bzerr, fin, 0, 0, nullptr, 0);
    if (bzerr != BZ_OK) {
        BZ2_bzReadClose(&bzerr, bz);
        fclose(fin);
        return make_error_code(ArchiveError::BadFormat);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<char> buf(BUF_SIZE);
    u64 total_out = 0;

    while (true) {
        if (impl_->cancelled) {
            BZ2_bzReadClose(&bzerr, bz);
            fclose(fin);
            return make_error_code(ArchiveError::Cancelled);
        }

        int n = BZ2_bzRead(&bzerr, bz, buf.data(), static_cast<int>(BUF_SIZE));
        if (bzerr == BZ_STREAM_END) {
            if (n > 0) total_out += n;
            break;
        }
        if (bzerr != BZ_OK) {
            BZ2_bzReadClose(&bzerr, bz);
            fclose(fin);
            return make_error_code(ArchiveError::BadFormat);
        }
        if (n > 0) total_out += n;

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

    BZ2_bzReadClose(&bzerr, bz);
    fclose(fin);

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::extract_files(const std::vector<u32>& indices,
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
std::error_code Bz2Archive::test() {
    return test_entry(0);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::create(const CreateOptions& opts) {
    impl_->path = opts.archive_path;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 添加文件
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::add_files(const std::vector<tstring>& files,
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

    FILE* fin = _wfopen(input.c_str(), L"rb");
    if (!fin) return make_error_code(ArchiveError::OpenFailed);

    FILE* fout = _wfopen(impl_->path.c_str(), L"wb");
    if (!fout) {
        fclose(fin);
        return make_error_code(ArchiveError::WriteFailed);
    }

    int bzerr;
    int level = opts.level;
    if (level < 1 || level > 9) level = 6;

    BZFILE* bz = BZ2_bzWriteOpen(&bzerr, fout, level, 0, 0);
    if (bzerr != BZ_OK) {
        BZ2_bzWriteClose(&bzerr, bz);
        fclose(fin);
        fclose(fout);
        return make_error_code(ArchiveError::InternalError);
    }

    constexpr size_t BUF_SIZE = 64 * 1024;
    std::vector<char> buf(BUF_SIZE);
    u64 total_in = 0;
    u64 file_size = util::file_size(input);

    impl_->current_file = input;

    while (true) {
        if (impl_->cancelled) {
            BZ2_bzWriteClose(&bzerr, bz);
            fclose(fin);
            fclose(fout);
            return make_error_code(ArchiveError::Cancelled);
        }

        size_t n = fread(buf.data(), 1, BUF_SIZE, fin);
        if (n == 0) break;

        BZ2_bzWrite(&bzerr, bz, buf.data(), static_cast<int>(n));
        if (bzerr != BZ_OK) {
            BZ2_bzWriteClose(&bzerr, bz);
            fclose(fin);
            fclose(fout);
            return make_error_code(ArchiveError::InternalError);
        }

        total_in += n;

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

    BZ2_bzWriteClose(&bzerr, bz);
    fclose(fin);
    fclose(fout);

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code Bz2Archive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
