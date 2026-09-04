// ============================================================================
// archive_cab.cpp - CAB 格式驱动完整实现
//
// 基于 Microsoft Cabinet SDK（FDI/FCI）实现 CAB 格式的读写。
// FDI（File Decompression Interface）用于解压，FCI（File Compression
// Interface）用于压缩。两者均通过回调函数操作。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_cab.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <fci.h>
#include <fdi.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <vector>
#include <map>

namespace bandzip {

// ---------------------------------------------------------------------------
// FDI 上下文
// ---------------------------------------------------------------------------
struct FdiContext {
    std::vector<ArchiveEntry> entries;
    tstring current_dir;
    tstring current_file;
    u32 current_index = 0;
    INT_PTR current_handle = -1;
    ProgressCallback progress_cb;
    std::atomic<bool> cancelled{false};
    u64 current_total = 0;
    u64 current_processed = 0;
};

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct CabArchive::Impl {
    tstring path;
    tstring password;
    OpenMode mode = OpenMode::Closed;
    bool is_open = false;

    std::vector<ArchiveEntry> cached_entries;
    bool entries_cached = false;

    ProgressCallback progress_cb;
    std::atomic<bool> cancelled{false};
    tstring current_file;

    FdiContext fdi_ctx;

    // FCI 写入上下文
    HFCI fci_handle = nullptr;
    ERF fci_erf = {};
    tstring cab_name;
    tstring cab_path;
    std::vector<tstring> files_to_add;
};

// ---------------------------------------------------------------------------
// FDI 内存分配回调
// ---------------------------------------------------------------------------
static FNALLOC(fdi_alloc) {
    return malloc(cb);
}

static FNFREE(fdi_free) {
    free(pv);
}

// ---------------------------------------------------------------------------
// FDI 文件 I/O 回调
// ---------------------------------------------------------------------------
static FNOPEN(fdi_open) {
    DWORD access = 0;
    DWORD share = 0;
    DWORD create = 0;

    if (oflag & _O_RDWR) {
        access = GENERIC_READ | GENERIC_WRITE;
    } else if (oflag & _O_WRONLY) {
        access = GENERIC_WRITE;
    } else {
        access = GENERIC_READ;
    }

    if (oflag & _O_CREAT) {
        create = CREATE_ALWAYS;
    } else {
        create = OPEN_EXISTING;
    }

    share = FILE_SHARE_READ;

    HANDLE h = CreateFileA(pszFile, access, share, nullptr,
                            create, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    return reinterpret_cast<INT_PTR>(h);
}

static FNREAD(fdi_read) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    DWORD read = 0;
    if (!ReadFile(h, pv, cb, &read, nullptr)) return -1;
    return static_cast<UINT>(read);
}

static FNWRITE(fdi_write) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    DWORD written = 0;
    if (!WriteFile(h, pv, cb, &written, nullptr)) return -1;
    return static_cast<UINT>(written);
}

static FNCLOSE(fdi_close) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    return CloseHandle(h) ? 0 : -1;
}

static FNSEEK(fdi_seek) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    DWORD method = FILE_BEGIN;
    if (seektype == SEEK_CUR) method = FILE_CURRENT;
    else if (seektype == SEEK_END) method = FILE_END;

    LONG high = static_cast<LONG>(dist >> 32);
    DWORD low = SetFilePointer(h, static_cast<LONG>(dist & 0xFFFFFFFF),
                                &high, method);
    if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        return -1;
    }
    return (static_cast<ULONG64>(high) << 32) | low;
}

