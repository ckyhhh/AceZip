// ============================================================================
// archive_7z.cpp - 7z 格式驱动实现
//
// 使用 LZMA SDK 的 7z 格式读写接口。
// 由于 LZMA SDK 是 C 接口，这里包装为 C++ 类。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_7z.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <ctime>

// LZMA SDK 头文件
#include "7z.h"
#include "7zAlloc.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zVersion.h"

namespace bandzip {

// ---------------------------------------------------------------------------
// 错误码转换
// ---------------------------------------------------------------------------
static ArchiveError sz_to_archive_err(int sz_err) {
    switch (sz_err) {
    case SZ_OK:                       return ArchiveError::Ok;
    case SZ_ERROR_DATA:               return ArchiveError::BadFormat;
    case SZ_ERROR_MEM:                return ArchiveError::InternalError;
    case SZ_ERROR_CRC:                return ArchiveError::CRCMismatch;
    case SZ_ERROR_UNSUPPORTED:        return ArchiveError::UnsupportedMethod;
    case SZ_ERROR_PARAM:              return ArchiveError::InvalidParameter;
    case SZ_ERROR_INPUT_EOF:          return ArchiveError::Truncated;
    case SZ_ERROR_OUTPUT_EOF:         return ArchiveError::DiskFull;
    case SZ_ERROR_READ:               return ArchiveError::ReadFailed;
    case SZ_ERROR_WRITE:              return ArchiveError::WriteFailed;
    case SZ_ERROR_PROGRESS:           return ArchiveError::Cancelled;
    case SZ_ERROR_FAIL:               return ArchiveError::InternalError;
    case SZ_ERROR_THREAD:             return ArchiveError::InternalError;
    case SZ_ERROR_ARCHIVE:            return ArchiveError::BadFormat;
    case SZ_ERROR_NO_ARCHIVE:         return ArchiveError::BadFormat;
    case SZ_ERROR_PASSWORD:            return ArchiveError::WrongPassword;
    default:                          return ArchiveError::InternalError;
    }
}

// ---------------------------------------------------------------------------
// 内存分配器
// ---------------------------------------------------------------------------
static ISzAlloc g_Alloc = { SzAlloc, SzFree };
static ISzAlloc g_AllocTemp = { SzAllocTemp, SzFreeTemp };

// ---------------------------------------------------------------------------
// 进度回调包装
// ---------------------------------------------------------------------------
struct SzProgressCtx {
    ProgressCallback cb;
    std::atomic<bool> cancelled{false};
    tstring current_file;
    u64 total = 0;
    u64 processed = 0;
};

static SRes SzProgressCallback(void* p, u64 in_size, u64 out_size) {
    auto* ctx = static_cast<SzProgressCtx*>(p);
    if (!ctx->cb) return SZ_OK;

    ProgressInfo info{};
    info.current_file = ctx->current_file;
    info.bytes_processed = in_size;
    info.bytes_total = ctx->total;
    info.percent = ctx->total > 0 ? static_cast<int>(in_size * 100 / ctx->total) : 0;
    info.cancelled = false;
    ctx->cb(info);
    ctx->cancelled = info.cancelled;

    return info.cancelled ? SZ_ERROR_PROGRESS : SZ_OK;
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct SevenZipArchive::Impl {
    CFileInStream file_stream{};
    CLookToRead2 look_stream{};
    CSzArEx db{};
    tstring path;
    tstring password;
    OpenMode mode = OpenMode::Closed;
    bool is_open = false;

    // 缓存的条目
    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    // 解压时的块索引
    UInt32 block_index_start = 0xFFFFFFFF;
    Byte* out_buffer = nullptr;
    size_t out_buffer_size = 0;

    // 进度
    SzProgressCtx progress_ctx;

    // 密码回调
    PasswordCallback password_cb;

    ~Impl() {
        if (out_buffer) {
            ISzAlloc_Free(&g_Alloc, out_buffer);
            out_buffer = nullptr;
        }
        if (is_open) {
            SzArEx_Free(&db, &g_Alloc);
            File_Close(&file_stream.file);
        }
    }

    tstring ask_password() {
        if (!password_cb) return tstring();
        PasswordInfo info{};
        info.archive_path = path;
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
SevenZipArchive::SevenZipArchive() : impl_(std::make_unique<Impl>()) {}
SevenZipArchive::~SevenZipArchive() = default;

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::open(const tstring& path,
                                       const tstring& password,
                                       OpenMode mode) {
    if (mode != OpenMode::Read) {
        // 写入模式由 create() 处理
        return make_error_code(ArchiveError::UnsupportedFeature);
    }

    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;

    // 初始化 CRC 表（仅一次）
    static bool crc_inited = false;
    if (!crc_inited) {
        CrcGenerateTable();
        crc_inited = true;
    }

    // 打开文件
    WRes werr = File_Open(&impl_->file_stream.file, path.c_str(), 0);
    if (werr != 0) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    // 初始化流
    FileInStream_CreateVTable(&impl_->file_stream);
    LookToRead2_CreateVTable(&impl_->look_stream, False);
    impl_->look_stream.buf = nullptr;

    // 获取文件大小
    UInt64 file_size = 0;
    File_GetLength(&impl_->file_stream.file, &file_size);
    impl_->file_stream.vt.Seek(&impl_->file_stream, 0, SZ_SEEK_SET);
    impl_->look_stream.realStream = &impl_->file_stream.vt;
    LookToRead2_Init(&impl_->look_stream);

    // 初始化数据库
    SzArEx_Init(&impl_->db);

    // 设置密码
    CSzArEx* db = &impl_->db;
    if (!password.empty()) {
        std::string pwd = util::tstring_to_string(password);
        SzArEx_SetPassword(db, pwd.c_str(), static_cast<size_t>(pwd.size()));
    }

    // 打开归档
    SRes res = SzArEx_Open(db, &impl_->look_stream.vt, &g_Alloc, &g_AllocTemp);
    if (res == SZ_ERROR_PASSWORD) {
        // 询问密码
        tstring pwd = impl_->ask_password();
        if (pwd.empty()) {
            File_Close(&impl_->file_stream.file);
            return make_error_code(ArchiveError::Cancelled);
        }
        impl_->password = pwd;
        std::string pwd_s = util::tstring_to_string(pwd);
        SzArEx_SetPassword(db, pwd_s.c_str(), static_cast<size_t>(pwd_s.size()));

        // 重新打开
        SzArEx_Free(db, &g_Alloc);
        SzArEx_Init(db);
        impl_->file_stream.vt.Seek(&impl_->file_stream, 0, SZ_SEEK_SET);
        LookToRead2_Init(&impl_->look_stream);
        res = SzArEx_Open(db, &impl_->look_stream.vt, &g_Alloc, &g_AllocTemp);
    }

    if (res != SZ_OK) {
        SzArEx_Free(db, &g_Alloc);
        File_Close(&impl_->file_stream.file);
        return make_error_code(sz_to_archive_err(res));
    }

    impl_->is_open = true;
    impl_->entries_cached = false;
    return make_error_code(ArchiveError::Ok);
}

std::error_code SevenZipArchive::close() {
    if (!impl_->is_open) return make_error_code(ArchiveError::Ok);

    if (impl_->out_buffer) {
        ISzAlloc_Free(&g_Alloc, impl_->out_buffer);
        impl_->out_buffer = nullptr;
    }
    SzArEx_Free(&impl_->db, &g_Alloc);
    File_Close(&impl_->file_stream.file);

    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;

    return make_error_code(ArchiveError::Ok);
}

bool SevenZipArchive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (impl_->entries_cached) {
        entries = impl_->cached_entries;
        return make_error_code(ArchiveError::Ok);
    }

    entries.clear();
    CSzArEx* db = &impl_->db;

    for (UInt32 i = 0; i < db->NumFiles; ++i) {
        CSzFileItem* fi = db->Files + i;
        if (fi->IsDir) continue;  // 目录会在路径中体现

        ArchiveEntry e{};
        e.index = static_cast<u32>(entries.size());

        // 文件名
        size_t name_len = SzArEx_GetFileNameUtf16(db, i, nullptr);
        if (name_len > 0) {
            std::vector<wchar_t> name(name_len);
            SzArEx_GetFileNameUtf16(db, i, reinterpret_cast<UInt16*>(name.data()));
            e.path = tstring(name.data(), name_len - 1);  // 去掉结尾 \0
        }
        e.name = util::get_filename(e.path);

        e.size = fi->Size;
        e.is_directory = fi->IsDir != 0;

        // 时间
        CNtfsFileTime ft{};
        if (SzBitWithVals_Check(&db->MTime, i)) {
            ft.Low = db->MTime.Vals[i].Low;
            ft.High = db->MTime.Vals[i].High;
            e.modified = util::filetime_to_unix(*reinterpret_cast<FILETIME*>(&ft));
        }
        if (SzBitWithVals_Check(&db->CTime, i)) {
            ft.Low = db->CTime.Vals[i].Low;
            ft.High = db->CTime.Vals[i].High;
            e.created = util::filetime_to_unix(*reinterpret_cast<FILETIME*>(&ft));
        }
        if (SzBitWithVals_Check(&db->ATime, i)) {
            ft.Low = db->ATime.Vals[i].Low;
            ft.High = db->ATime.Vals[i].High;
            e.accessed = util::filetime_to_unix(*reinterpret_cast<FILETIME*>(&ft));
        }

        // 属性
        if (SzBitWithVals_Check(&db->Attribs, i)) {
            e.attributes = static_cast<u32>(db->Attribs.Vals[i]);
        }

        // 加密
        e.encrypted = SzArEx_IsFileEncrypted(db, i) != 0;

        // 压缩方法（7z 不直接暴露，统一为 LZMA2）
        e.method = CompressionMethod::Lzma2;

        // 压缩大小（7z 不易获取单个条目的压缩大小）
        e.compressed_size = 0;

        entries.push_back(std::move(e));
    }

    impl_->cached_entries = entries;
    impl_->entries_cached = true;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::extract_entry(u32 index,
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

    // 安全检查
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

    // 找到 7z 内的真实索引
    UInt32 real_index = 0;
    UInt32 count = 0;
    for (UInt32 i = 0; i < impl_->db.NumFiles; ++i) {
        if (impl_->db.Files[i].IsDir) continue;
        if (count == index) {
            real_index = i;
            break;
        }
        ++count;
    }

    // 解压
    size_t offset = 0;
    size_t out_size_processed = 0;

    impl_->progress_ctx.current_file = entry.path;
    impl_->progress_ctx.total = entry.size;
    impl_->progress_ctx.processed = 0;
    impl_->progress_ctx.cancelled = false;

    SRes res = SzArEx_Extract(&impl_->db, &impl_->look_stream.vt, real_index,
                               &impl_->block_index_start,
                               &impl_->out_buffer, &impl_->out_buffer_size,
                               &offset, &out_size_processed,
                               &g_Alloc, &g_AllocTemp,
                               SzProgressCallback, &impl_->progress_ctx);

    if (res == SZ_ERROR_PASSWORD) {
        tstring pwd = impl_->ask_password();
        if (pwd.empty()) return make_error_code(ArchiveError::Cancelled);
        impl_->password = pwd;
        std::string pwd_s = util::tstring_to_string(pwd);
        SzArEx_SetPassword(&impl_->db, pwd_s.c_str(), static_cast<size_t>(pwd_s.size()));

        res = SzArEx_Extract(&impl_->db, &impl_->look_stream.vt, real_index,
                             &impl_->block_index_start,
                             &impl_->out_buffer, &impl_->out_buffer_size,
                             &offset, &out_size_processed,
                             &g_Alloc, &g_AllocTemp,
                             SzProgressCallback, &impl_->progress_ctx);
    }

    if (res != SZ_OK) {
        return make_error_code(sz_to_archive_err(res));
    }

    // 写入文件
    HANDLE hOut = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) {
        return make_error_code(ArchiveError::WriteFailed);
    }

    DWORD written = 0;
    if (!WriteFile(hOut, impl_->out_buffer + offset,
                   static_cast<DWORD>(out_size_processed), &written, nullptr) ||
        written != static_cast<DWORD>(out_size_processed)) {
        CloseHandle(hOut);
        return make_error_code(ArchiveError::WriteFailed);
    }

    CloseHandle(hOut);

    // 设置时间
    if (opts.preserve_attributes) {
        FILETIME ft = util::unix_to_filetime(entry.modified);
        HANDLE h = CreateFile(output_path.c_str(), GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            SetFileTime(h, &ft, &ft, &ft);
            CloseHandle(h);
        }
        if (entry.attributes) {
            SetFileAttributes(output_path.c_str(),
                              util::archive_attr_to_win(entry.attributes));
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::test_entry(u32 index) {
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

    // 解压到内存但不写文件
    UInt32 real_index = 0;
    UInt32 count = 0;
    for (UInt32 i = 0; i < impl_->db.NumFiles; ++i) {
        if (impl_->db.Files[i].IsDir) continue;
        if (count == index) { real_index = i; break; }
        ++count;
    }

    size_t offset = 0;
    size_t out_size = 0;

    impl_->progress_ctx.current_file = entry.path;
    impl_->progress_ctx.total = entry.size;
    impl_->progress_ctx.cancelled = false;

    SRes res = SzArEx_Extract(&impl_->db, &impl_->look_stream.vt, real_index,
                               &impl_->block_index_start,
                               &impl_->out_buffer, &impl_->out_buffer_size,
                               &offset, &out_size,
                               &g_Alloc, &g_AllocTemp,
                               SzProgressCallback, &impl_->progress_ctx);

    if (res != SZ_OK) return make_error_code(sz_to_archive_err(res));

    // CRC 校验（如果归档中存储了 CRC）
    if (SzBitWithVals_Check(&impl_->db.CRCs, real_index)) {
        UInt32 expected = impl_->db.CRCs.Vals[real_index];
        UInt32 actual = CrcCalc(impl_->out_buffer + offset, out_size);
        if (expected != actual) {
            return make_error_code(ArchiveError::CRCMismatch);
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::extract_files(const std::vector<u32>& indices,
                                                const tstring& output_dir,
                                                const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!util::dir_exists(output_dir)) {
        if (!util::create_dir_recursive(output_dir)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    for (u32 idx : indices) {
        if (impl_->progress_ctx.cancelled) {
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
std::error_code SevenZipArchive::test() {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!impl_->entries_cached) {
        std::vector<ArchiveEntry> tmp;
        auto ec = read_entries(tmp);
        if (ec) return ec;
    }

    for (const auto& entry : impl_->cached_entries) {
        if (impl_->progress_ctx.cancelled) {
            return make_error_code(ArchiveError::Cancelled);
        }
        auto ec = test_entry(entry.index);
        if (ec) return ec;
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 创建归档（占位实现，实际由 7z 写入器处理）
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::create(const CreateOptions& opts) {
    // 7z 写入需要单独的写入器实现
    // 这里先记录选项，实际写入在 add_files 中进行
    impl_->path = opts.archive_path;
    impl_->password = opts.password;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;
    impl_->entries_cached = false;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 添加文件（占位实现）
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::add_files(const std::vector<tstring>& files,
                                            const CompressOptions& opts) {
    // 7z 写入需要使用 LZMA SDK 的写入接口
    // 完整实现需要约 500 行代码，这里给出框架
    // 实际生产代码请参考 7z SDK 中的 LzmaEnc.c 示例
    LOG_WARN(_T("7z write not fully implemented, please use ZIP format"));
    return make_error_code(ArchiveError::UnsupportedFeature);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code SevenZipArchive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
