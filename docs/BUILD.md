# ============================================================================
# BUILD.md - 构建指南
#
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================

# 构建指南

## 1. 环境要求

### 必需
- **Windows 7 SP1** 或更高（构建主机需 Windows 10+）
- **Visual Studio 2022**（含 C++ 桌面开发工作负载）
  - 也支持 VS 2019（需在 `build.bat` 中修改生成器）
- **CMake 3.16+**
- **Python 3.8+**（用于体积检查脚本）

### 第三方库（需手动下载）
将以下库解压到 `third_party/` 目录：

| 库 | 版本 | 下载地址 |
|----|------|----------|
| zlib | 1.3.1 | https://zlib.net/zlib-1.3.1.tar.gz |
| minizip-ng | 4.0.7 | https://github.com/zlib-ng/minizip-ng/archive/refs/tags/4.0.7.tar.gz |
| LZMA SDK | 23.01 | https://www.7-zip.org/a/lzma2301.7z |
| bzip2 | 1.0.8 | https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz |
| XZ Utils | 5.6.3 | https://tukaani.org/xz/xz-5.6.3.tar.gz |
| zstd | 1.5.6 | https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz |
| lz4 | 1.10.1 | https://github.com/lz4/lz4/releases/download/v1.10.1/lz4-1.10.1.tar.gz |
| unRAR | 7.0.0 | https://www.rarlab.com/rar/unrarsrc-7.0.0.tar.gz |
| Duilib | 1.5.0 | https://github.com/duilib/duilib/archive/refs/tags/1.5.0.tar.gz |

### 可选
- **OpenSSL 3.x**（用于 AES 加密，可选用 minizip-ng 内置实现）
- **UPX 4.x**（用于进一步压缩二进制体积）

## 2. 目录结构

```
bandzip-clone/
├── CMakeLists.txt
├── build.bat
├── src/
│   ├── app/           # 应用程序入口
│   ├── ui/            # Duilib UI
│   ├── core/          # 归档核心
│   ├── shell/         # Shell 扩展
│   └── utils/         # 工具类
├── third_party/      # 第三方库（需手动下载）
│   ├── zlib-1.3.1/
│   ├── minizip-ng-4.0.7/
│   ├── lzma2301/
│   ├── bzip2-1.0.8/
│   ├── xz-5.6.3/
│   ├── zstd-1.5.6/
│   ├── lz4-1.10.1/
│   ├── unrar700/
│   └── duilib/
├── res/               # 资源文件
│   ├── skins/         # Duilib 皮肤 XML
│   └── icons/         # 图标
├── scripts/          # 构建脚本
└── docs/             # 文档
```

## 3. 构建步骤

### 3.1 命令行构建

```batch
# 1. 下载第三方库到 third_party/ 目录

# 2. 运行构建脚本
build.bat MinSizeRel x64

# 或手动构建
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=MinSizeRel ^
    -DUSE_STATIC_CRT=ON ^
    -DENABLE_LTO=ON
cmake --build . --config MinSizeRel --parallel
```

### 3.2 Visual Studio 构建

1. 打开 CMake GUI
2. 设置源码目录和构建目录
3. 点击 Configure
4. 勾选以下选项：
   - `USE_STATIC_CRT`
   - `ENABLE_LTO`
   - `ENABLE_AES`
   - `ENABLE_SFX`
   - `ENABLE_SHELL_EXTENSION`
5. 点击 Generate
6. 打开生成的 `.sln` 文件
7. 选择 `MinSizeRel` 配置
8. 按 `Ctrl+Shift+B` 构建

### 3.3 构建配置

| 配置 | 优化级别 | 适用场景 |
|------|---------|---------|
| Debug | /Od /Zi | 开发调试 |
| Release | /O2 | 性能优先 |
| **MinSizeRel** | /O1 | **推荐**：体积优先 |
| RelWithDebInfo | /O2 /Zi | 性能+调试 |

## 4. 体积控制策略

### 4.1 编译选项

```cmake
# 静态链接 CRT（避免 MSVCRT 依赖）
set(USE_STATIC_CRT ON)

# LTO 链接时优化（消除未使用代码）
set(ENABLE_LTO ON)

# 体积优先优化
add_compile_options(/O1 /GS- /Zc:inline)

# 裁剪调试信息
add_link_options(/DEBUG:NONE /OPT:REF /OPT:ICF)
```

### 4.2 第三方库裁剪

| 库 | 裁剪选项 | 节省 |
|----|---------|------|
| zlib | `ZLIB_BUILD_EXAMPLES=OFF` | ~50KB |
| minizip-ng | `MZ_PKCRYPT=OFF MZ_WZAES=OFF` | ~30KB |
| zstd | `ZSTD_BUILD_PROGRAMS=OFF` | ~200KB |
| lz4 | `LZ4_BUILD_CLI=OFF` | ~50KB |
| Duilib | `DUILIB_BUILD_EXAMPLE=OFF` | ~100KB |

### 4.3 UPX 压缩（可选）

```batch
# 启用 UPX
cmake .. -DENABLE_UPX=ON

# 或手动压缩
upx --best --ultra-brute bandzip.exe
```

UPX 可将体积压缩 50-70%，但会触发杀毒软件误报。

### 4.4 体积分布

| 组件 | 体积 |
|------|------|
| bandzip.exe | ~3.5 MB |
| bzshell.dll | ~400 KB |
| skins/ | ~200 KB |
| icons/ | ~100 KB |
| **总计** | **~4.2 MB** |

## 5. 运行时依赖

构建产物**无任何运行时依赖**（除 Windows 系统库外）：

- ✅ 不需要 Visual C++ Redistributable
- ✅ 不需要 .NET Framework
- ✅ 不需要任何 DLL（所有库静态链接）
- ✅ 可在 Windows 7 SP1 原生运行

## 6. 安装与部署

### 6.1 绿色版

直接复制以下文件到任意目录即可运行：

```
bandzip.exe
bzshell.dll
skins/
icons/
```

### 6.2 安装版

运行 `bandzip.exe --register` 注册文件关联和 Shell 扩展。

运行 `bandzip.exe --unregister` 注销。

## 7. 故障排除

### 7.1 找不到第三方库

确保第三方库已解压到 `third_party/` 目录，且目录名与
`third_party/CMakeLists.txt` 中的路径一致。

### 7.2 编译错误

- 确保使用 VS 2022 或 VS 2019
- 确保安装了 C++ 桌面开发工作负载
- 确保安装了 Windows 10 SDK

### 7.3 链接错误

- 确保所有第三方库都使用相同的 CRT（/MT 或 /MD）
- 确保所有库都是相同架构（x86 或 x64）

### 7.4 体积超标

- 使用 `MinSizeRel` 配置
- 启用 LTO
- 启用 UPX
- 裁剪未使用的第三方库功能

## 8. 贡献

欢迎提交 Pull Request。请确保：

1. 代码通过 `clang-format` 格式化
2. 构建产物体积不超过 5MB
3. 在 Windows 7 上测试通过
4. 提交信息遵循 Conventional Commits
