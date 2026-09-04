// ============================================================================
// archive.cpp - 归档抽象接口实现（错误码、格式映射、工厂）
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive.h"
#include "archive_zip.h"
#include "archive_7z.h"
#include "archive_rar.h"
#include "archive_tar.h"
#include "archive_gz.h"
#include "archive_bz2.h"
#include "archive_xz.h"
#include "archive_zstd.h"
#include "archive_lz4.h"
#include "archive_cab.h"
#include "codec_detector.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace bandzip {

// ---------------------------------------------------------------------------
// 错误类别
// ---------------------------------------------------------------------------
class ArchiveErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "bandzip.archive";
    }

    std::string message(int ev) const override {
        switch (static_cast<ArchiveError>(ev)) {
        case ArchiveError::Ok:                 return "成功";
        case ArchiveError::OpenFailed:         return "无法打开文件";
        case ArchiveError::ReadFailed:         return "读取失败";
        case ArchiveError::WriteFailed:        return "写入失败";
        case ArchiveError::BadFormat:          return "格式错误或损坏";
        case ArchiveError::UnsupportedMethod:  return "不支持的压缩方法";
        case ArchiveError::WrongPassword:      return "密码错误";
        case ArchiveError::PasswordRequired:   return "需要密码";
        case ArchiveError::Truncated:          return "文件被截断";
        case ArchiveError::CRCMismatch:        return "CRC 校验失败";
        case ArchiveError::DiskFull:           return "磁盘空间不足";
        case ArchiveError::Cancelled:          return "操作已取消";
        case ArchiveError::FileNotFound:       return "文件不存在";
        case ArchiveError::InvalidParameter:   return "参数无效";
        case ArchiveError::InternalError:      return "内部错误";
        case ArchiveError::UnsupportedFeature: return "不支持的功能";
        }
        return "未知错误";
    }
};

const std::error_category& archive_category() {
    static ArchiveErrorCategory cat;
    return cat;
}

std::error_code make_error_code(ArchiveError e) {
    return {static_cast<int>(e), archive_category()};
}

// ---------------------------------------------------------------------------
// 格式与扩展名
// ---------------------------------------------------------------------------
struct FormatInfo {
    ArchiveFormat format;
    const tchar* display_name;
    const tchar* extension;
    bool can_write;
    bool can_encrypt;
    bool can_volume;
    bool can_solid;
    const u8* magic;
    size_t magic_len;
};

// 各格式的魔数（用于检测）
static const u8 kZipMagic[]   = {0x50, 0x4B, 0x03, 0x04};
static const u8 kZipEmpty[]   = {0x50, 0x4B, 0x05, 0x06};
static const u8 kZipSpan[]    = {0x50, 0x4B, 0x07, 0x08};
static const u8 k7zMagic[]     = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
static const u8 kRar4Magic[]   = {'R', 'a', 'r', '!', 0x1A, 0x07, 0x00};
static const u8 kRar5Magic[]   = {'R', 'a', 'r', '!', 0x1A, 0x07, 0x01, 0x00};
static const u8 kGzipMagic[]   = {0x1F, 0x8B};
static const u8 kBzip2Magic[]  = {'B', 'Z', 'h'};
static const u8 kXzMagic[]     = {0xFD, '7', 'z', 'X', 'Z', 0x00};
static const u8 kZstdMagic[]   = {0x28, 0xB5, 0x2F, 0xFD};
static const u8 kLz4Magic[]     = {0x04, 0x22, 0x4D, 0x18};
static const u8 kCabMagic[]    = {'M', 'S', 'C', 'F'};
static const u8 kTarUstar[]    = {'u', 's', 't', 'a', 'r'};  // 偏移 257

static const FormatInfo kFormatTable[] = {
    {ArchiveFormat::Zip,      _T("ZIP"),     _T("zip"),   true,  true,  true,  false, kZipMagic,  4},
    {ArchiveFormat::SevenZip, _T("7Z"),      _T("7z"),    true,  true,  true,  true,  k7zMagic,   6},
    {ArchiveFormat::Rar,      _T("RAR"),     _T("rar"),   false, true,  true,  true,  kRar5Magic, 8},
    {ArchiveFormat::Tar,      _T("TAR"),     _T("tar"),   true,  false, false, false, nullptr,    0},
    {ArchiveFormat::Gzip,     _T("GZIP"),    _T("gz"),    true,  false, false, false, kGzipMagic, 2},
    {ArchiveFormat::Bzip2,    _T("BZIP2"),   _T("bz2"),   true,  false, false, false, kBzip2Magic,3},
    {ArchiveFormat::Xz,       _T("XZ"),      _T("xz"),    true,  false, false, false, kXzMagic,   6},
    {ArchiveFormat::Zstd,     _T("Zstandard"),_T("zst"),  true,  false, false, false, kZstdMagic, 4},
    {ArchiveFormat::Lz4,      _T("LZ4"),     _T("lz4"),   true,  false, false, false, kLz4Magic,  4},
    {ArchiveFormat::Cab,      _T("CAB"),     _T("cab"),   false, false, true,  false, kCabMagic,  4},
    {ArchiveFormat::Wim,      _T("WIM"),     _T("wim"),   false, false, false, false, nullptr,    0},
    {ArchiveFormat::Iso,      _T("ISO"),     _T("iso"),   false, false, false, false, nullptr,    0},
    {ArchiveFormat::Arj,      _T("ARJ"),     _T("arj"),   false, false, false, false, nullptr,    0},
    {ArchiveFormat::Lzh,      _T("LZH"),     _T("lzh"),   false, false, false, false, nullptr,    0},
};

