// ============================================================================
// archive_manager.h - 归档管理器
//
// 单例，管理当前打开的归档、最近文件、配置等。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "archive.h"
#include <memory>
#include <mutex>
#include <vector>
#include <deque>

namespace bandzip {

class ArchiveManager {
public:
    static ArchiveManager& instance();

    // 打开归档
    std::unique_ptr<IArchive> open(const tstring& path,
                                   const tstring& password = tstring(),
                                   std::error_code* ec = nullptr);

    // 创建新归档
    std::unique_ptr<IArchive> create(const CreateOptions& opts,
                                      std::error_code* ec = nullptr);

    // 最近文件
    const std::deque<tstring>& recent_files() const { return recent_; }
    void add_recent(const tstring& path);
    void clear_recent();

    // 全局密码回调（UI 层设置）
    void set_password_callback(PasswordCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        password_cb_ = std::move(cb);
    }
    PasswordCallback password_callback() const { return password_cb_; }

    // 全局进度回调
    void set_progress_callback(ProgressCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_cb_ = std::move(cb);
    }
    ProgressCallback progress_callback() const { return progress_cb_; }

    // 配置
    struct Config {
        tstring default_extract_dir;
        tstring default_compress_dir;
        ArchiveFormat default_format = ArchiveFormat::Zip;
        int default_level = 5;
        bool delete_after_extract = false;
        bool open_after_extract = true;
        bool show_password_dialog = true;
        bool integrate_shell = true;
        bool associate_files = true;
        int max_recent = 20;
    };

    const Config& config() const { return config_; }
    void set_config(const Config& c) { config_ = c; }

    void load_config();
    void save_config();

private:
    ArchiveManager();
    ~ArchiveManager();
    ArchiveManager(const ArchiveManager&) = delete;
    ArchiveManager& operator=(const ArchiveManager&) = delete;

    mutable std::mutex mutex_;
    std::deque<tstring> recent_;
    PasswordCallback password_cb_;
    ProgressCallback progress_cb_;
    Config config_;
};

} // namespace bandzip