// ---------------------------------------------------------------------------
// FDI 通知回调
// ---------------------------------------------------------------------------
static FNFDINOTIFY(fdi_notify) {
    FdiContext* ctx = reinterpret_cast<FdiContext*>(pfdin->pv);

    switch (fdint) {
    case fdintCABINET_INFO:
        // CAB 信息
        return 0;

    case fdintPARTIAL_FILE:
        // 部分文件（跨卷）
        return 0;

    case fdintCOPY_FILE: {
        // 准备解压文件
        ArchiveEntry e{};
        e.index = ctx->current_index++;
        e.name = util::ansi_to_tstring(pfdin->pszFile);
        e.path = e.name;
        e.size = pfdin->cb;
        e.compressed_size = pfdin->cb;  // 实际压缩大小未知
        e.modified = static_cast<std::time_t>(pfdin->date);
        e.is_directory = false;
        e.method = CompressionMethod::MsZip;

        ctx->entries.push_back(e);
        ctx->current_file = e.path;
        ctx->current_total = pfdin->cb;
        ctx->current_processed = 0;

        // 创建输出文件
        tstring out_path = util::join_path(ctx->current_dir,
                                            util::ansi_to_tstring(pfdin->pszFile));
        tstring parent = util::get_dirname(out_path);
        if (!parent.empty() && !util::dir_exists(parent)) {
            util::create_dir_recursive(parent);
        }

        std::string out = util::tstring_to_ansi(out_path);
        int hf = _open(out.c_str(), _O_BINARY | _O_CREAT | _O_WRONLY | _O_TRUNC,
                       _S_IREAD | _S_IWRITE);
        return hf;
    }

    case fdintCLOSE_FILE_INFO: {
        // 文件解压完成
        HANDLE h = reinterpret_cast<HANDLE>(pfdin->hf);
        CloseHandle(h);

        // 设置文件时间
        tstring out_path = util::join_path(ctx->current_dir,
                                            util::ansi_to_tstring(pfdin->pszFile));
        if (ctx->progress_cb) {
            ProgressInfo info{};
            info.current_file = ctx->current_file;
            info.bytes_processed = ctx->current_total;
            info.bytes_total = ctx->current_total;
            info.percent = 100;
            info.cancelled = ctx->cancelled.load();
            ctx->progress_cb(info);
            ctx->cancelled = info.cancelled;
        }

        return TRUE;
    }

    case fdintNEXT_CABINET:
        // 下一卷
        return 0;

    case fdintENUMERATE:
        // 枚举
        return 0;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
CabArchive::CabArchive() : impl_(std::make_unique<Impl>()) {}
CabArchive::~CabArchive() { close(); }

// ---------------------------------------------------------------------------
// 打开
// ---------------------------------------------------------------------------
std::error_code CabArchive::open(const tstring& path,
                                    const tstring& password,
                                    OpenMode mode) {
    if (mode != OpenMode::Read) {
        return make_error_code(ArchiveError::UnsupportedFeature);
    }

    impl_->path = path;
    impl_->password = password;
    impl_->mode = mode;

    // 创建 FDI 上下文
    ERF erf = {};
    HFDI hfdi = FDICreate(fdi_alloc, fdi_free, fdi_open, fdi_read,
                           fdi_write, fdi_close, fdi_seek,
                           cpuUNKNOWN, &erf);
    if (!hfdi) {
        return make_error_code(ArchiveError::InternalError);
    }

    // 枚举所有文件（不实际解压）
    impl_->fdi_ctx.entries.clear();
    impl_->fdi_ctx.current_index = 0;
    impl_->fdi_ctx.current_dir = util::create_temp_dir(_T("bz_cab"));

    std::string cab_path = util::tstring_to_ansi(path);
    std::string cab_name = util::tstring_to_ansi(util::get_filename(path));

    BOOL ok = FDICopy(hfdi, const_cast<char*>(cab_name.c_str()),
                       const_cast<char*>(util::tstring_to_ansi(
                           util::get_dirname(path)).c_str()),
                       0, fdi_notify, nullptr, &impl_->fdi_ctx);

    if (!ok) {
        FDIDestroy(hfdi);
        // 清理临时目录
        util::delete_dir(impl_->fdi_ctx.current_dir, true);
        return make_error_code(ArchiveError::BadFormat);
    }

    FDIDestroy(hfdi);

    // 缓存条目
    impl_->cached_entries = impl_->fdi_ctx.entries;
    impl_->entries_cached = true;
    impl_->is_open = true;

    // 清理临时目录
    util::delete_dir(impl_->fdi_ctx.current_dir, true);

    return make_error_code(ArchiveError::Ok);
}

std::error_code CabArchive::close() {
    impl_->is_open = false;
    impl_->mode = OpenMode::Closed;
    impl_->entries_cached = false;
    impl_->cached_entries.clear();
    return make_error_code(ArchiveError::Ok);
}

bool CabArchive::is_open() const {
    return impl_->is_open;
}

// ---------------------------------------------------------------------------
// 读取条目
// ---------------------------------------------------------------------------
std::error_code CabArchive::read_entries(std::vector<ArchiveEntry>& entries) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    entries = impl_->cached_entries;
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 解压单个条目
// ---------------------------------------------------------------------------
std::error_code CabArchive::extract_entry(u32 index,
                                             const tstring& output_path,
                                             const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index >= impl_->cached_entries.size()) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    // 创建 FDI 上下文
    ERF erf = {};
    HFDI hfdi = FDICreate(fdi_alloc, fdi_free, fdi_open, fdi_read,
                           fdi_write, fdi_close, fdi_seek,
                           cpuUNKNOWN, &erf);
    if (!hfdi) {
        return make_error_code(ArchiveError::InternalError);
    }

    // 设置目标目录
    impl_->fdi_ctx.current_dir = util::get_dirname(output_path);
    impl_->fdi_ctx.current_index = 0;
    impl_->fdi_ctx.entries.clear();
    impl_->fdi_ctx.progress_cb = impl_->progress_cb;
    impl_->fdi_ctx.cancelled = false;

    // 创建目标目录
    if (!util::dir_exists(impl_->fdi_ctx.current_dir)) {
        util::create_dir_recursive(impl_->fdi_ctx.current_dir);
    }

    std::string cab_path = util::tstring_to_ansi(impl_->path);
    std::string cab_name = util::tstring_to_ansi(util::get_filename(impl_->path));

    BOOL ok = FDICopy(hfdi, const_cast<char*>(cab_name.c_str()),
                       const_cast<char*>(util::tstring_to_ansi(
                           util::get_dirname(impl_->path)).c_str()),
                       0, fdi_notify, nullptr, &impl_->fdi_ctx);

    FDIDestroy(hfdi);

    if (!ok) {
        return make_error_code(ArchiveError::BadFormat);
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试单个条目
// ---------------------------------------------------------------------------
std::error_code CabArchive::test_entry(u32 index) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    if (index >= impl_->cached_entries.size()) {
        return make_error_code(ArchiveError::FileNotFound);
    }
    // CAB 测试通过解压到临时目录实现
    tstring temp = util::create_temp_dir(_T("bz_cab_test"));
    auto ec = extract_entry(index, temp, ExtractOptions{});
    util::delete_dir(temp, true);
    return ec;
}

// ---------------------------------------------------------------------------
// 批量解压
// ---------------------------------------------------------------------------
std::error_code CabArchive::extract_files(const std::vector<u32>& indices,
                                              const tstring& output_dir,
                                              const ExtractOptions& opts) {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);

    if (!util::dir_exists(output_dir)) {
        if (!util::create_dir_recursive(output_dir)) {
            return make_error_code(ArchiveError::WriteFailed);
        }
    }

    // CAB 一次解压所有文件
    ERF erf = {};
    HFDI hfdi = FDICreate(fdi_alloc, fdi_free, fdi_open, fdi_read,
                           fdi_write, fdi_close, fdi_seek,
                           cpuUNKNOWN, &erf);
    if (!hfdi) {
        return make_error_code(ArchiveError::InternalError);
    }

    impl_->fdi_ctx.current_dir = output_dir;
    impl_->fdi_ctx.current_index = 0;
    impl_->fdi_ctx.entries.clear();
    impl_->fdi_ctx.progress_cb = impl_->progress_cb;
    impl_->fdi_ctx.cancelled = false;

    std::string cab_path = util::tstring_to_ansi(impl_->path);
    std::string cab_name = util::tstring_to_ansi(util::get_filename(impl_->path));

    BOOL ok = FDICopy(hfdi, const_cast<char*>(cab_name.c_str()),
                       const_cast<char*>(util::tstring_to_ansi(
                           util::get_dirname(impl_->path)).c_str()),
                       0, fdi_notify, nullptr, &impl_->fdi_ctx);

    FDIDestroy(hfdi);

    if (!ok) {
        return make_error_code(ArchiveError::BadFormat);
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 测试整个归档
// ---------------------------------------------------------------------------
std::error_code CabArchive::test() {
    if (!is_open()) return make_error_code(ArchiveError::OpenFailed);
    tstring temp = util::create_temp_dir(_T("bz_cab_test"));
    auto ec = extract_files({}, temp, ExtractOptions{});
    util::delete_dir(temp, true);
    return ec;
}

// ---------------------------------------------------------------------------
// FCI 回调（写入）
// ---------------------------------------------------------------------------
static void* FCI_API fci_alloc(ULONG cb) {
    return malloc(cb);
}

static void FCI_API fci_free(void* pv) {
    free(pv);
}

static INT_PTR FCI_API fci_open(char* pszFile, int oflag, int pmode,
                                  int* err, void* pv) {
    DWORD access = 0;
    DWORD share = FILE_SHARE_READ;
    DWORD create = 0;

    if (oflag & _O_RDWR) {
        access = GENERIC_READ | GENERIC_WRITE;
    } else if (oflag & _O_WRONLY) {
        access = GENERIC_WRITE;
    } else {
        access = GENERIC_READ;
    }

    if (oflag & _O_CREAT) {
        create = CREATE_ALWAYS;
    } else {
        create = OPEN_EXISTING;
    }

    HANDLE h = CreateFileA(pszFile, access, share, nullptr,
                            create, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        *err = GetLastError();
        return -1;
    }
    return reinterpret_cast<INT_PTR>(h);
}

static UINT FCI_API fci_read(INT_PTR hf, void* memory, UINT cb,
                              int* err, void* pv) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    DWORD read = 0;
    if (!ReadFile(h, memory, cb, &read, nullptr)) {
        *err = GetLastError();
        return static_cast<UINT>(-1);
    }
    return static_cast<UINT>(read);
}

static UINT FCI_API fci_write(INT_PTR hf, void* memory, UINT cb,
                               int* err, void* pv) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    DWORD written = 0;
    if (!WriteFile(h, memory, cb, &written, nullptr)) {
        *err = GetLastError();
        return static_cast<UINT>(-1);
    }
    return static_cast<UINT>(written);
}

static int FCI_API fci_close(INT_PTR hf, int* err, void* pv) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    if (!CloseHandle(h)) {
        *err = GetLastError();
        return -1;
    }
    return 0;
}

