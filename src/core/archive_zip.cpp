// ============================================================================
// archive_zip.cpp - ZIP 格式驱动实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_zip.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <ctime>

// minizip-ng 头文件
extern "C" {
#include <mz.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_strm_split.h>
#include <mz_strm_mem.h>
}

namespace bandzip {

// ---------------------------------------------------------------------------
// 工具：minizip 错误码 -> ArchiveError
// ---------------------------------------------------------------------------
static ArchiveError mz_to_archive_err(int32_t mz_err) {
    switch (mz_err) {
    case MZ_OK:                       return ArchiveError::Ok;
    case MZ_OPEN_ERROR:               return ArchiveError::OpenFailed;
    case MZ_READ_ERROR:               return ArchiveError::ReadFailed;
    case MZ_WRITE_ERROR:              return ArchiveError::WriteFailed;
    case MZ_CLOSE_ERROR:              return ArchiveError::WriteFailed;
    case MZ_SEEK_ERROR:               return ArchiveError::ReadFailed;
    case MZ_TELL_ERROR:               return ArchiveError::ReadFailed;
    case MZ_CRC_ERROR:                return ArchiveError::CRCMismatch;
    case MZ_FORMAT_ERROR:             return ArchiveError::BadFormat;
    case MZ_PASSWORD_ERROR:            return ArchiveError::WrongPassword;
    case MZ_SUPPORT_ERROR:            return ArchiveError::UnsupportedMethod;
    case MZ_END_OF_LIST:              return ArchiveError::FileNotFound;
    case MZ_END_OF_STREAM:            return ArchiveError::Truncated;
    default:                          return ArchiveError::InternalError;
    }
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct ZipArchive::Impl {
    void* zip_handle = nullptr;
    void* stream = nullptr;
    tstring path;
    tstring password;
    OpenMode mode = OpenMode::Closed;
    bool is_split = false;
    u64 volume_size = 0;

    // 缓存的条目列表
    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    // 创建选项
    CreateOptions create_opts;

    // 进度回调
    ProgressCallback progress_cb;
    void* progress_user = nullptr;
    std::atomic<bool> cancelled{false};

    // 密码回调
    PasswordCallback password_cb;

    ~Impl() {
        if (zip_handle) {
            mz_zip_close(zip_handle);
            zip_handle = nullptr;
        }
        if (stream) {
            mz_stream_close(stream);
            mz_stream_delete(&stream);
            stream = nullptr;
        }
    }

    // 调用进度回调
    bool call_progress(const tstring& file, u64 processed, u64 total) {
        if (!progress_cb) return true;
        ProgressInfo info{};
        info.current_file = file;
        info.bytes_processed = processed;
        info.bytes_total = total;
        info.percent = total > 0 ? static_cast<int>(processed * 100 / total) : 0;
        info.cancelled = false;
        progress_cb(info);
        cancelled = info.cancelled;
        return !info.cancelled;
    }

    // 调用密码回调
    tstring ask_password(const tstring& file) {
        if (!password_cb) return tstring();
        PasswordInfo info{};
        info.archive_path = path;
        info.entry_path = file;
        info.first_attempt = password.empty();
        info.cancelled = false;
        password_cb(info);
        if (info.cancelled) return tstring();
        return info.password;
    }
};

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
ZipArchive::ZipArchive() : impl_(std::make_unique<Impl>()) {}
ZipArchive::~ZipArchive() = default;

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code ZipArchive::open(const tstring& path,
                                  const tstring& password,
                                  OpenMode mode) {
    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;
    impl_->cancelled = false;

    int32_t err = MZ_OK;

    // 创建流
    if (mode == OpenMode::Read) {
        err = mz_stream_os_create(&impl_->stream);
        if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

        err = mz_stream_open(impl_->stream, path.c_str(), MZ_OPEN_MODE_READ);
        if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

        // 检测分卷
        int32_t is_split = 0;
        mz_stream_set_prop(impl_->stream, MZ_STREAM_PROP_DISK_NUMBER, &is_split);
        impl_->is_split = (is_split != 0);
    } else if (mode == OpenMode::Write) {
        err = mz_stream_os_create(&impl_->stream);
        if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

        err = mz_stream_open(impl_->stream, path.c_str(), MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE);
        if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

        // 分卷支持
        if (impl_->volume_size > 0) {
            void* split_stream = nullptr;
            mz_stream_split_create(&split_stream);
            mz_stream_set_base(split_stream, impl_->stream);
            int64_t v = static_cast<int64_t>(impl_->volume_size);
            mz_stream_set_prop(split_stream, MZ_STREAM_PROP_VOLUME_SIZE, &v);
            impl_->stream = split_stream;
        }
    } else {
        return make_error_code(ArchiveError::InvalidParameter);
    }

    // 打开 ZIP
    err = mz_zip_create(&impl_->zip_handle);
    if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

    int32_t zip_mode = (mode == OpenMode::Read) ? MZ_OPEN_MODE_READ : MZ_OPEN_MODE_WRITE;
    err = mz_zip_open(impl_->zip_handle, impl_->stream, zip_mode);
    if (err != MZ_OK) {
        mz_zip_delete(&impl_->zip_handle);
        impl_->zip_handle = nullptr;
        return make_error_code(mz_to_archive_err(err));
    }

    // 设置密码
    if (!password.empty() && mode == OpenMode::Read) {
        mz_zip_set_password(impl_->zip_handle, util::tstring_to_string(password).c_str());
    }

    impl_->entries_cached = false;
    return make_error_code(ArchiveError::Ok);
}

std::error_code ZipArchive::close() {
    if (!impl_->zip_handle) return make_error_code(ArchiveError::Ok);

    int32_t err = mz_zip_close(impl_->zip_handle);
    mz_zip_delete(&impl_->zip_handle);
    impl_->zip_handle = nullptr;

    if (impl_->stream) {
        mz_stream_close(impl_->stream);
        mz_stream_delete(&impl_->stream);
        impl_->stream = nullptr;
    }

    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;

    return make_error_code(mz_to_archive_err(err));
}

bool ZipArchive::is_open() const {
    return impl_->zip_handle != nullptr && impl_->mode != OpenMode::Closed;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code ZipArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (impl_->entries_cached) {
        entries = impl_->cached_entries;
        return make_error_code(ArchiveError::Ok);
    }

    entries.clear();

    int32_t err = mz_zip_goto_first_entry(impl_->zip_handle);
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        return make_error_code(mz_to_archive_err(err));
    }

    while (err == MZ_OK) {
        mz_zip_file* file_info = nullptr;
        err = mz_zip_entry_get_info(impl_->zip_handle, &file_info);
        if (err != MZ_OK) break;

        ArchiveEntry e{};
        e.index = static_cast<u32>(entries.size());

        // 文件名编码处理
        std::string name_bytes(file_info->filename, file_info->filename_size);
        if (file_info->flag & MZ_ZIP_FLAG_UTF8) {
            e.path = util::utf8_to_utf16(name_bytes);
        } else {
            // 自动检测编码
            e.path = CodecDetector::decode_auto(name_bytes);
        }
        e.name = util::get_filename(e.path);

        e.size = file_info->uncompressed_size;
        e.compressed_size = file_info->compressed_size;
        e.crc32 = file_info->crc32;
        e.is_directory = (file_info->external_attr & 0x10) != 0;

        // 时间
        e.modified = util::dostime_to_unix(file_info->modified_date, file_info->modified_time);
        e.created = e.modified;
        e.accessed = e.modified;

        // 属性
        e.attributes = file_info->external_attr >> 16;

        // 加密
        e.encrypted = (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0;

        // 压缩方法
        switch (file_info->compression_method) {
        case MZ_COMPRESS_METHOD_STORE:    e.method = CompressionMethod::Copy; break;
        case MZ_COMPRESS_METHOD_DEFLATE:  e.method = CompressionMethod::Deflate; break;
        case MZ_COMPRESS_METHOD_BZIP2:    e.method = CompressionMethod::Bzip2; break;
        case MZ_COMPRESS_METHOD_LZMA:     e.method = CompressionMethod::Lzma; break;
        case MZ_COMPRESS_METHOD_XZ:       e.method = CompressionMethod::Lzma2; break;
        case MZ_COMPRESS_METHOD_ZSTD:     e.method = CompressionMethod::Zstd; break;
        default:                          e.method = CompressionMethod::Unknown; break;
        }

        entries.push_back(std::move(e));

        err = mz_zip_goto_next_entry(impl_->zip_handle);
    }

    impl_->cached_entries = entries;
    impl_->entries_cached = true;

    if (err == MZ_END_OF_LIST) err = MZ_OK;
    return make_error_code(mz_to_archive_err(err));
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code ZipArchive::extract_entry(u32 index,
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

    // 目录
    if (entry.is_directory) {
        if (!util::create_dir_recursive(output_path)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
        return make_error_code(ArchiveError::Ok);
    }

    // 安全检查：防止 zip slip
    if (util::has_parent_ref(output_path)) {
        return make_error_code(ArchiveError::InvalidParameter);
    }

    // 创建父目录
    tstring parent = util::get_dirname(output_path);
    if (!parent.empty() && !util::dir_exists(parent)) {
        if (!util::create_dir_recursive(parent)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    // 定位到条目
    int32_t err = mz_zip_goto_first_entry(impl_->zip_handle);
    while (err == MZ_OK) {
        mz_zip_file* fi = nullptr;
        mz_zip_entry_get_info(impl_->zip_handle, &fi);
        if (static_cast<u32>(fi->index) == index) break;
        err = mz_zip_goto_next_entry(impl_->zip_handle);
    }
    if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

    // 打开读取
    err = mz_zip_entry_read_open(impl_->zip_handle, 0,
                                  impl_->password.empty() ? nullptr :
                                  util::tstring_to_string(impl_->password).c_str());
    if (err != MZ_OK) {
        if (err == MZ_PASSWORD_ERROR) {
            // 询问密码
            tstring pwd = impl_->ask_password(entry.path);
            if (pwd.empty()) return make_error_code(ArchiveError::Cancelled);
            impl_->password = pwd;
            err = mz_zip_entry_read_open(impl_->zip_handle, 0,
                                          util::tstring_to_string(pwd).c_str());
            if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));
        } else {
            return make_error_code(mz_to_archive_err(err));
        }
    }

    // 创建输出文件
    HANDLE hOut = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        mz_zip_entry_close(impl_->zip_handle);
        return make_error_code(ArchiveError::WriteFailed);
    }

    // 读取并写入
    std::vector<u8> buf(64 * 1024);
    u64 total_read = 0;
    u64 total_size = entry.size;

    while (!impl_->cancelled) {
        int32_t n = mz_zip_entry_read(impl_->zip_handle, buf.data(),
                                       static_cast<int32_t>(buf.size()));
        if (n < 0) {
            CloseHandle(hOut);
            mz_zip_entry_close(impl_->zip_handle);
            return make_error_code(mz_to_archive_err(n));
        }
        if (n == 0) break;

        DWORD written = 0;
        if (!WriteFile(hOut, buf.data(), static_cast<DWORD>(n), &written, nullptr) ||
            written != static_cast<DWORD>(n)) {
            CloseHandle(hOut);
            mz_zip_entry_close(impl_->zip_handle);
            return make_error_code(ArchiveError::WriteFailed);
        }

        total_read += n;
        if (!impl_->call_progress(entry.path, total_read, total_size)) {
            CloseHandle(hOut);
            mz_zip_entry_close(impl_->zip_handle);
            return make_error_code(ArchiveError::Cancelled);
        }
    }

    CloseHandle(hOut);
    mz_zip_entry_close(impl_->zip_handle);

    // 设置时间
    if (opts.preserve_attributes) {
        FILETIME ft = util::unix_to_filetime(entry.modified);
        HANDLE h = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            SetFileTime(h, &ft, &ft, &ft);
            CloseHandle(h);
        }
        // 设置属性
        if (entry.attributes) {
            SetFileAttributes(output_path.c_str(),
                              util::archive_attr_to_win(entry.attributes));
        }
    }

    if (impl_->cancelled) return make_error_code(ArchiveError::Cancelled);
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code ZipArchive::test_entry(u32 index) {
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
    if (entry.is_directory) return make_error_code(ArchiveError::Ok);

    // 定位
    int32_t err = mz_zip_goto_first_entry(impl_->zip_handle);
    while (err == MZ_OK) {
        mz_zip_file* fi = nullptr;
        mz_zip_entry_get_info(impl_->zip_handle, &fi);
        if (static_cast<u32>(fi->index) == index) break;
        err = mz_zip_goto_next_entry(impl_->zip_handle);
    }
    if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

    err = mz_zip_entry_read_open(impl_->zip_handle, 0,
                                  impl_->password.empty() ? nullptr :
                                  util::tstring_to_string(impl_->password).c_str());
    if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

    // 读取并校验 CRC
    std::vector<u8> buf(64 * 1024);
    u64 total_read = 0;
    u32 crc = 0;

    while (true) {
        int32_t n = mz_zip_entry_read(impl_->zip_handle, buf.data(),
                                       static_cast<int32_t>(buf.size()));
        if (n < 0) {
            mz_zip_entry_close(impl_->zip_handle);
            return make_error_code(mz_to_archive_err(n));
        }
        if (n == 0) break;
        crc = mz_crc32_update(crc, buf.data(), static_cast<int32_t>(n));
        total_read += n;
        if (!impl_->call_progress(entry.path, total_read, entry.size)) {
            mz_zip_entry_close(impl_->zip_handle);
            return make_error_code(ArchiveError::Cancelled);
        }
    }

    mz_zip_entry_close(impl_->zip_handle);

    if (crc != entry.crc32) {
        return make_error_code(ArchiveError::CRCMismatch);
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code ZipArchive::extract_files(const std::vector<u32>& indices,
                                           const tstring& output_dir,
                                           const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!util::dir_exists(output_dir)) {
        if (!util::create_dir_recursive(output_dir)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    for (u32 idx : indices) {
        if (impl_->cancelled) return make_error_code(ArchiveError::Cancelled);

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

        // 安全检查
        if (util::has_parent_ref(entry.path)) {
            LOG_WARN(_T("Skipping unsafe path: ") << entry.path);
            continue;
        }

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
std::error_code ZipArchive::test() {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!impl_->entries_cached) {
        std::vector<ArchiveEntry> tmp;
        auto ec = read_entries(tmp);
        if (ec) return ec;
    }

    for (const auto& entry : impl_->cached_entries) {
        if (impl_->cancelled) return make_error_code(ArchiveError::Cancelled);
        auto ec = test_entry(entry.index);
        if (ec) return ec;
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code ZipArchive::create(const CreateOptions& opts) {
    impl_->create_opts = opts;
    impl_->volume_size = opts.volume_size;
    impl_->cancelled = false;

    return open(opts.archive_path, opts.password, OpenMode::Write);
}

// ---------------------------------------------------------------------------
// 添加文件
// ---------------------------------------------------------------------------
std::error_code ZipArchive::add_files(const std::vector<tstring>& files,
                                       const CompressOptions& opts) {
    if (!is_open() || impl_->mode != OpenMode::Write) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    for (const auto& file : files) {
        if (impl_->cancelled) return make_error_code(ArchiveError::Cancelled);

        if (util::dir_exists(file)) {
            // 递归添加目录
            auto ec = util::walk_dir(file, [this, &file, &opts](const util::FindData& d) -> bool {
                if (d.is_directory) return true;

                // 计算归档内路径
                tstring rel = util::make_relative(file, d.path);
                tstring archive_path = util::to_archive_path(
                    util::join_path(util::get_basename(file), rel));

                // 添加文件
                mz_zip_file info = {};
                info.filename = util::tstring_to_string(archive_path).c_str();
                info.filename_size = strlen(info.filename);
                info.flag = MZ_ZIP_FLAG_UTF8;

                // 压缩方法
                int32_t method = MZ_COMPRESS_METHOD_DEFLATE;
                switch (opts.method) {
                case CompressionMethod::Copy:     method = MZ_COMPRESS_METHOD_STORE; break;
                case CompressionMethod::Deflate:   method = MZ_COMPRESS_METHOD_DEFLATE; break;
                case CompressionMethod::Bzip2:     method = MZ_COMPRESS_METHOD_BZIP2; break;
                case CompressionMethod::Lzma:      method = MZ_COMPRESS_METHOD_LZMA; break;
                case CompressionMethod::Zstd:      method = MZ_COMPRESS_METHOD_ZSTD; break;
                default: break;
                }

                int32_t err = mz_zip_entry_write_open(impl_->zip_handle, &info,
                                                       opts.level, opts.use_aes ? 1 : 0,
                                                       impl_->password.empty() ? nullptr :
                                                       util::tstring_to_string(impl_->password).c_str());
                if (err != MZ_OK) return false;

                // 读取并写入
                HANDLE hIn = CreateFile(d.path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hIn == INVALID_HANDLE_VALUE) {
                    mz_zip_entry_close(impl_->zip_handle);
                    return false;
                }

                std::vector<u8> buf(64 * 1024);
                u64 total = 0;
                LARGE_INTEGER fsz;
                GetFileSizeEx(hIn, &fsz);
                u64 file_size = static_cast<u64>(fsz.QuadPart);

                while (true) {
                    DWORD rd = 0;
                    if (!ReadFile(hIn, buf.data(), static_cast<DWORD>(buf.size()), &rd, nullptr) || rd == 0)
                        break;
                    int32_t w = mz_zip_entry_write(impl_->zip_handle, buf.data(), static_cast<int32_t>(rd));
                    if (w < 0) break;
                    total += rd;
                    if (!impl_->call_progress(d.path, total, file_size)) {
                        CloseHandle(hIn);
                        mz_zip_entry_close(impl_->zip_handle);
                        return false;
                    }
                }

                CloseHandle(hIn);
                mz_zip_entry_close(impl_->zip_handle);
                return true;
            }, true);
            if (ec) return ec;
        } else {
            // 单文件
            if (!util::file_exists(file)) {
                return make_error_code(ArchiveError::FileNotFound);
            }

            tstring archive_name = util::to_archive_path(util::get_filename(file));

            mz_zip_file info = {};
            info.filename = util::tstring_to_string(archive_name).c_str();
            info.filename_size = strlen(info.filename);
            info.flag = MZ_ZIP_FLAG_UTF8;

            int32_t method = MZ_COMPRESS_METHOD_DEFLATE;
            switch (opts.method) {
            case CompressionMethod::Copy:     method = MZ_COMPRESS_METHOD_STORE; break;
            case CompressionMethod::Deflate:   method = MZ_COMPRESS_METHOD_DEFLATE; break;
            case CompressionMethod::Bzip2:     method = MZ_COMPRESS_METHOD_BZIP2; break;
            case CompressionMethod::Lzma:      method = MZ_COMPRESS_METHOD_LZMA; break;
            case CompressionMethod::Zstd:      method = MZ_COMPRESS_METHOD_ZSTD; break;
            default: break;
            }

            int32_t err = mz_zip_entry_write_open(impl_->zip_handle, &info,
                                                   opts.level, opts.use_aes ? 1 : 0,
                                                   impl_->password.empty() ? nullptr :
                                                   util::tstring_to_string(impl_->password).c_str());
            if (err != MZ_OK) return make_error_code(mz_to_archive_err(err));

            HANDLE hIn = CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hIn == INVALID_HANDLE_VALUE) {
                mz_zip_entry_close(impl_->zip_handle);
                return make_error_code(ArchiveError::ReadFailed);
            }

            std::vector<u8> buf(64 * 1024);
            u64 total = 0;
            LARGE_INTEGER fsz;
            GetFileSizeEx(hIn, &fsz);
            u64 file_size = static_cast<u64>(fsz.QuadPart);

            while (true) {
                DWORD rd = 0;
                if (!ReadFile(hIn, buf.data(), static_cast<DWORD>(buf.size()), &rd, nullptr) || rd == 0)
                    break;
                int32_t w = mz_zip_entry_write(impl_->zip_handle, buf.data(), static_cast<int32_t>(rd));
                if (w < 0) {
                    CloseHandle(hIn);
                    mz_zip_entry_close(impl_->zip_handle);
                    return make_error_code(ArchiveError::WriteFailed);
                }
                total += rd;
                if (!impl_->call_progress(file, total, file_size)) {
                    CloseHandle(hIn);
                    mz_zip_entry_close(impl_->zip_handle);
                    return make_error_code(ArchiveError::Cancelled);
                }
            }

            CloseHandle(hIn);
            mz_zip_entry_close(impl_->zip_handle);
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code ZipArchive::delete_entries(const std::vector<u32>& indices) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    // minizip-ng 不直接支持删除，需要重写整个归档
    // 这里返回不支持
    return make_error_code(ArchiveError::UnsupportedFeature);
}

// ---------------------------------------------------------------------------
// 注释
// ---------------------------------------------------------------------------
std::error_code ZipArchive::set_comment(const tstring& comment) {
    if (!is_open() || impl_->mode != OpenMode::Write) {
        return make_error_code(ArchiveError::OpenFailed);
    }
    std::string c = util::tstring_to_string(comment);
    int32_t err = mz_zip_set_comment(impl_->zip_handle, c.c_str());
    return make_error_code(mz_to_archive_err(err));
}

tstring ZipArchive::comment() const {
    if (!is_open()) return tstring();
    const char* c = mz_zip_get_comment(impl_->zip_handle);
    if (!c) return tstring();
    return util::utf8_to_utf16(c);
}

} // namespace bandzip
