// ============================================================================
// archive_zip.h - ZIP 格式驱动
//
// 基于 minizip-ng 实现 ZIP 格式的读写。
// 支持：Store/Deflate/Deflate64/Bzip2/LZMA/Zstd 压缩方法
// 支持：ZipCrypto/AES-128/AES-192/AES-256 加密
// 支持：文件名编码自动检测（解决中文乱码）
// 支持：ZIP64（大文件）
// 支持：分卷
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "archive.h"
#include <memory>

namespace bandzip {

class ZipArchive : public IArchive {
public:
    ZipArchive();
    ~ZipArchive() override;

    // IArchive 接口
    ArchiveFormat format() const override { return ArchiveFormat::Zip; }
    bool can_read() const override { return true; }
    bool can_write() const override { return true; }
    bool can_encrypt() const override { return true; }
    bool can_solid() const override { return false; }
    bool can_volume() const override { return true; }

    std::error_code open(const tstring& path,
                         const tstring& password,
                         OpenMode mode) override;
    std::error_code close() override;
    bool is_open() const override;

    std::error_code read_entries(std::vector<ArchiveEntry>& entries) override;
    std::error_code extract_entry(u32 index,
                                  const tstring& output_path,
                                  const ExtractOptions& opts) override;
    std::error_code test_entry(u32 index) override;
    std::error_code extract_files(const std::vector<u32>& indices,
                                  const tstring& output_dir,
                                  const ExtractOptions& opts) override;
    std::error_code test() override;

    std::error_code create(const CreateOptions& opts) override;
    std::error_code add_files(const std::vector<tstring>& files,
                              const CompressOptions& opts) override;
    std::error_code delete_entries(const std::vector<u32>& indices) override;

    // ZIP 特有
    std::error_code set_comment(const tstring& comment);
    tstring comment() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bandzip