static long FCI_API fci_seek(INT_PTR hf, long dist, int seektype,
                               int* err, void* pv) {
    HANDLE h = reinterpret_cast<HANDLE>(hf);
    DWORD method = FILE_BEGIN;
    if (seektype == SEEK_CUR) method = FILE_CURRENT;
    else if (seektype == SEEK_END) method = FILE_END;

    DWORD low = SetFilePointer(h, dist, nullptr, method);
    if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        *err = GetLastError();
        return -1;
    }
    return static_cast<long>(low);
}

static int FCI_API fci_delete(char* pszFile, int* err, void* pv) {
    if (!DeleteFileA(pszFile)) {
        *err = GetLastError();
        return -1;
    }
    return 0;
}

static BOOL FCI_API fci_get_temp(char* pszTempName, int cbTempName,
                                   int* err, void* pv) {
    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);
    char temp_file[MAX_PATH];
    if (!GetTempFileNameA(temp_path, "bz_", 0, temp_file)) {
        *err = GetLastError();
        return FALSE;
    }
    strncpy_s(pszTempName, cbTempName, temp_file, _TRUNCATE);
    return TRUE;
}

static BOOL FCI_API fci_get_next_cab(PCCAB pccab, ULONG cbPrevCab,
                                       int* err, void* pv) {
    // 不分卷
    return FALSE;
}

