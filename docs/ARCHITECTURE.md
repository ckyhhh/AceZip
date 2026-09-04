# ============================================================================
# ARCHITECTURE.md - 架构设计文档
#
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================

# 架构设计

## 1. 整体架构

BandzipClone 采用分层架构，各层之间通过接口解耦：

```
┌─────────────────────────────────────────────────────────────┐
│                     应用层 (app/)                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  WinMain    │  │ CommandLine │  │ FileAssociation     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                      UI 层 (ui/)                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ MainWindow  │  │  Dialogs    │  │  Resource           │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│                      基于 Duilib                              │
├─────────────────────────────────────────────────────────────┤
│                    核心层 (core/)                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ArchiveManager│  │  IArchive   │  │  CodecDetector     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│                         │                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  ZipArchive │ SevenZipArchive │ RarArchive │ ...     │   │
│  └──────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                   第三方库 (third_party/)                    │
│  minizip-ng │ LZMA SDK │ unRAR │ zlib │ bzip2 │ zstd │ ... │
├─────────────────────────────────────────────────────────────┤
│                   工具层 (utils/)                            │
│  StringUtils │ FileUtils │ PathUtils │ Logger │ ...        │
└─────────────────────────────────────────────────────────────┘
```

## 2. 核心接口

### 2.1 IArchive 接口

所有压缩格式驱动都实现 `IArchive` 接口：

```cpp
class IArchive {
public:
    virtual ~IArchive() = default;

    // 格式信息
    virtual ArchiveFormat format() const = 0;
    virtual bool can_read() const = 0;
    virtual bool can_write() const = 0;
    virtual bool can_encrypt() const = 0;
    virtual bool can_solid() const = 0;
    virtual bool can_volume() const = 0;

    // 打开/关闭
    virtual std::error_code open(const tstring& path,
                                 const tstring& password,
                                 OpenMode mode) = 0;
    virtual std::error_code close() = 0;
    virtual bool is_open() const = 0;

    // 读取
    virtual std::error_code read_entries(
        std::vector<ArchiveEntry>& entries) = 0;
    virtual std::error_code extract_entry(
        u32 index,
        const tstring& output_path,
        const ExtractOptions& opts) = 0;
    virtual std::error_code test_entry(u32 index) = 0;
    virtual std::error_code extract_files(
        const std::vector<u32>& indices,
        const tstring& output_dir,
        const ExtractOptions& opts) = 0;
    virtual std::error_code test() = 0;

    // 写入
    virtual std::error_code create(const CreateOptions& opts) = 0;
    virtual std::error_code add_files(
        const std::vector<tstring>& files,
        const CompressOptions& opts) = 0;
    virtual std::error_code delete_entries(
        const std::vector<u32>& indices) = 0;
};
```

### 2.2 工厂模式

通过工厂函数创建归档实例：

```cpp
std::unique_ptr<IArchive> create_archive(ArchiveFormat fmt);
std::unique_ptr<IArchive> open_archive(const tstring& path, ...);
```

### 2.3 回调机制

进度回调和密码回调：

```cpp
using ProgressCallback = std::function<void(const ProgressInfo&)>;
using PasswordCallback = std::function<tstring(const tstring& path,
                                                bool* cancelled)>;
```

## 3. 数据流

### 3.1 解压流程

```
用户拖入/打开压缩包
        │
        ▼
ArchiveManager::open()
        │
        ▼
detect_format()  ──→  根据魔数检测格式
        │
        ▼
create_archive()  ──→  创建对应驱动
        │
        ▼
IArchive::open()
        │
        ▼
IArchive::read_entries()  ──→  读取条目列表
        │
        ▼
MainWindow::RefreshList()  ──→  显示列表
        │
        ▼
用户选择"解压"
        │
        ▼
ExtractDialog  ──→  获取解压选项
        │
        ▼
IArchive::extract_files()
        │
        ▼
ProgressCallback  ──→  更新进度对话框
        │
        ▼
完成
```

