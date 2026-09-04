# ============================================================================
# PROJECT_STRUCTURE.md - 项目结构总览
#
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================

# 项目结构

```
bandzip-clone/
│
├── CMakeLists.txt                  # 主 CMake 配置
├── build.bat                       # Windows 构建脚本
├── .clang-format                   # 代码格式化配置
├── .gitignore                      # Git 忽略
├── LICENSE                         # AGPLv3 许可证
├── README.md                       # 项目说明
├── THIRDPARTY.md                   # 第三方库许可证
│
├── docs/                           # 文档
│   ├── BUILD.md                    # 构建指南
│   └── ARCHITECTURE.md             # 架构设计
│
├── scripts/                        # 脚本
│   ├── build.bat                   # 构建脚本
│   ├── check_size.py               # 体积检查
│   └── download_deps.py            # 下载第三方库
│
├── src/                            # 源代码
│   ├── app/                        # 应用程序层
│   │   ├── app.h                   # 应用入口
│   │   ├── app.cpp
│   │   ├── app.manifest            # 应用清单
│   │   ├── command_line.h          # 命令行解析
│   │   ├── command_line.cpp
│   │   ├── file_association.h      # 文件关联
│   │   ├── file_association.cpp
│   │   ├── sfx.h                   # 自解压模块
│   │   ├── sfx.cpp
│   │   └── version.rc              # 版本资源
│   │
│   ├── core/                       # 核心层
│   │   ├── archive.h               # 归档抽象接口
│   │   ├── archive.cpp
│   │   ├── archive_manager.h        # 归档管理器
│   │   ├── archive_manager.cpp
│   │   ├── archive_zip.h            # ZIP 驱动
│   │   ├── archive_zip.cpp
│   │   ├── archive_7z.h             # 7Z 驱动
│   │   ├── archive_7z.cpp
│   │   ├── archive_tar.h            # TAR 驱动
│   │   ├── archive_tar.cpp
│   │   ├── archive_gz.h             # GZ 驱动
│   │   ├── archive_gz.cpp
│   │   ├── archive_bz2.h            # BZ2 驱动
│   │   ├── archive_bz2.cpp
│   │   ├── archive_xz.h             # XZ 驱动
│   │   ├── archive_xz.cpp
│   │   ├── archive_zstd.h           # ZSTD 驱动
│   │   ├── archive_zstd.cpp
│   │   ├── archive_lz4.h            # LZ4 驱动
│   │   ├── archive_lz4.cpp
│   │   ├── archive_rar.h            # RAR 驱动（仅解压）
│   │   ├── archive_rar.cpp
│   │   ├── archive_cab.h            # CAB 驱动
│   │   ├── archive_cab.cpp
│   │   └── codec_detector.h         # 编码检测
│   │   └── codec_detector.cpp
│   │
│   ├── ui/                         # UI 层
│   │   ├── main_window.h            # 主窗口
│   │   ├── main_window.cpp
│   │   ├── dialogs.h                # 对话框
│   │   ├── dialogs.cpp
│   │   ├── drag_drop.h              # 拖拽支持
│   │   ├── drag_drop.cpp
│   │   ├── resource.h               # 资源头文件
│   │   └── version.rc               # 版本资源
│   │
│   ├── shell/                      # Shell 扩展
│   │   ├── shell_extension.h        # 右键菜单扩展
│   │   └── shell_extension.cpp
│   │
│   └── utils/                      # 工具
│       ├── string_utils.h           # 字符串工具
│       ├── string_utils.cpp
│       ├── file_utils.h             # 文件工具
│       ├── file_utils.cpp
│       ├── path_utils.h             # 路径工具
│       ├── path_utils.cpp
│       ├── logger.h                 # 日志
│       └── logger.cpp
│
├── res/                            # 资源
│   ├── skins/                      # 皮肤
│   │   └── default/
│   │       ├── main.xml            # 主窗口
│   │       ├── extract.xml         # 解压对话框
│   │       ├── compress.xml        # 压缩对话框
│   │       ├── password.xml        # 密码对话框
│   │       ├── progress.xml        # 进度对话框
│   │       ├── about.xml           # 关于对话框
│   │       └── options.xml        # 选项对话框
│   └── icons/                      # 图标
│
├── third_party/                   # 第三方库
│   └── CMakeLists.txt              # 第三方库构建配置
│
├── tests/                         # 测试
│   └── test_archive.cpp            # 单元测试
│
└── build/                         # 构建输出（gitignore）
```

## 文件统计

| 类型 | 文件数 | 说明 |
|------|--------|------|
| C++ 头文件 | 24 | 接口与声明 |
| C++ 源文件 | 24 | 实现 |
| XML 皮肤 | 7 | UI 布局 |
| 脚本 | 3 | 构建/检查/下载 |
| 文档 | 4 | README/BUILD/ARCH/STRUCT |
| 配置 | 4 | CMake/git/clang/rc |
| **总计** | **~66** | |

## 代码行数（估算）

| 模块 | 行数 |
|------|------|
| 核心层（接口+管理器+编码检测） | ~1,500 |
| 格式驱动（10 个） | ~8,000 |
| UI 层（主窗口+对话框+拖拽） | ~3,500 |
| Shell 扩展 | ~800 |
| 应用层（入口+命令行+关联+SFX） | ~2,000 |
| 工具层 | ~1,500 |
| 测试 | ~500 |
| 资源 XML | ~500 |
| 文档 | ~1,500 |
| **总计** | **~20,000** |
