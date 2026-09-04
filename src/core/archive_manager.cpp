// ============================================================================
// archive_manager.cpp - 归档管理器实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "archive_manager.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <shlobj.h>
#include <algorithm>

namespace bandzip {

ArchiveManager& ArchiveManager::instance() {
    static ArchiveManager inst;
    return inst;
}

ArchiveManager::ArchiveManager() {
    load_config();
}

ArchiveManager::~ArchiveManager() {
    save_config();
}

std::unique_ptr<IArchive> ArchiveManager::open(const tstring& path,
                                                const tstring& password,
                                                std::error_code* ec) {
    LOG_INFO(_T("Opening archive: ") << path);

    auto archive = open_archive(path, password, ec);
    if (archive) {
        add_recent(path);
    }
    return archive;
}

std::unique_ptr<IArchive> ArchiveManager::create(const CreateOptions& opts,
                                                  std::error_code* ec) {
    LOG_INFO(_T("Creating archive: ") << opts.archive_path
              << _T(" format=") << static_cast<int>(opts.format));

    auto archive = create_archive(opts.format);
    if (!archive) {
        if (ec) *ec = make_error_code(ArchiveError::UnsupportedFeature);
        return nullptr;
    }

    auto err = archive->create(opts);
    if (err) {
        if (ec) *ec = err;
        return nullptr;
    }

    if (ec) *ec = {};
    add_recent(opts.archive_path);
    return archive;
}

void ArchiveManager::add_recent(const tstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 去重
    auto it = std::find(recent_.begin(), recent_.end(), path);
    if (it != recent_.end()) recent_.erase(it);

    recent_.push_front(path);
    while (recent_.size() > static_cast<size_t>(config_.max_recent)) {
        recent_.pop_back();
    }
}

void ArchiveManager::clear_recent() {
    std::lock_guard<std::mutex> lock(mutex_);
    recent_.clear();
}

void ArchiveManager::load_config() {
    // 配置文件路径：%APPDATA%\BandzipClone\config.ini
    tstring dir = util::get_appdata_dir() + _T("\\BandzipClone");
    if (!util::dir_exists(dir)) {
        util::create_dir_recursive(dir);
    }
    tstring path = dir + _T("\\config.ini");

    // 简单 INI 读取（避免依赖额外库）
    std::ifstream f(util::tstring_to_string(path));
    if (!f) {
        // 默认配置
        config_.default_extract_dir = util::get_desktop_dir();
        config_.default_compress_dir = util::get_desktop_dir();
        return;
    }

    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        if (key == "default_extract_dir")
            config_.default_extract_dir = util::string_to_tstring(val);
        else if (key == "default_compress_dir")
            config_.default_compress_dir = util::string_to_tstring(val);
        else if (key == "default_format")
            config_.default_format = static_cast<ArchiveFormat>(std::stoi(val));
        else if (key == "default_level")
            config_.default_level = std::stoi(val);
        else if (key == "delete_after_extract")
            config_.delete_after_extract = (val == "1");
        else if (key == "open_after_extract")
            config_.open_after_extract = (val == "1");
        else if (key == "show_password_dialog")
            config_.show_password_dialog = (val == "1");
        else if (key == "integrate_shell")
            config_.integrate_shell = (val == "1");
        else if (key == "associate_files")
            config_.associate_files = (val == "1");
        else if (key == "max_recent")
            config_.max_recent = std::stoi(val);
        else if (key == "recent") {
            tstring p = util::string_to_tstring(val);
            if (util::file_exists(p)) recent_.push_back(p);
        }
    }
}

void ArchiveManager::save_config() {
    tstring dir = util::get_appdata_dir() + _T("\\BandzipClone");
    if (!util::dir_exists(dir)) {
        util::create_dir_recursive(dir);
    }
    tstring path = dir + _T("\\config.ini");

    std::ofstream f(util::tstring_to_string(path));
    if (!f) return;

    f << "default_extract_dir=" << util::tstring_to_string(config_.default_extract_dir) << "\n";
    f << "default_compress_dir=" << util::tstring_to_string(config_.default_compress_dir) << "\n";
    f << "default_format=" << static_cast<int>(config_.default_format) << "\n";
    f << "default_level=" << config_.default_level << "\n";
    f << "delete_after_extract=" << (config_.delete_after_extract ? 1 : 0) << "\n";
    f << "open_after_extract=" << (config_.open_after_extract ? 1 : 0) << "\n";
    f << "show_password_dialog=" << (config_.show_password_dialog ? 1 : 0) << "\n";
    f << "integrate_shell=" << (config_.integrate_shell ? 1 : 0) << "\n";
    f << "associate_files=" << (config_.associate_files ? 1 : 0) << "\n";
    f << "max_recent=" << config_.max_recent << "\n";

    for (const auto& r : recent_) {
        f << "recent=" << util::tstring_to_string(r) << "\n";
    }
}

} // namespace bandzip
