// ============================================================================
// sfx.cpp - 自解压模块实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "sfx.h"
#include "../core/archive.h"
#include "../core/archive_manager.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>

namespace bandzip {
namespace sfx {

// ---------------------------------------------------------------------------
// SFX 标记
// ---------------------------------------------------------------------------
// SFX 文件末尾的标记，用于定位 SFX 元信息
// 8 字节 magic + 4 字节配置长度 + 配置数据 + 8 字节 magic
static const char SFX_MAGIC_BEGIN[8] = { 'B', 'Z', 'S', 'F', 'X', '0', '1', '\0' };
static const char SFX_MAGIC_END[8]   = { 'B', 'Z', 'S', 'F', 'X', 'E', 'N', 'D' };

// ---------------------------------------------------------------------------
// SFX 配置序列化/反序列化
// ---------------------------------------------------------------------------
static std::string SerializeConfig(const SfxConfig& config) {
    std::string s;
    auto write_str = [&s](const tstring& v) {
        std::string a = util::tstring_to_string(v);
        u32 len = static_cast<u32>(a.size());
        s.append(reinterpret_cast<const char*>(&len), 4);
        s += a;
    };
    auto write_bool = [&s](bool v) {
        u8 b = v ? 1 : 0;
        s.append(reinterpret_cast<const char*>(&b), 1);
    };

    write_str(config.title);
    write_str(config.default_dir);
    write_str(config.text);
    write_bool(config.overwrite);
    write_bool(config.auto_extract);
    write_bool(config.auto_run);
    write_str(config.run_command);
    write_bool(config.show_progress);
    write_bool(config.silent);
    write_bool(config.create_shortcut);
    write_str(config.shortcut_target);
    write_str(config.shortcut_name);

    return s;
}

static SfxConfig DeserializeConfig(const std::string& data) {
    SfxConfig config;
    if (data.empty()) return config;

    const char* p = data.data();
    const char* end = p + data.size();

    auto read_str = [p, end](tstring& v) mutable -> bool {
        if (p + 4 > end) return false;
        u32 len = *reinterpret_cast<const u32*>(p);
        p += 4;
        if (p + len > end) return false;
        v = util::string_to_tstring(std::string(p, len));
        p += len;
        return true;
    };
    auto read_bool = [p, end](bool& v) mutable -> bool {
        if (p + 1 > end) return false;
        v = *reinterpret_cast<const u8*>(p) != 0;
        p += 1;
        return true;
    };

    read_str(config.title);
    read_str(config.default_dir);
    read_str(config.text);
    read_bool(config.overwrite);
    read_bool(config.auto_extract);
    read_bool(config.auto_run);
    read_str(config.run_command);
    read_bool(config.show_progress);
    read_bool(config.silent);
    read_bool(config.create_shortcut);
    read_str(config.shortcut_target);
    read_str(config.shortcut_name);

    return config;
}

// ---------------------------------------------------------------------------
// 创建 SFX
// ---------------------------------------------------------------------------
std::error_code CreateSfx(const tstring& archive_path,
                          const tstring& sfx_path,
                          const SfxConfig& config) {
    // 1. 读取 SFX Stub
    tstring stub_path = util::get_module_dir() + _T("\\bandzip.sfx");
    std::ifstream stub_file(util::tstring_to_string(stub_path),
                             std::ios::binary);
    if (!stub_file) {
        return make_error_code(ArchiveError::FileNotFound);
    }

    std::vector<u8> stub_data((std::istreambuf_iterator<char>(stub_file)),
                                std::istreambuf_iterator<char>());
    stub_file.close();

    // 2. 读取压缩包数据
    std::ifstream arch_file(util::tstring_to_string(archive_path),
                             std::ios::binary);
    if (!arch_file) {
        return make_error_code(ArchiveError::OpenFailed);
    }

    std::vector<u8> arch_data((std::istreambuf_iterator<char>(arch_file)),
                                std::istreambuf_iterator<char>());
    arch_file.close();

    // 3. 序列化配置
    std::string config_data = SerializeConfig(config);
    u32 config_len = static_cast<u32>(config_data.size());

    // 4. 写入 SFX 文件
    std::ofstream sfx_file(util::tstring_to_string(sfx_path),
                            std::ios::binary | std::ios::trunc);
    if (!sfx_file) {
        return make_error_code(ArchiveError::WriteFailed);
    }

    // 写入 Stub
    sfx_file.write(reinterpret_cast<const char*>(stub_data.data()),
                   stub_data.size());

    // 写入压缩包数据
    sfx_file.write(reinterpret_cast<const char*>(arch_data.data()),
                   arch_data.size());

    // 写入配置
    sfx_file.write(SFX_MAGIC_BEGIN, 8);
    sfx_file.write(reinterpret_cast<const char*>(&config_len), 4);
    sfx_file.write(config_data.data(), config_data.size());
    sfx_file.write(SFX_MAGIC_END, 8);

    sfx_file.close();

    LOG_INFO(_T("SFX created: ") << sfx_path
              << _T(" (stub=") << stub_data.size()
              << _T(" archive=") << arch_data.size()
              << _T(" config=") << config_data.size() << _T(")"));

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 检查是否为 SFX
// ---------------------------------------------------------------------------
bool IsSfx(const tstring& path) {
    std::ifstream f(util::tstring_to_string(path), std::ios::binary);
    if (!f) return false;

    // 检查文件末尾是否有 SFX 标记
    f.seekg(-8, std::ios::end);
    char magic[8];
    f.read(magic, 8);
    if (!f) return false;

    return std::memcmp(magic, SFX_MAGIC_END, 8) == 0;
}

// ---------------------------------------------------------------------------
// 从 SFX 提取压缩包
// ---------------------------------------------------------------------------
std::error_code ExtractSfxArchive(const tstring& sfx_path,
                                   const tstring& output_path) {
    std::ifstream f(util::tstring_to_string(sfx_path), std::ios::binary);
    if (!f) return make_error_code(ArchiveError::OpenFailed);

    // 读取末尾标记
    f.seekg(-8, std::ios::end);
    char magic_end[8];
    f.read(magic_end, 8);
    if (!f || std::memcmp(magic_end, SFX_MAGIC_END, 8) != 0) {
        return make_error_code(ArchiveError::BadFormat);
    }

    // 读取配置长度
    f.seekg(-12, std::ios::end);
    u32 config_len = 0;
    f.read(reinterpret_cast<char*>(&config_len), 4);
    if (!f) return make_error_code(ArchiveError::ReadFailed);

    // 读取开始标记
    f.seekg(-(12 + config_len + 8), std::ios::end);
    char magic_begin[8];
    f.read(magic_begin, 8);
    if (!f || std::memcmp(magic_begin, SFX_MAGIC_BEGIN, 8) != 0) {
        return make_error_code(ArchiveError::BadFormat);
    }

    // 计算压缩包数据范围
    std::streamoff archive_end = -(12 + config_len + 8);

    // 读取 Stub 大小（从 PE 头解析）
    // 简化：直接读取整个文件，然后截取
    f.seekg(0, std::ios::end);
    std::streamoff total = f.tellg();
    std::streamoff archive_size = total + archive_end;

    // 读取 Stub 大小（从 PE 头）
    f.seekg(0, std::ios::beg);
    // DOS 头
    u16 e_magic = 0;
    f.read(reinterpret_cast<char*>(&e_magic), 2);
    if (e_magic != 0x5A4D) {  // "MZ"
        return make_error_code(ArchiveError::BadFormat);
    }

    f.seekg(60, std::ios::beg);  // e_lfanew
    u32 pe_offset = 0;
    f.read(reinterpret_cast<char*>(&pe_offset), 4);

    f.seekg(pe_offset, std::ios::beg);
    u32 pe_magic = 0;
    f.read(reinterpret_cast<char*>(&pe_magic), 4);
    if (pe_magic != 0x00004550) {  // "PE\0\0"
        return make_error_code(ArchiveError::BadFormat);
    }

    // COFF 头
    u16 num_sections = 0;
    f.seekg(pe_offset + 6, std::ios::beg);
    f.read(reinterpret_cast<char*>(&num_sections), 2);

    // 可选头大小
    u16 opt_header_size = 0;
    f.seekg(pe_offset + 20, std::ios::beg);
    f.read(reinterpret_cast<char*>(&opt_header_size), 2);

    // 节表起始
    std::streamoff section_start = pe_offset + 24 + opt_header_size;

    // 遍历节，找到最大的 PointerToRawData + SizeOfRawData
    u32 stub_size = 0;
    for (u16 i = 0; i < num_sections; ++i) {
        f.seekg(section_start + i * 40 + 16, std::ios::beg);
        u32 raw_size = 0;
        f.read(reinterpret_cast<char*>(&raw_size), 4);
        u32 raw_ptr = 0;
        f.read(reinterpret_cast<char*>(&raw_ptr), 4);

        u32 end = raw_ptr + raw_size;
        if (end > stub_size) stub_size = end;
    }

    f.close();

    // 提取压缩包数据
    std::ifstream in(util::tstring_to_string(sfx_path), std::ios::binary);
    in.seekg(stub_size, std::ios::beg);

    std::ofstream out(util::tstring_to_string(output_path),
                       std::ios::binary | std::ios::trunc);

    const size_t BUF = 65536;
    std::vector<char> buf(BUF);
    std::streamoff remaining = archive_size - stub_size;

    while (remaining > 0) {
        size_t to_read = static_cast<size_t>(
            std::min< std::streamoff>(remaining, BUF));
        in.read(buf.data(), to_read);
        out.write(buf.data(), to_read);
        remaining -= to_read;
    }

    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// 获取 SFX 配置
// ---------------------------------------------------------------------------
std::error_code GetSfxConfig(const tstring& sfx_path, SfxConfig& config) {
    std::ifstream f(util::tstring_to_string(sfx_path), std::ios::binary);
    if (!f) return make_error_code(ArchiveError::OpenFailed);

    f.seekg(-8, std::ios::end);
    char magic_end[8];
    f.read(magic_end, 8);
    if (!f || std::memcmp(magic_end, SFX_MAGIC_END, 8) != 0) {
        return make_error_code(ArchiveError::BadFormat);
    }

    f.seekg(-12, std::ios::end);
    u32 config_len = 0;
    f.read(reinterpret_cast<char*>(&config_len), 4);
    if (!f) return make_error_code(ArchiveError::ReadFailed);

    f.seekg(-(12 + config_len), std::ios::end);
    std::string config_data(config_len, '\0');
    f.read(&config_data[0], config_len);

    config = DeserializeConfig(config_data);
    return make_error_code(ArchiveError::Ok);
}

// ---------------------------------------------------------------------------
// SFX Stub 运行时入口
// ---------------------------------------------------------------------------
int SfxMain() {
    // 1. 获取自身路径
    tchar self_path[MAX_PATH];
    GetModuleFileName(nullptr, self_path, MAX_PATH);

    // 2. 读取配置
    SfxConfig config;
    std::error_code ec = GetSfxConfig(self_path, config);
    if (ec) {
        MessageBox(nullptr, _T("无效的 SFX 文件"), _T("错误"), MB_ICONERROR);
        return 1;
    }

    // 3. 提取压缩包到临时文件
    tstring temp_dir = util::create_temp_dir(_T("bzsfx"));
    tstring temp_archive = temp_dir + _T("\\archive.tmp");
    ec = ExtractSfxArchive(self_path, temp_archive);
    if (ec) {
        MessageBox(nullptr, _T("无法提取压缩包数据"), _T("错误"), MB_ICONERROR);
        return 1;
    }

    // 4. 解压
    tstring dest_dir = config.default_dir;
    if (dest_dir.empty()) {
        // 询问用户
        BROWSEINFO bi = {};
        bi.hwndOwner = nullptr;
        bi.lpszTitle = config.title.c_str();
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDL pidl = SHBrowseForFolder(&bi);
        if (!pidl) return 0;
        tchar buf[MAX_PATH];
        SHGetPathFromIDList(pidl, buf);
        dest_dir = buf;
        CoTaskMemFree(pidl);
    }

    auto archive = ArchiveManager::instance().open(temp_archive,
                                                     config.silent ? _T("") : _T(""),
                                                     &ec);
    if (!archive) {
        MessageBox(nullptr, _T("无法打开压缩包"), _T("错误"), MB_ICONERROR);
        return 1;
    }

    ExtractOptions opts;
    opts.overwrite = config.overwrite ? OverwriteMode::All : OverwriteMode::Ask;
    opts.keep_broken = false;
    opts.create_dir = true;
    opts.silent = config.silent;

    std::vector<u32> indices;
    auto entries = archive->entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        indices.push_back(static_cast<u32>(i));
    }

    ec = archive->extract_files(indices, dest_dir, opts);
    if (ec) {
        MessageBox(nullptr, _T("解压失败"), _T("错误"), MB_ICONERROR);
        return 1;
    }

    // 5. 创建快捷方式
    if (config.create_shortcut && !config.shortcut_target.empty()) {
        tstring desktop = util::get_desktop_dir();
        tstring shortcut_path = desktop + _T("\\") +
            (config.shortcut_name.empty() ? _T("BandzipClone") : config.shortcut_name) +
            _T(".lnk");

        // 使用 IShellLink
        IShellLink* psl;
        CoInitialize(nullptr);
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr,
                                         CLSCTX_INPROC_SERVER,
                                         IID_IShellLink,
                                         reinterpret_cast<void**>(&psl)))) {
            tstring target = util::join_path(dest_dir, config.shortcut_target);
            psl->SetPath(target.c_str());
            psl->SetWorkingDirectory(dest_dir.c_str());

            IPersistFile* ppf;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile,
                                                reinterpret_cast<void**>(&ppf)))) {
                ppf->Save(shortcut_path.c_str(), TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();
    }

    // 6. 自动运行
    if (config.auto_run && !config.run_command.empty()) {
        tstring cmd = util::join_path(dest_dir, config.run_command);
        ShellExecute(nullptr, _T("open"), cmd.c_str(), nullptr,
                     dest_dir.c_str(), SW_SHOWNORMAL);
    }

    // 7. 打开目标文件夹
    if (!config.silent) {
        util::shell_open(dest_dir);
    }

    // 8. 清理临时文件
    util::delete_dir(temp_dir, true);

    return 0;
}

} // namespace sfx
} // namespace bandzip
