// ============================================================================
// archive_rar.h - RAR 格式驱动（仅解压）
//
// 基于 unRAR 库实现 RAR 格式的读取。
// 注意：unRAR 许可证禁止用于创建 RAR 文件，因此本驱动只支持解压。
//
// 支持：RAR 4.x 和 RAR 5.0 格式
// 支持：分卷、密码、固实压缩、恢复记录
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "archive.h"
#include <memory>

namespace bandzip {

class RarArchive : public IArchive {
public:
    RarArchive();
    ~RarArchive() override;

    ArchiveFormat format() const override { return ArchiveFormat::Rar; }
    bool can_read() const override { return true; }
    bool can_write() const override { return false; }  // unRAR 许可证限制
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