static const FormatInfo* find_format_info(ArchiveFormat fmt) {
    for (const auto& info : kFormatTable) {
        if (info.format == fmt) return &info;
    }
    return nullptr;
}

const tchar* format_to_extension(ArchiveFormat fmt) {
    if (auto* info = find_format_info(fmt)) return info->extension;
    return _T("");
}

const tchar* format_display_name(ArchiveFormat fmt) {
    if (auto* info = find_format_info(fmt)) return info->display_name;
    return _T("未知");
}

ArchiveFormat extension_to_format(const tstring& path) {
    tstring ext = util::get_extension_lower(path);
    if (ext.empty()) return ArchiveFormat::Unknown;

    // 处理复合扩展名 .tgz / .tbz2 / .txz / .tzst / .tlz4
    if (ext == _T("tgz"))   return ArchiveFormat::Gzip;
    if (ext == _T("tbz2") || ext == _T("tbz")) return ArchiveFormat::Bzip2;
    if (ext == _T("txz"))   return ArchiveFormat::Xz;
    if (ext == _T("tzst"))  return ArchiveFormat::Zstd;
    if (ext == _T("tlz4"))  return ArchiveFormat::Lz4;

    for (const auto& info : kFormatTable) {
        if (ext == info.extension) return info.format;
    }
    return ArchiveFormat::Unknown;
}

// ---------------------------------------------------------------------------
// 工厂
// ---------------------------------------------------------------------------
std::unique_ptr<IArchive> create_archive(ArchiveFormat fmt) {
    switch (fmt) {
    case ArchiveFormat::Zip:      return std::make_unique<ZipArchive>();
    case ArchiveFormat::SevenZip: return std::make_unique<SevenZipArchive>();
    case ArchiveFormat::Rar:       return std::make_unique<RarArchive>();
    case ArchiveFormat::Tar:      return std::make_unique<TarArchive>();
    case ArchiveFormat::Gzip:     return std::make_unique<GzipArchive>();
    case ArchiveFormat::Bzip2:    return std::make_unique<Bzip2Archive>();
    case ArchiveFormat::Xz:       return std::make_unique<XzArchive>();
    case ArchiveFormat::Zstd:     return std::make_unique<ZstdArchive>();
    case ArchiveFormat::Lz4:      return std::make_unique<Lz4Archive>();
    case ArchiveFormat::Cab:      return std::make_unique<CabArchive>();
    default: return nullptr;
    }
}