### 3.2 压缩流程

```
用户选择文件/文件夹
        │
        ▼
CompressDialog  ──→  获取压缩选项
        │
        ▼
create_archive(format)
        │
        ▼
IArchive::create()
        │
        ▼
IArchive::add_files()
        │
        ▼
ProgressCallback  ──→  更新进度对话框
        │
        ▼
完成
```

## 4. 编码检测

ZIP 文件名编码问题是中文用户的痛点。BandzipClone 采用以下策略：

1. **检查 UTF-8 标志位**：ZIP 规范中 GPB 第 11 位为 1 表示文件名是 UTF-8
2. **UTF-8 验证**：即使没有标志位，也尝试验证是否为合法 UTF-8
3. **双字节编码检测**：对 GBK、Big5、Shift-JIS、EUC-KR 进行评分
4. **置信度选择**：选择得分最高的编码

```cpp
NameEncoding CodecDetector::detect(const std::string& bytes) {
    if (is_ascii(bytes)) return NameEncoding::Ascii;
    if (is_valid_utf8(bytes)) return NameEncoding::Utf8;

    double gbk  = score_gbk(bytes);
    double big5 = score_big5(bytes);
    double sjis = score_sjis(bytes);

    // 选择最高分
    return best_of(gbk, big5, sjis);
}
```

## 5. Shell 扩展

Shell 扩展是一个独立的 DLL（bzshell.dll），实现：

- `IShellExtInit` - 初始化
- `IContextMenu` - 上下文菜单
- `IClassFactory` - 类工厂

注册到注册表：

```
HKCR\*\shellex\ContextMenuHandlers\BandzipClone
HKCR\Folder\shellex\ContextMenuHandlers\BandzipClone
HKCR\Directory\shellex\ContextMenuHandlers\BandzipClone
```

## 6. 体积控制

### 6.1 静态链接

所有第三方库静态链接，避免 DLL 依赖。

### 6.2 LTO

启用链接时优化，消除未使用代码。

### 6.3 裁剪

第三方库只编译需要的部分：

- zlib: 不编译示例
- minizip-ng: 不编译 PKCRYPT/WZAES
- zstd: 不编译 CLI
- lz4: 不编译 CLI
- Duilib: 不编译示例

### 6.4 优化级别

使用 `/O1`（体积优先）而非 `/O2`（速度优先）。

## 7. 线程模型

- **主线程**：UI 操作
- **工作线程**：解压/压缩任务
- **进度回调**：通过 PostMessage 回到主线程更新 UI

```cpp
void MainWindow::RunAsyncTask(
    std::function<void()> task,
    std::function<void(std::error_code)> on_complete)
{
    std::thread([this, task, on_complete]() {
        task();
        // 通过 PostMessage 回到主线程
        PostMessage(m_hWnd, WM_TASK_COMPLETE, 0, 0);
    }).detach();
}
```

## 8. 错误处理

使用 `std::error_code` 而非异常：

```cpp
std::error_code ec;
auto archive = ArchiveManager::instance().open(path, password, &ec);
if (ec) {
    // 显示错误
}
```

错误码定义在 `ArchiveError` 枚举中，每个格式驱动负责将库特定的错误码
转换为 `ArchiveError`。

## 9. 配置管理

配置存储在：

```
%APPDATA%\BandzipClone\config.ini
%APPDATA%\BandzipClone\logs\bandzip_YYYYMMDD.log
```

配置采用简单的 INI 格式，键值对形式。

## 10. 测试策略

- **单元测试**：Google Test，测试各格式驱动
- **集成测试**：测试端到端流程
- **兼容性测试**：在 Windows 7/8/10/11 上测试
- **体积测试**：自动检查构建产物体积

## 11. 国际化

当前版本仅支持简体中文。未来计划支持：

- 繁体中文
- 英文
- 日文
- 韩文

使用 Duilib 的多语言机制（XML 语言文件）。
