// ============================================================================
// command_line.cpp - 命令行参数解析实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "command_line.h"
#include "../core/archive_manager.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <atomic>

namespace bandzip {
namespace app {

// ---------------------------------------------------------------------------
// 全局状态（用于回调）
// ---------------------------------------------------------------------------
static std::atomic<bool> g_cancelled{false};

// ---------------------------------------------------------------------------
// 解析
// ---------------------------------------------------------------------------
bool CommandLine::Parse(int argc, tchar* argv[]) {
    if (argc < 2) {
        return true;  // 无参数，启动 GUI
    }

    for (int i = 1; i < argc; ++i) {
        tstring arg = argv[i];

        // 命令（第一个非选项参数）
        if (command.empty() && arg[0] != _T('-') && arg[0] != _T('/')) {
            command = arg;
            continue;
        }

        // 选项
        if (arg[0] == _T('-') || arg[0] == _T('/')) {
            if (!ParseOption(arg)) {
                return false;
            }
            continue;
        }

        // 文件参数
        files.push_back(arg);
    }

    // 如果没有命令但有文件，则第一个文件是命令（打开归档）
    if (command.empty() && !files.empty()) {
        command = files[0];
        files.erase(files.begin());
    }

    return true;
}

// ---------------------------------------------------------------------------
// 解析选项
// ---------------------------------------------------------------------------
bool CommandLine::ParseOption(const tstring& opt) {
    tstring lower = util::to_lower(opt);

    if (lower == _T("-y") || lower == _T("/y")) {
        yes = true;
    } else if (lower == _T("-silent") || lower == _T("/silent") ||
               lower == _T("-s")) {
        silent = true;
    } else if (lower == _T("-solid")) {
        solid = true;
    } else if (lower == _T("-encrypt-names")) {
        encrypt_names = true;
    } else if (lower == _T("-delete-after")) {
        delete_after = true;
    } else if (lower == _T("-open-after")) {
        open_after = true;
    } else if (lower == _T("-no-overwrite")) {
        overwrite = false;
    } else if (lower == _T("-keep-broken")) {
        keep_broken = true;
    } else if (lower == _T("--help") || lower == _T("-h") ||
               lower == _T("/?")) {
        command = _T("--help");
    } else if (lower == _T("--version")) {
        command = _T("--version");
    } else if (lower == _T("--register")) {
        command = _T("--register");
    } else if (lower == _T("--unregister")) {
        command = _T("--unregister");
    } else if (lower == _T("--register-shell")) {
        command = _T("--register-shell");
    } else if (lower == _T("--unregister-shell")) {
        command = _T("--unregister-shell");
    } else if (lower == _T("--extract-dialog")) {
        command = _T("--extract-dialog");
    } else if (lower == _T("--compress-dialog")) {
        command = _T("--compress-dialog");
    } else if (lower.substr(0, 2) == _T("-o")) {
        output_dir = opt.substr(2);
    } else if (lower.substr(0, 2) == _T("-p")) {
        password = opt.substr(2);
    } else if (lower.substr(0, 5) == _T("-fmt:")) {
        format = opt.substr(5);
    } else if (lower.substr(0, 7) == _T("-level:")) {
        level = std::stoi(opt.substr(7));
    } else if (lower.substr(0, 2) == _T("-v")) {
        // 分卷大小：-v100m, -v1g, -v500k
        tstring s = opt.substr(2);
        if (!s.empty()) {
            tchar unit = s.back();
            tstring num = s;
            u64 mult = 1;
            if (unit == _T('k') || unit == _T('K')) {
                mult = 1024;
                num = s.substr(0, s.length() - 1);
            } else if (unit == _T('m') || unit == _T('M')) {
                mult = 1024 * 1024;
                num = s.substr(0, s.length() - 1);
            } else if (unit == _T('g') || unit == _T('G')) {
                mult = 1024 * 1024 * 1024;
                num = s.substr(0, s.length() - 1);
            }
            try {
                volume_size = std::stoull(num) * mult;
            } catch (...) {
                PrintError(_T("Invalid volume size: ") + s);
                return false;
            }
        }
    } else if (lower.substr(0, 5) == _T("-log:")) {
        log_file = opt.substr(5);
    } else if (lower.substr(0, 9) == _T("-encoding")) {
        encoding = opt.substr(9);
    } else {
        PrintError(_T("Unknown option: ") + opt);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// 解压
// ---------------------------------------------------------------------------
int CommandLine::Extract() {
    if (files.empty()) {
        PrintError(_T("No archive specified"));
        return 1;
    }

    tstring archive_path = files[0];
    if (!util::file_exists(archive_path)) {
        PrintError(_T("File not found: ") + archive_path);
        return 1;
    }

    // 输出目录
    tstring dest_dir = output_dir;
    if (dest_dir.empty()) {
        // 默认解压到当前目录
        dest_dir = util::get_current_dir();
    }
    if (!util::dir_exists(dest_dir)) {
        util::create_dir_recursive(dest_dir);
    }

    Print(_T("Extracting: ") + archive_path);
    Print(_T("To: ") + dest_dir);

    // 设置进度回调
    ArchiveManager::instance().set_progress_callback(
        [](const ProgressInfo& info) {
            if (!g_cancelled) {
                tstring s = util::format(_T("\r%s  %d%%  %s / %s"),
                    info.current_file.c_str(),
                    info.percent,
                    util::format_size(info.bytes_processed).c_str(),
                    util::format_size(info.bytes_total).c_str());
                _tprintf(_T("%s"), s.c_str());
                if (info.cancelled) g_cancelled = true;
            }
        });

    // 打开归档
    std::error_code ec;
    auto archive = ArchiveManager::instance().open(archive_path, password, &ec);
    if (!archive) {
        PrintError(_T("Failed to open archive: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    // 读取条目
    std::vector<ArchiveEntry> entries;
    ec = archive->read_entries(entries);
    if (ec) {
        PrintError(_T("Failed to read entries: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    Print(_T("Found ") + util::format_count(entries.size()) + _T(" entries"));

    // 解压
    ExtractOptions opts;
    opts.overwrite = overwrite;
    opts.keep_broken = keep_broken;
    opts.skip_existing = !overwrite;
    opts.encoding = encoding;

    std::vector<u32> indices;
    for (size_t i = 1; i < files.size(); ++i) {
        // 后续文件参数为要解压的条目名
        // 这里简化处理：解压全部
    }

    ec = archive->extract_files({}, dest_dir, opts);
    _tprintf(_T("\n"));
    if (ec) {
        PrintError(_T("Extraction failed: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    Print(_T("Done."));
    return 0;
}

// ---------------------------------------------------------------------------
// 压缩
// ---------------------------------------------------------------------------
int CommandLine::Compress() {
    if (files.empty()) {
        PrintError(_T("No files specified"));
        return 1;
    }

    // 第一个文件是归档路径
    archive_path = files[0];
    std::vector<tstring> input_files(files.begin() + 1, files.end());

    if (input_files.empty()) {
        PrintError(_T("No input files specified"));
        return 1;
    }

    // 检测格式
    ArchiveFormat fmt = DetectFormat(archive_path);

    Print(_T("Creating: ") + archive_path);
    Print(_T("Format: ") + util::archive_format_to_string(fmt));
    Print(_T("Level: ") + std::to_wstring(level));

    // 创建归档
    CreateOptions copts;
    copts.archive_path = archive_path;
    copts.format = fmt;
    copts.level = level;
    copts.password = password;
    copts.solid = solid;
    copts.encrypt_names = encrypt_names;
    copts.volume_size = volume_size;

    std::error_code ec;
    auto archive = create_archive(fmt);
    if (!archive) {
        PrintError(_T("Failed to create archive"));
        return 1;
    }

    ec = archive->create(copts);
    if (ec) {
        PrintError(_T("Failed to create archive: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    // 设置进度回调
    ArchiveManager::instance().set_progress_callback(
        [](const ProgressInfo& info) {
            if (!g_cancelled) {
                tstring s = util::format(_T("\r%s  %d%%"),
                    info.current_file.c_str(),
                    info.percent);
                _tprintf(_T("%s"), s.c_str());
                if (info.cancelled) g_cancelled = true;
            }
        });

    // 添加文件
    CompressOptions opts;
    opts.level = level;
    opts.method = CompressionMethod::Default;
    opts.solid = solid;
    opts.password = password;
    opts.encrypt_names = encrypt_names;
    opts.delete_after = delete_after;

    ec = archive->add_files(input_files, opts);
    _tprintf(_T("\n"));
    if (ec) {
        PrintError(_T("Compression failed: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    archive->close();

    Print(_T("Done."));

    if (open_after) {
        util::shell_open(archive_path);
    }

    return 0;
}

// ---------------------------------------------------------------------------
// 测试
// ---------------------------------------------------------------------------
int CommandLine::Test() {
    if (files.empty()) {
        PrintError(_T("No archive specified"));
        return 1;
    }

    tstring archive_path = files[0];
    if (!util::file_exists(archive_path)) {
        PrintError(_T("File not found: ") + archive_path);
        return 1;
    }

    Print(_T("Testing: ") + archive_path);

    std::error_code ec;
    auto archive = ArchiveManager::instance().open(archive_path, password, &ec);
    if (!archive) {
        PrintError(_T("Failed to open archive: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    // 设置进度回调
    ArchiveManager::instance().set_progress_callback(
        [](const ProgressInfo& info) {
            if (!g_cancelled) {
                tstring s = util::format(_T("\r%s  %d%%"),
                    info.current_file.c_str(),
                    info.percent);
                _tprintf(_T("%s"), s.c_str());
                if (info.cancelled) g_cancelled = true;
            }
        });

    ec = archive->test();
    _tprintf(_T("\n"));
    if (ec) {
        PrintError(_T("Test failed: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    Print(_T("All OK."));
    return 0;
}

// ---------------------------------------------------------------------------
// 列出
// ---------------------------------------------------------------------------
int CommandLine::List() {
    if (files.empty()) {
        PrintError(_T("No archive specified"));
        return 1;
    }

    tstring archive_path = files[0];
    if (!util::file_exists(archive_path)) {
        PrintError(_T("File not found: ") + archive_path);
        return 1;
    }

    std::error_code ec;
    auto archive = ArchiveManager::instance().open(archive_path, password, &ec);
    if (!archive) {
        PrintError(_T("Failed to open archive: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    std::vector<ArchiveEntry> entries;
    ec = archive->read_entries(entries);
    if (ec) {
        PrintError(_T("Failed to read entries: ") +
                   util::error_code_to_string(ec));
        return 1;
    }

    // 打印表头
    _tprintf(_T("%-40s %12s %12s %20s %10s\n"),
             _T("Name"), _T("Size"), _T("Packed"), _T("Modified"), _T("CRC"));
    _tprintf(_T("%-40s %12s %12s %20s %10s\n"),
             _T("----"), _T("----"), _T("------"), _T("--------"), _T("---"));

    u64 total_size = 0;
    u64 total_packed = 0;
    for (const auto& e : entries) {
        _tprintf(_T("%-40s %12s %12s %20s %08X\n"),
                 e.path.c_str(),
                 util::format_size(e.size).c_str(),
                 util::format_size(e.packed_size).c_str(),
                 util::format_time(e.modified_time).c_str(),
                 e.crc32);
        total_size += e.size;
        total_packed += e.packed_size;
    }

    _tprintf(_T("\n"));
    _tprintf(_T("Total: %llu files, %s (%s packed)\n"),
             static_cast<u64>(entries.size()),
             util::format_size(total_size).c_str(),
             util::format_size(total_packed).c_str());

    return 0;
}

// ---------------------------------------------------------------------------
// 帮助
// ---------------------------------------------------------------------------
void CommandLine::PrintHelp() const {
    _tprintf(_T("BandzipClone 0.9.0 - Open-source Bandizip-like archiver\n"));
    _tprintf(_T("Copyright (C) 2024 BandzipClone Contributors, AGPLv3\n"));
    _tprintf(_T("\n"));
    _tprintf(_T("Usage:\n"));
    _tprintf(_T("  bandzip.exe [archive]                    Browse archive\n"));
    _tprintf(_T("  bandzip.exe x [archive] [options]        Extract\n"));
    _tprintf(_T("  bandzip.exe a [archive] [files...] [opt] Compress\n"));
    _tprintf(_T("  bandzip.exe t [archive]                  Test\n"));
    _tprintf(_T("  bandzip.exe l [archive]                  List\n"));
    _tprintf(_T("\n"));
    _tprintf(_T("Options:\n"));
    _tprintf(_T("  -o<dir>          Output directory\n"));
    _tprintf(_T("  -p<password>     Archive password\n"));
    _tprintf(_T("  -fmt:<format>    Output format (zip|7z|tar|gz|bz2|xz|zstd|lz4)\n"));
    _tprintf(_T("  -level:<0-9>     Compression level\n"));
    _tprintf(_T("  -v<size>         Volume size (e.g. -v100m, -v1g)\n"));
    _tprintf(_T("  -y                Answer yes to all prompts\n"));
    _tprintf(_T("  -silent           Silent mode (no UI)\n"));
    _tprintf(_T("  -solid            Solid compression\n"));
    _tprintf(_T("  -encrypt-names    Encrypt file names\n"));
    _tprintf(_T("  -delete-after     Delete files after compression\n"));
    _tprintf(_T("  -open-after       Open archive after compression\n"));
    _tprintf(_T("  -no-overwrite     Do not overwrite existing files\n"));
    _tprintf(_T("  -keep-broken      Keep broken files\n"));
    _tprintf(_T("  -log:<file>       Log to file\n"));
    _tprintf(_T("  -encoding:<enc>   File name encoding (auto|utf8|gbk|big5|sjis|euckr)\n"));
    _tprintf(_T("\n"));
    _tprintf(_T("Commands:\n"));
    _tprintf(_T("  --extract-dialog [archive]   Show extract dialog\n"));
    _tprintf(_T("  --compress-dialog [files...] Show compress dialog\n"));
    _tprintf(_T("  --register                   Register file associations\n"));
    _tprintf(_T("  --unregister                 Unregister file associations\n"));
    _tprintf(_T("  --register-shell             Register shell extension\n"));
    _tprintf(_T("  --unregister-shell           Unregister shell extension\n"));
    _tprintf(_T("  --help                       Show this help\n"));
    _tprintf(_T("  --version                    Show version\n"));
}

// ---------------------------------------------------------------------------
// 检测格式
// ---------------------------------------------------------------------------
ArchiveFormat CommandLine::DetectFormat(const tstring& path) const {
    if (format != _T("zip")) {
        if (format == _T("7z"))  return ArchiveFormat::SevenZip;
        if (format == _T("tar")) return ArchiveFormat::Tar;
        if (format == _T("gz"))  return ArchiveFormat::Gzip;
        if (format == _T("bz2")) return ArchiveFormat::Bzip2;
        if (format == _T("xz"))  return ArchiveFormat::Xz;
        if (format == _T("zstd")) return ArchiveFormat::Zstd;
        if (format == _T("lz4")) return ArchiveFormat::Lz4;
    }
    return util::detect_format(path);
}

// ---------------------------------------------------------------------------
// 输出
// ---------------------------------------------------------------------------
void CommandLine::Print(const tstring& msg) const {
    if (!silent) {
        _tprintf(_T("%s\n"), msg.c_str());
    }
}

void CommandLine::PrintError(const tstring& msg) const {
    _tprintf(_T("Error: %s\n"), msg.c_str());
}

} // namespace app
} // namespace bandzip
