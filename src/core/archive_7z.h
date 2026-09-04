// ============================================================================
// archive_7z.h - 7z 格式驱动
//
// 基于 LZMA SDK 实现 7z 格式的读写。
// 支持：LZMA/LZMA2/PPMd/Bzip2/Zstd/Deflate 压缩方法
// 支持：AES-256 加密（含文件名加密）
// 支持：固实压缩
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

class SevenZipArchive : public IArchive {
public:
    SevenZipArchive();
    ~SevenZipArchive() override;

    ArchiveFormat format() const override { return ArchiveFormat::SevenZip; }
    bool can_read() const override { return true; }
    bool can_write() const override { return true; }
    bool can_encrypt() const override { return true; }
    bool can_solid() const override { return true; }
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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bandzip
