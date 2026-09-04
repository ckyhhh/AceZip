// ============================================================================
// archive_gz.h - GZ 单文件压缩驱动
//
// GZ 是 gzip 格式，只能压缩单个文件。
// 本驱动将其包装为 IArchive 接口，使其在 UI 中表现为单条目归档。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "archive.h"
#include <memory>

namespace bandzip {

class GzArchive : public IArchive {
public:
    GzArchive();
    ~GzArchive() override;

    ArchiveFormat format() const override { return ArchiveFormat::Gzip; }
    bool can_read() const override { return true; }
    bool can_write() const override { return true; }
    bool can_encrypt() const override { return false; }
    bool can_solid() const override { return false; }
    bool can_volume() const override { return false; }

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