static INT_PTR FCI_API fci_status(UINT typeStatus, ULONG cb1,
                                     ULONG cb2, int* err, void* pv) {
    return 0;
}

static INT_PTR FCI_API fci_get_open_info(char* pszName, USHORT* pdate,
                                            USHORT* ptime, USHORT* pattribs,
                                            int* err, void* pv) {
    HANDLE h = CreateFileA(pszName, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        *err = GetLastError();
        return -1;
    }

    // 获取文件时间
    FILETIME ft;
    GetFileTime(h, nullptr, nullptr, &ft);
    std::time_t t = util::filetime_to_unix(ft);
    u16 dos_date, dos_time;
    util::unix_to_dostime(t, dos_date, dos_time);
    *pdate = dos_date;
    *ptime = dos_time;

    // 获取属性
    BY_HANDLE_FILE_INFORMATION info;
    GetFileInformationByHandle(h, &info);
    *pattribs = static_cast<USHORT>(info.dwFileAttributes);

    return reinterpret_cast<INT_PTR>(h);
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
std::error_code CabArchive::create(const CreateOptions& opts) {
    impl_->path = opts.archive_path;
    impl_->mode = OpenMode::Write;
    impl_->is_open = true;

    CCAB cab_params = {};
    cab_params.cb = opts.volume_size > 0 ? opts.volume_size : 0;
    cab_params.cbFolderThresh = 0;  // 默认

    // 设置 CAB 名称
    std::string name = util::tstring_to_ansi(util::get_filename(opts.archive_path));
    strncpy_s(cab_params.szCabName, sizeof(cab_params.szCabName),
              name.c_str(), _TRUNCATE);

    // 设置 CAB 路径
    std::string dir = util::tstring_to_ansi(util::get_dirname(opts.archive_path));
    strncpy_s(cab_params.szCabPath, sizeof(cab_params.szCabPath),
              dir.c_str(), _TRUNCATE);

    // 设置磁盘名称
    strcpy_s(cab_params.szDiskName, "BandzipClone");

    impl_->fci_handle = FCICreate(&impl_->fci_erf,
        fci_file_placed,          // 占位
        fci_alloc, fci_free,
        fci_open, fci_read, fci_write, fci_close, fci_seek, fci_delete,
        fci_get_temp, fci_get_next_cab, fci_status,
        fci_get_open_info,
        cab_params, impl_.get());

    if (!impl_->fci_handle) {
        return make_error_code(ArchiveError::InternalError);
    }

    return make_error_code(ArchiveError::Ok);
}

// 占位函数（FCI 需要）
static BOOL FCI_API fci_file_placed(PCCAB pccab, char* pszFile,
                                       LONG cbFile, BOOL fContinuation,
                                       void* pv) {
    return TRUE;
}

// ---------------------------------------------------------------------------
// 添加文件
// ---------------------------------------------------------------------------
std::error_code CabArchive::add_files(const std::vector<tstring>& files,
                                         const CompressOptions& opts) {
    if (!is_open() || !impl_->fci_handle) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    for (const auto& file : files) {
        if (impl_->cancelled) {
            return make_error_code(ArchiveError::Cancelled);
        }

        if (util::dir_exists(file)) {
            // 递归添加目录
            util::walk_dir(file, [this, &file, &opts](const util::FindData& fd) {
                if (fd.is_directory) return true;
                tstring rel = util::make_relative(file, fd.path);
                std::string src = util::tstring_to_ansi(fd.path);
                std::string dst = util::tstring_to_ansi(rel);
                if (!FCIAddFile(impl_->fci_handle,
                                 const_cast<char*>(src.c_str()),
                                 const_cast<char*>(dst.c_str()),
                                 FALSE,
                                 fci_get_next_cab, fci_status,
                                 fci_get_open_info,
                                 opts.level >= 7 ? tcompTYPE_MSZIP :
                                                  tcompTYPE_NONE)) {
                    return false;
                }
                return true;
            }, true);
        } else {
            // 添加单个文件
            std::string src = util::tstring_to_ansi(file);
            std::string dst = util::tstring_to_ansi(util::get_filename(file));
            if (!FCIAddFile(impl_->fci_handle,
                             const_cast<char*>(src.c_str()),
                             const_cast<char*>(dst.c_str()),
                             FALSE,
                             fci_get_next_cab, fci_status,
                             fci_get_open_info,
                             opts.level >= 7 ? tcompTYPE_MSZIP : tcompTYPE_NONE)) {
                return make_error_code(ArchiveError::WriteFailed);
            }
        }
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 删除条目
// ---------------------------------------------------------------------------
std::error_code CabArchive::delete_entries(const std::vector<u32>& indices) {
    return make_error_code(ArchiveError::UnsupportedFeature);
}

} // namespace bandzip