std::unique_ptr<IArchive> open_archive(const tstring& path,
                                       const tstring& password,
                                       std::error_code* ec) {
    // 1. 优先按扩展名猜测
    ArchiveFormat fmt = extension_to_format(path);

    // 2. 用魔数验证
    ArchiveFormat detected = util::detect_format(path);
    if (detected != ArchiveFormat::Unknown) {
        fmt = detected;
    }

    if (fmt == ArchiveFormat::Unknown) {
        if (ec) *ec = make_error_code(ArchiveError::BadFormat);
        return nullptr;
    }

    auto archive = create_archive(fmt);
    if (!archive) {
        if (ec) *ec = make_error_code(ArchiveError::UnsupportedFeature);
        return nullptr;
    }

    auto err = archive->open(path, password);
    if (err) {
        if (ec) *ec = err;
        return nullptr;
    }

    if (ec) *ec = {};
    return archive;
}

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------
namespace util {

ArchiveFormat detect_format(const tstring& path) {
    std::ifstream f(util::tstring_to_string(path), std::ios::binary);
    if (!f) return ArchiveFormat::Unknown;

    u8 header[512] = {};
    f.read(reinterpret_cast<char*>(header), sizeof(header));
    auto read_len = static_cast<size_t>(f.gcount());

    if (read_len < 4) return ArchiveFormat::Unknown;

    // 检查各格式魔数
    auto match = [&](const u8* m, size_t n) {
        return read_len >= n && std::memcmp(header, m, n) == 0;
    };

    if (match(k7zMagic, sizeof(k7zMagic)))   return ArchiveFormat::SevenZip;
    if (match(kRar5Magic, sizeof(kRar5Magic))) return ArchiveFormat::Rar;
    if (match(kRar4Magic, sizeof(kRar4Magic))) return ArchiveFormat::Rar;
    if (match(kZipMagic, sizeof(kZipMagic)) ||
        match(kZipEmpty, sizeof(kZipEmpty)) ||
        match(kZipSpan, sizeof(kZipSpan)))   return ArchiveFormat::Zip;
    if (match(kGzipMagic, sizeof(kGzipMagic))) return ArchiveFormat::Gzip;
    if (match(kBzip2Magic, sizeof(kBzip2Magic))) return ArchiveFormat::Bzip2;
    if (match(kXzMagic, sizeof(kXzMagic)))    return ArchiveFormat::Xz;
    if (match(kZstdMagic, sizeof(kZstdMagic))) return ArchiveFormat::Zstd;
    if (match(kLz4Magic, sizeof(kLz4Magic)))  return ArchiveFormat::Lz4;
    if (match(kCabMagic, sizeof(kCabMagic)))  return ArchiveFormat::Cab;

    // TAR：魔数在偏移 257
    if (read_len >= 262 &&
        std::memcmp(header + 257, kTarUstar, sizeof(kTarUstar)) == 0) {
        return ArchiveFormat::Tar;
    }

    // TAR（旧格式无 ustar 魔数）：检查字段合理性
    if (read_len >= 512) {
        // 简单启发：文件名字段非空、size 字段为八进制
        bool looks_like_tar = true;
        for (int i = 0; i < 100 && header[i]; ++i) {
            if (header[i] < 0x20 || header[i] > 0x7E) {
                looks_like_tar = false;
                break;
            }
        }
        // size 字段（124-135）应为八进制
        if (looks_like_tar) {
            for (int i = 124; i < 134; ++i) {
                u8 c = header[i];
                if (c != 0 && c != ' ' && (c < '0' || c > '7')) {
                    looks_like_tar = false;
                    break;
                }
            }
        }
        if (looks_like_tar) return ArchiveFormat::Tar;
    }

    return ArchiveFormat::Unknown;
}

std::vector<tstring> get_extensions(ArchiveFormat fmt) {
    switch (fmt) {
    case ArchiveFormat::Zip:      return {_T("zip"), _T("zipx"), _T("jar"), _T("apk"), _T("ipa")};
    case ArchiveFormat::SevenZip: return {_T("7z")};
    case ArchiveFormat::Rar:      return {_T("rar")};
    case ArchiveFormat::Tar:      return {_T("tar")};
    case ArchiveFormat::Gzip:     return {_T("gz"), _T("gzip"), _T("tgz"), _T("tpz")};
    case ArchiveFormat::Bzip2:    return {_T("bz2"), _T("bzip2"), _T("tbz2"), _T("tbz")};
    case ArchiveFormat::Xz:       return {_T("xz"), _T("txz")};
    case ArchiveFormat::Zstd:     return {_T("zst"), _T("zstd"), _T("tzst")};
    case ArchiveFormat::Lz4:      return {_T("lz4"), _T("tlz4")};
    case ArchiveFormat::Cab:      return {_T("cab")};
    case ArchiveFormat::Wim:      return {_T("wim")};
    case ArchiveFormat::Iso:      return {_T("iso")};
    case ArchiveFormat::Arj:      return {_T("arj")};
    case ArchiveFormat::Lzh:      return {_T("lzh"), _T("lha")};
    default: return {};
    }
}

bool format_supports_write(ArchiveFormat fmt) {
    if (auto* info = find_format_info(fmt)) return info->can_write;
    return false;
}

bool format_supports_encryption(ArchiveFormat fmt) {
    if (auto* info = find_format_info(fmt)) return info->can_encrypt;
    return false;
}

bool format_supports_volumes(ArchiveFormat fmt) {
    if (auto* info = find_format_info(fmt)) return info->can_volume;
    return false;
}

bool format_supports_solid(ArchiveFormat fmt) {
    if (auto* info = find_format_info(fmt)) return info->can_solid;
    return false;
}

std::vector<CompressionMethod> get_supported_methods(ArchiveFormat fmt) {
    switch (fmt) {
    case ArchiveFormat::Zip:
        return {CompressionMethod::Copy, CompressionMethod::Deflate,
                CompressionMethod::Deflate64, CompressionMethod::Bzip2,
                CompressionMethod::Lzma, CompressionMethod::Zstd};
    case ArchiveFormat::SevenZip:
        return {CompressionMethod::Copy, CompressionMethod::Lzma,
                CompressionMethod::Lzma2, CompressionMethod::Ppmd,
                CompressionMethod::Bzip2, CompressionMethod::Zstd};
    case ArchiveFormat::Tar:
        return {CompressionMethod::Copy};
    case ArchiveFormat::Gzip:
        return {CompressionMethod::Deflate};
    case ArchiveFormat::Bzip2:
        return {CompressionMethod::Bzip2};
    case ArchiveFormat::Xz:
        return {CompressionMethod::Lzma2};
    case ArchiveFormat::Zstd:
        return {CompressionMethod::Zstd};
    case ArchiveFormat::Lz4:
        return {CompressionMethod::Lz4};
    default:
        return {};
    }
}

} // namespace util

} // namespace bandzip
