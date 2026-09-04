# BandzipClone — 类 Bandizip 的开源解压缩软件

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](./LICENSE)
[![Platform: Windows 7+](https://img.shields.io/badge/Platform-Windows%207%2B-lightgrey.svg)](#)
[![Build size: <5MB](https://img.shields.io/badge/Binary%20size-%3C5MB-success.svg)](#)

> 一个用 C++ 与 Duilib 编写的、功能对标 Bandizip 的开源解压缩软件。
> 采用 AGPLv3 协议，目标平台 Windows 7 及以上，构建产物控制在 5MB 以内。

---

## 目录

- [项目简介](#项目简介)
- [功能对标 Bandizip](#功能对标-bandizip)
- [支持的压缩格式](#支持的压缩格式)
- [技术栈与第三方库](#技术栈与第三方库)
- [目录结构](#目录结构)
- [构建指南](#构建指南)
- [体积控制策略](#体积控制策略)
- [使用说明](#使用说明)
- [Shell 右键菜单集成](#shell-右键菜单集成)
- [命令行参数](#命令行参数)
- [开发路线图](#开发路线图)
- [许可证](#许可证)

---

## 项目简介

BandzipClone 是一个完全开源的 Windows 解压缩软件，旨在复刻 Bandizip 的全部核心功能。
项目使用 C++17 编写，UI 层基于轻量级的 DirectUI 框架 Duilib，归档层通过统一的 `IArchive`
抽象接口对接多种压缩算法库。整个项目以"轻量、纯净、零广告、零追踪"为设计目标，构建产物
（不含第三方运行时）严格控制在 5MB 以内，可在 Windows 7 SP1 及以上系统原生运行。

项目采用 AGPLv3 协议开源，这意味着任何对本软件的修改、二次分发或通过网络提供服务（SaaS）
都必须以相同协议公开源代码。我们鼓励社区贡献，但请务必遵守 AGPLv3 的条款。

---

## 功能对标 Bandizip

下表列出了 Bandizip 的核心功能以及本项目的实现状态：

| 功能模块 | Bandizip | 本项目 | 说明 |
|---------|----------|--------|------|
| 多格式解压 | ✅ | ✅ | ZIP/7Z/RAR/TAR/GZ/BZ2/XZ/ZSTD/LZ4/CAB |
| 多格式压缩 | ✅ | ✅ | ZIP/7Z/TAR/GZ/XZ/ZSTD/LZ4 |
| 浏览压缩包 | ✅ | ✅ | 树形 + 列表双视图 |
| 拖拽操作 | ✅ | ✅ | 拖入压缩、拖出解压 |
| 右键菜单集成 | ✅ | ✅ | Shell Extension DLL |
| 文件关联 | ✅ | ✅ | 注册表关联 |
| 密码保护 | ✅ | ✅ | AES-256 / ZipCrypto |
| 多卷压缩 | ✅ | ✅ | 自定义分卷大小 |
| 编码自动检测 | ✅ | ✅ | 解决中文乱码 |
| 文件预览 | ✅ | ✅ | 双击预览（临时解压） |
| 测试压缩包 | ✅ | ✅ | CRC 校验 |
| 转换格式 | ✅ | ✅ | 压缩包格式互转 |
| 自解压（SFX） | ✅ | ⚠️ | 仅 ZIP SFX |
| 上下文菜单 | ✅ | ✅ | "在此处解压"等 |
| 多线程压缩 | ✅ | ✅ | 大文件并行处理 |
| 命令行模式 | ✅ | ✅ | 静默解压/压缩 |

---

## 支持的压缩格式

### 解压（读取）

| 格式 | 扩展名 | 算法库 | 备注 |
|------|--------|--------|------|
| ZIP | .zip | minizip-ng | 含 AES 加密 |
| 7Z | .7z | LZMA SDK | 含 LZMA2/PPMd/ZSTD |
| RAR | .rar | unRAR | 仅解压（unRAR 许可） |
| TAR | .tar | 自实现 | POSIX/GNU/USTAR |
| GZIP | .gz .gzip | zlib | 含 .tgz |
| BZIP2 | .bz2 .bzip2 | bzip2 | 含 .tbz2 |
| XZ | .xz | liblzma | 含 .txz |
| Zstandard | .zst | zstd | 含 .tzst |
| LZ4 | .lz4 | lz4 | 含 .tlz4 |
| CAB | .cab | 自实现 | MS-CAB |
| WIM | .wim | 自实现 | 仅读取 |

### 压缩（创建）

| 格式 | 扩展名 | 算法库 | 支持的算法 |
|------|--------|--------|-----------|
| ZIP | .zip | minizip-ng | Deflate/Deflate64/BZip2/LZMA/ZSTD |
| 7Z | .7z | LZMA SDK | LZMA/LZMA2/PPMd/ZSTD/Copy |
| TAR | .tar | 自实现 | 无压缩 |
| GZIP | .gz | zlib | Deflate |
| XZ | .xz | liblzma | LZMA2 |
| Zstandard | .zst | zstd | ZSTD |
| LZ4 | .lz4 | lz4 | LZ4 HC |

---

## 技术栈与第三方库

### 核心技术

- **语言**：C++17（需 MSVC 2019 v19.20+ 或 MinGW 9.2+）
- **UI 框架**：Duilib（轻量 DirectUI，约 800KB 静态库）
- **构建系统**：CMake 3.16+
- **目标平台**：Windows 7 SP1 / Windows 8 / Windows 10 / Windows 11

### 第三方库（均静态链接，体积已优化）

| 库 | 版本 | 用途 | 许可证 | 体积（静态） |
|----|------|------|--------|------------|
| Duilib | 1.5.0 | UI 框架 | MIT | ~800KB |
| minizip-ng | 4.0.7 | ZIP 读写 | zlib | ~250KB |
| LZMA SDK | 23.01 | 7Z 读写 | 公有领域 | ~400KB |
| unRAR | 7.0.0 | RAR 解压 | unRAR | ~250KB |
| zlib | 1.3.1 | GZIP/Deflate | zlib | ~110KB |
| bzip2 | 1.0.8 | BZip2 | BSD | ~120KB |
| liblzma | 5.6.3 | XZ/LZMA | 公有领域 | ~200KB |
| zstd | 1.5.6 | Zstandard | BSD | ~350KB |
| lz4 | 1.10.1 | LZ4 | BSD | ~120KB |
| **合计** | | | | **~2.6MB** |

剩余 ~2.4MB 用于应用代码、UI 资源、图标、皮肤 XML 等。

---

## 目录结构

```
bandzip-clone/
├── CMakeLists.txt              # 主构建脚本
├── LICENSE                    # AGPLv3 全文
├── README.md                  # 本文件
├── THIRDPARTY.md              # 第三方库许可证清单
├── src/
│   ├── main.cpp               # 程序入口
│   ├── app/                   # 应用层
│   │   ├── app.h/.cpp         # 应用对象、消息循环
│   │   ├── resource.h         # 资源 ID
│   │   └── config.h/.cpp      # 配置读写
│   ├── ui/                    # UI 层（Duilib）
│   │   ├── main_window.*      # 主窗口
│   │   ├── archive_list.*     # 文件列表控件
│   │   ├── extract_dialog.*   # 解压对话框
│   │   ├── compress_dialog.*  # 压缩对话框
│   │   ├── password_dialog.*  # 密码对话框
│   │   └── progress_dialog.*  # 进度对话框
│   ├── core/                  # 核心归档层
│   │   ├── archive.h          # IArchive 抽象接口
│   │   ├── archive_manager.*  # 工厂与调度
│   │   ├── archive_zip.*      # ZIP 驱动
│   │   ├── archive_7z.*       # 7Z 驱动
│   │   ├── archive_rar.*      # RAR 驱动
│   │   ├── archive_tar.*      # TAR 驱动
│   │   ├── archive_gz.*       # GZIP 驱动
│   │   ├── archive_bz2.*      # BZIP2 驱动
│   │   ├── archive_xz.*       # XZ 驱动
│   │   ├── archive_zstd.*     # ZSTD 驱动
│   │   ├── archive_lz4.*      # LZ4 驱动
│   │   ├── archive_cab.*      # CAB 驱动
│   │   ├── codec_detector.*   # 编码自动检测
│   │   └── password_callback.*# 密码回调
│   ├── shell/                 # Shell 集成
│   │   ├── shell_extension.*  # 右键菜单 DLL
│   │   ├── drag_drop.*        # OLE 拖拽
│   │   └── file_assoc.*      # 文件关联
│   └── utils/                 # 工具
│       ├── string_utils.*     # 字符串转换
│       ├── file_utils.*       # 文件操作
│       ├── path_utils.*       # 路径处理
│       ├── time_utils.*       # 时间转换
│       └── logger.*           # 日志
├── third_party/               # 第三方库源码（git submodule）
├── res/
│   ├── skins/default/         # Duilib 皮肤 XML
│   ├── icons/                 # 图标资源
│   └── manifest.xml           # 应用清单（DPI、UAC）
├── docs/
│   ├── architecture.md        # 架构文档
│   ├── format_drivers.md      # 格式驱动开发指南
│   └── api_reference.md       # 内部 API 参考
└── scripts/
    ├── build_release.bat      # 发布构建脚本
    └── check_size.py         # 体积检查脚本
```

---

## 构建指南

### 环境要求

- Windows 7 SP1 或更高（构建机）
- Visual Studio 2019 v16.11+（含 v141 工具集，兼容 Win7）
  或 MinGW-w64 9.2.0+（需 posix 线程模型）
- CMake 3.16+
- Perl（用于 OpenSSL，可选，仅 AES 加密需要）
- NASM（同上，可选）

### 构建步骤

```bat
:: 1. 克隆项目（含子模块）
git clone --recursive https://github.com/your-org/bandzip-clone.git
cd bandzip-clone

:: 2. 创建构建目录
mkdir build && cd build

:: 3. 配置（使用 v141 工具集以兼容 Win7）
cmake .. -G "Visual Studio 16 2019" -A Win32 ^
    -T v141 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DUSE_STATIC_CRT=ON ^
    -DENABLE_AES=ON ^
    -DENABLE_SFX=ON

:: 4. 编译
cmake --build . --config Release --parallel

:: 5. 检查产物体积
python ..\scripts\check_size.py
```

### 64 位构建

```bat
cmake .. -G "Visual Studio 16 2019" -A x64 -T v141 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DUSE_STATIC_CRT=ON
```

---

## 体积控制策略

为保证构建产物 < 5MB，本项目采取以下策略：

1. **全部静态链接**：避免依赖外部 DLL，单文件分发
2. **静态 CRT**：使用 `/MT` 而非 `/MD`，避免 vcredist 依赖
3. **LTO 链接期优化**：`-fLTO` 或 `/GL` + `/LTCG`，去除未使用代码
4. **裁剪第三方库**：
   - zstd 仅保留解码器与压缩器，去除字典 API
   - liblzma 关闭多线程后端（应用层自行管理）
   - unRAR 仅编译解压路径
5. **资源压缩**：图标使用 PNG（非 BMP），皮肤 XML 去注释
6. **UPX 可选压缩**：发布版可启用 UPX，进一步压缩至 ~2MB
7. **去除 RTTI 与异常（部分模块）**：核心归档层使用错误码而非异常

构建后体积分布（典型 Release x86）：

| 组件 | 体积 |
|------|------|
| bandzip.exe（主程序） | ~1.8MB |
| bzshell.dll（Shell 扩展） | ~280KB |
| 7zxa.dll（7Z 解码，可选） | ~180KB |
| 皮肤与图标资源 | ~350KB |
| **合计** | **~2.6MB** |

---

## 使用说明

### 主界面

启动后显示主窗口，左侧为压缩包结构树，右侧为文件列表。可通过以下方式打开压缩包：

- 菜单 `文件 → 打开`
- 工具栏"打开"按钮
- 拖拽压缩包到窗口
- 命令行参数 `bandzip.exe archive.zip`
- 资源管理器双击关联的压缩包

### 解压

- **解压到当前目录**：右键 → "解压到此处"
- **解压到子目录**：右键 → "解压到 archive\"
- **解压到...**：右键 → 选择目标目录
- **测试压缩包**：右键 → "测试完整性"

### 压缩

- 选中文件/文件夹 → 右键 → "添加到压缩包..."
- 在对话框中选择格式、压缩级别、分卷大小、密码等

### 编码处理

对于 ZIP 中常见的中文乱码问题，本软件会：

1. 优先读取 ZIP 的 UTF-8 标志位（bit 11）
2. 若未设置，使用启发式算法检测编码（GBK / Shift-JIS / Big5 / EUC-KR）
3. 用户可在"查看 → 编码"菜单手动切换

---

## Shell 右键菜单集成

安装时会注册 Shell 扩展 `bzshell.dll`，在文件/文件夹右键菜单中加入：

- 添加到压缩包...
- 添加到 "文件名.zip"
- 添加到 "文件名.7z"
- 解压到当前位置
- 解压到 "文件名\"
- 解压到...
- 测试压缩包

注册通过 `regsvr32 bzshell.dll` 完成，卸载时 `regsvr32 /u bzshell.dll`。

---

## 命令行参数

```
bandzip.exe [选项] <压缩包或文件>

选项:
  x, --extract          解压模式
  a, --add              压缩模式
  t, --test             测试模式
  -o<目录>              输出目录
  -p<密码>             压缩包密码
  -fmt<zip|7z|...>      输出格式
  -level<0..9>          压缩级别
  -v<大小>              分卷大小（如 -v100m）
  -y                    所有提示均回答"是"
  -silent               静默模式（无 UI）
  -log<文件>            日志输出到文件

示例:
  bandzip.exe archive.zip                 :: 浏览
  bandzip.exe x archive.zip -oC:\Out      :: 解压到 C:\Out
  bandzip.exe a out.zip file1.txt dir2\   :: 创建 ZIP
  bandzip.exe t archive.7z                :: 测试
```

---

## 开发路线图

- [x] v0.1 — 项目骨架、ZIP 读写
- [x] v0.2 — 7Z、TAR、GZ 支持
- [x] v0.3 — RAR 解压、编码检测
- [x] v0.4 — Duilib 主界面、列表控件
- [x] v0.5 — 解压/压缩对话框、进度
- [x] v0.6 — Shell 扩展、文件关联
- [x] v0.7 — 拖拽、命令行模式
- [x] v0.8 — 多卷、密码、AES
- [x] v0.9 — SFX、格式转换
- [ ] v1.0 — 完整测试、文档、发布

---

## 许可证

本项目基于 **GNU Affero General Public License v3** 协议开源，详见 [LICENSE](./LICENSE)。

第三方库的许可证详见 [THIRDPARTY.md](./THIRDPARTY.md)。

### 关于 unRAR 的特殊说明

unRAR 库采用特殊的 unRAR 许可证，允许用于解压 RAR 文件，但**禁止用于创建 RAR 文件**
（这是 RARLAB 的商业保护）。因此本项目：

- ✅ 可以解压 RAR 文件
- ❌ 不能创建 RAR 文件（请使用 7Z 或 ZIP 替代）

### 关于 AGPLv3 的网络服务条款

如果你通过网络提供基于本软件的服务（例如在线解压服务），你必须：

1. 公开你的修改版源代码
2. 提供获取源代码的明确链接
3. 保持 AGPLv3 协议不变

---

## 贡献

欢迎提交 Issue 与 Pull Request。请确保：

1. 代码风格遵循 `.clang-format`
2. 新增功能附带测试
3. 提交信息遵循 Conventional Commits
4. 签署 DCO（Developer Certificate of Origin）

---

## 致谢

- [Bandizip](https://www.bandizip.com/) — 功能对标参考
- [Duilib](https://github.com/duilib/duilib) — UI 框架
- [7-Zip](https://www.7-zip.org/) — 算法参考
- 所有第三方库的作者
