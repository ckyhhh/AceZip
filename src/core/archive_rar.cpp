// ============================================================================
// archive_rar.cpp - RAR 格式驱动完整实现（仅解压）
//
// 基于 RARLAB 提供的 unRAR 库实现 RAR 格式的读取。
// unRAR 库以 C 接口提供，通过 RAROpenArchiveEx / RARReadHeaderEx /
// RARProcessFile / RARCloseArchive 系列函数操作。
//
// 注意：unRAR 许可证禁止用于创建 RAR 文件，因此本驱动只支持解压。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_rar.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <vector>

// unRAR 库头文件
#include "unrar/dll.hpp"

namespace bandzip {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct RarArchive::Impl {
    tstring path;
    tstring password;
    OpenMode mode = OpenMode::Closed;
    bool is_open = false;

    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    ProgressCallback progress_cb;
    std::atomic<bool> cancelled{false};
    tstring current_file;

    // 解压过程中的回调数据
    u64 current_total = 0;
    u64 current_processed = 0;

    static int CALLBACK unrar_callback(UINT msg, LPARAM userdata,
                                         LPARAM p1, LPARAM p2);
};

// ---------------------------------------------------------------------------
// unRAR 回调
// ---------------------------------------------------------------------------
int CALLBACK RarArchive::Impl::unrar_callback(UINT msg, LPARAM userdata,
                                                  LPARAM p1, LPARAM p2) {
    Impl* self = reinterpret_cast<Impl*>(userdata);
    if (!self) return 0;

    switch (msg) {
    case UCM_CHANGEVOLUME: {
        // 需要下一卷
        // p1 = 下一卷路径，p2 = RAR_VOL_NOTIFY 或 RAR_VOL_ASK
        if (p2 == RAR_VOL_ASK) {
            // 询问用户是否继续
            // 这里简单返回 1 表示继续
            return 1;
        }
        return 1;
    }
    case UCM_PROCESSDATA: {
        // 数据正在处理
        self->current_processed += p2;
        if (self->progress_cb) {
            ProgressInfo info{};
            info.current_file = self->current_file;
            info.bytes_processed = self->current_processed;
            info.bytes_total = self->current_total;
            info.percent = self->current_total > 0 ?
                static_cast<int>(self->current_processed * 100 /
                                  self->current_total) : 0;
            info.cancelled = self->cancelled.load();
            self->progress_cb(info);
            self->cancelled = info.cancelled;
        }
        return self->cancelled ? -1 : 1;
    }
    case UCM_NEEDPASSWORD: {
        // 需要密码
        if (!self->password.empty() && p2 > 0) {
            strncpy_s(reinterpret_cast<char*>(p1), p2,
                      util::tstring_to_string(self->password).c_str(),
                      _TRUNCATE);
            return 1;
        }
        return 0;
    }
    case UCM_CHANGEVOLUMEW: {
        // Unicode 版本
        if (p2 == RAR_VOL_ASK) return 1;
        return 1;
    }
    case UCM_NEEDPASSWORDW: {
        if (!self->password.empty() && p2 > 0) {
            wcsncpy_s(reinterpret_cast<wchar_t*>(p1), p2,
                      self->password.c_str(), _TRUNCATE);
            return 1;
        }
        return 0;
    }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
RarArchive::RarArchive() : impl_(std::make_unique<Impl>()) {}
RarArchive::~RarArchive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code RarArchive::open(const tstring& path,
                                   const tstring& password,
                                   OpenMode mode) {
    if (mode != OpenMode::Read) {
        return make_error_code(ArchiveError::UnsupportedFeature);
    }

    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;

    // 打开归档以读取条目列表
    RAROpenArchiveDataEx open_data = {};
    open_data.ArcName = const_cast<char*>(util::tstring_to_ansi(path).c_str());
    open_data.OpenMode = RAR_OM_EXTRACT;
    open_data.CmtBuf = nullptr;
    open_data.Callback = &Impl::unrar_callback;
    open_data.UserData = reinterpret_cast<LPARAM>(impl_.get());

    HANDLE hArc = RAROpenArchiveEx(&open_data);
    if (open_data.OpenResult != 0) {
        switch (open_data.OpenResult) {
        case ERAR_NO_MEMORY:   return make_error_code(ArchiveError::InternalError);
        case ERAR_BAD_DATA:    return make_error_code(ArchiveError::BadFormat);
        case ERAR_BAD_ARCHIVE: return make_error_code(ArchiveError::BadFormat);
        case ERAR_EOPEN:       return make_error_code(ArchiveError::OpenFailed);
        case ERAR_SMALL_BUF:   return make_error_code(ArchiveError::InternalError);
        default:               return make_error_code(ArchiveError::OpenFailed);
        }
    }

    // 设置密码
    if (!password.empty()) {
        std::string pw = util::tstring_to_ansi(password);
        RARSetPassword(hArc, const_cast<char*>(pw.c_str()));
    }

    // 读取所有条目
    impl_->cached_entries.clear();
    u32 index = 0;

    while (true) {
        RARHeaderDataEx header = {};
        int ret = RARReadHeaderEx(hArc, &header);
        if (ret == ERAR_END_ARCHIVE) break;
        if (ret != 0) {
            RARCloseArchive(hArc);
            return make_error_code(ArchiveError::BadFormat);
        }

        ArchiveEntry e{};
        e.index = index++;
        e.path = util::ansi_to_tstring(header.FileName);
        e.name = util::get_filename(e.path);
        e.size = static_cast<u64>(header.UnpSizeHigh) << 32 | header.UnpSize;
        e.compressed_size = static_cast<u64>(header.PackSizeHigh) << 32 |
                            header.PackSize;
        e.modified = static_cast<std::time_t>(header.mtime);
        e.created = static_cast<std::time_t>(header.ctime);
        e.accessed = static_cast<std::time_t>(header.atime);
        e.is_directory = (header.Flags & RHDF_DIRECTORY) != 0;
        e.is_encrypted = (header.Flags & RHDF_ENCRYPTED) != 0;
        e.is_solid = (header.Flags & RHDF_SOLID) != 0;
        e.crc32 = header.FileCRC;
        e.attributes = header.FileAttr;

        // 压缩方法
        switch (header.Method) {
        case RAR_M0: e.method = CompressionMethod::Copy; break;
        case RAR_M1:
        case RAR_M2:
        case RAR_M3:
        case RAR_M4:
        case RAR_M5: e.method = CompressionMethod::Lzma; break;
        default:     e.method = CompressionMethod::Lzma; break;
        }

        impl_->cached_entries.push_back(e);

        // 跳过数据
        ret = RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
        if (ret != 0) {
            RARCloseArchive(hArc);
            return make_error_code(ArchiveError::ReadFailed);
        }
    }

    RARCloseArchive(hArc);

    impl_->entries_cached = true;
    impl_->is_open = true;
    return make_error_code(ArchiveError::Ok);
}

std::error_code RarArchive::close() {
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    return make_error_code(ArchiveError::Ok);
}

bool RarArchive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code RarArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    entries = impl_->cached_entries;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code RarArchive::extract_entry(u32 index,
                                            const tstring& output_path,
                                            const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index >= impl_->cached_entries.size()) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    const auto& entry = impl_->cached_entries[index];

    // 创建父目录
    tstring parent = util::get_dirname(output_path);
    if (!parent.empty() && !util::dir_exists(parent)) {
        if (!util::create_dir_recursive(parent)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    // 如果是目录，直接创建
    if (entry.is_directory) {
        if (!util::create_dir_recursive(output_path)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
        return make_error_code(ArchiveError::Ok);
    }

    // 重新打开归档
    RAROpenArchiveDataEx open_data = {};
    open_data.ArcName = const_cast<char*>(util::tstring_to_ansi(impl_->path).c_str());
    open_data.OpenMode = RAR_OM_EXTRACT;
    open_data.Callback = &Impl::unrar_callback;
    open_data.UserData = reinterpret_cast<LPARAM>(impl_.get());

    HANDLE hArc = RAROpenArchiveEx(&open_data);
    if (open_data.OpenResult != 0) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    if (!impl_->password.empty()) {
        std::string pw = util::tstring_to_ansi(impl_->password);
        RARSetPassword(hArc, const_cast<char*>(pw.c_str()));
    }

    impl_->current_file = entry.path;
    impl_->current_total = entry.size;
    impl_->current_processed = 0;
    impl_->cancelled = false;

    // 跳过到目标条目
    u32 current = 0;
    std::error_code ec;

    while (current <= index) {
        RARHeaderDataEx header = {};
        int ret = RARReadHeaderEx(hArc, &header);
        if (ret == ERAR_END_ARCHIVE) {
            ec = make_error_code(ArchiveError::FileNotFound);
            break;
        }
        if (ret != 0) {
            ec = make_error_code(ArchiveError::BadFormat);
            break;
        }

        if (current == index) {
            // 解压此条目
            std::string out_dir = util::tstring_to_ansi(
                util::get_dirname(output_path));
            std::string out_name = util::tstring_to_ansi(
                util::get_filename(output_path));

            ret = RARProcessFile(hArc, RAR_EXTRACT,
                                  out_dir.empty() ? nullptr :
                                  const_cast<char*>(out_dir.c_str()),
                                  out_name.empty() ? nullptr :
                                  const_cast<char*>(out_name.c_str()));
            if (ret != 0) {
                switch (ret) {
                case ERAR_BAD_DATA:
                    ec = make_error_code(ArchiveError::BadFormat);
                    break;
                case ERAR_BAD_ARCHIVE:
                    ec = make_error_code(ArchiveError::BadFormat);
                    break;
                case ERAR_UNKNOWN_FORMAT:
                    ec = make_error_code(ArchiveError::UnsupportedMethod);
                    break;
                case ERAR_ECREATE:
                    ec = make_error_code(ArchiveError::WriteFailed);
                    break;
                case ERAR_ECLOSE:
                    ec = make_error_code(ArchiveError::WriteFailed);
                    break;
                case ERAR_EREAD:
                    ec = make_error_code(ArchiveError::ReadFailed);
                    break;
                case ERAR_WRITE:
                    ec = make_error_code(ArchiveError::WriteFailed);
                    break;
                case ERAR_SMALL_BUF:
                    ec = make_error_code(ArchiveError::InternalError);
                    break;
                case ERAR_MISSING_PASSWORD:
                    ec = make_error_code(ArchiveError::PasswordRequired);
                    break;
                case ERAR_EREFERENCE:
                    ec = make_error_code(ArchiveError::BadFormat);
                    break;
                case ERAR_BAD_PASSWORD:
                    ec = make_error_code(ArchiveError::WrongPassword);
                    break;
                default:
                    ec = make_error_code(ArchiveError::InternalError);
                    break;
                }
            }
            break;
        } else {
            // 跳过
            ret = RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
            if (ret != 0) {
                ec = make_error_code(ArchiveError::ReadFailed);
                break;
            }
        }
        ++current;
    }

    RARCloseArchive(hArc);
    return ec;
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code RarArchive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index >= impl_->cached_entries.size()) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    const auto& entry = impl_->cached_entries[index];

    RAROpenArchiveDataEx open_data = {};
    open_data.ArcName = const_cast<char*>(util::tstring_to_ansi(impl_->path).c_str());
    open_data.OpenMode = RAR_OM_EXTRACT;
    open_data.Callback = &Impl::unrar_callback;
    open_data.UserData = reinterpret_cast<LPARAM>(impl_.get());

    HANDLE hArc = RAROpenArchiveEx(&open_data);
    if (open_data.OpenResult != 0) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    if (!impl_->password.empty()) {
        std::string pw = util::tstring_to_ansi(impl_->password);
        RARSetPassword(hArc, const_cast<char*>(pw.c_str()));
    }

    impl_->current_file = entry.path;
    impl_->current_total = entry.size;
    impl_->current_processed = 0;
    impl_->cancelled = false;

    u32 current = 0;
    std::error_code ec;

    while (current <= index) {
        RARHeaderDataEx header = {};
        int ret = RARReadHeaderEx(hArc, &header);
        if (ret == ERAR_END_ARCHIVE) {
            ec = make_error_code(ArchiveError::FileNotFound);
            break;
        }
        if (ret != 0) {
            ec = make_error_code(ArchiveError::BadFormat);
            break;
        }

        if (current == index) {
            ret = RARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
            if (ret != 0) {
                switch (ret) {
                case ERAR_BAD_DATA:
                    ec = make_error_code(ArchiveError::CRCMismatch);
                    break;
                case ERAR_MISSING_PASSWORD:
                    ec = make_error_code(ArchiveError::PasswordRequired);
                    break;
                case ERAR_BAD_PASSWORD:
                    ec = make_error_code(ArchiveError::WrongPassword);
                    break;
                default:
                    ec = make_error_code(ArchiveError::InternalError);
                    break;
                }
            }
            break;
        } else {
            ret = RARProcessFile(hArc, RAR_SKIP, nullptr, nullptr);
            if (ret != 0) {
                ec = make_error_code(ArchiveError::ReadFailed);
                break;
            }
        }
        ++current;
    }

    RARCloseArchive(hArc);
    return ec;
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code RarArchive::extract_files(const std::vector<u32>& indices,
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
        tstring out_path = util::join_path(output_dir, entry.path);
        auto ec = extract_entry(idx, out_path, opts);
        if (ec) return ec;
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试整个归档
// ---------------------------------------------------------------------------
std::error_code RarArchive::test() {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    RAROpenArchiveDataEx open_data = {};
    open_data.ArcName = const_cast<char*>(util::tstring_to_ansi(impl_->path).c_str());
    open_data.OpenMode = RAR_OM_EXTRACT;
    open_data.Callback = &Impl::unrar_callback;
    open_data.UserData = reinterpret_cast<LPARAM>(impl_.get());

    HANDLE hArc = RAROpenArchiveEx(&open_data);
    if (open_data.OpenResult != 0) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    if (!impl_->password.empty()) {
        std::string pw = util::tstring_to_ansi(impl_->password);
        RARSetPassword(hArc, const_cast<char*>(pw.c_str()));
    }

    impl_->cancelled = false;
    std::error_code ec;

    while (true) {
        if (impl_->cancelled) {
            ec = make_error_code(ArchiveError::Cancelled);
            break;
        }

        RARHeaderDataEx header = {};
        int ret = RARReadHeaderEx(hArc, &header);
        if (ret == ERAR_END_ARCHIVE) break;
        if (ret != 0) {
            ec = make_error_code(ArchiveError::BadFormat);
            break;
        }

        impl_->current_file = util::ansi_to_tstring(header.FileName);
        impl_->current_total = static_cast<u64>(header.UnpSizeHigh) << 32 |
                                header.UnpSize;
        impl_->current_processed = 0;

        ret = RARProcessFile(hArc, RAR_TEST, nullptr, nullptr);
        if (ret != 0) {
            switch (ret) {
            case ERAR_BAD_DATA:
                ec = make_error_code(ArchiveError::CRCMismatch);
                break;
            case ERAR_MISSING_PASSWORD:
                ec = make_error_code(ArchiveError::PasswordRequired);
                break;
            case ERAR_BAD_PASSWORD:
                ec = make_error_code(ArchiveError::WrongPassword);
                break;
            default:
                ec = make_error_code(ArchiveError::InternalError);
                break;
            }
            break;
        }
    }

    RARCloseArchive(hArc);
    return ec;
}

// ---------------------------------------------------------------------------
// 创建归档（不支持）
// ---------------------------------------------------------------------------
std::error_code RarArchive::create(const CreateOptions& opts) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

// ---------------------------------------------------------------------------
// 添加文件（不支持）
// ---------------------------------------------------------------------------
std::error_code RarArchive::add_files(const std::vector<tstring>& files,
                                        const CompressOptions& opts) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

// ---------------------------------------------------------------------------
// 删除条目（不支持）
// ---------------------------------------------------------------------------
std::error_code RarArchive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
