# 第三方依赖库

本目录包含 BandzipClone 所需的全部第三方依赖库源码，已随项目一起提交，
CI 构建时无需再从外部下载。

## 依赖清单

| 目录 | 库名 | 版本 | 许可证 | 上游来源 |
|------|------|------|--------|----------|
| `zlib-1.3.1/` | zlib | 1.3.1 | zlib License | https://github.com/madler/zlib/tree/v1.3.1 |
| `minizip-ng-4.0.7/` | minizip-ng | 4.0.7 | zlib License | https://github.com/zlib-ng/minizip-ng/tree/4.0.7 |
| `lzma2301/` | LZMA SDK | 23.01 | Public Domain | https://www.7-zip.org/sdk.html |
| `bzip2-1.0.8/` | bzip2 | 1.0.8 | BSD-like | https://gitlab.com/bzip2/bzip2/-/tree/bzip2-1.0.8 |
| `xz-5.6.3/` | XZ Utils (liblzma) | 5.6.3 | Public Domain (0BSD) | https://github.com/tukaani-project/xz/tree/v5.6.3 |
| `zstd-1.5.6/` | zstd | 1.5.6 | BSD-3-Clause | https://github.com/facebook/zstd/tree/v1.5.6 |
| `lz4-1.10.1/` | lz4 | 1.10.0 | BSD-2-Clause | https://github.com/lz4/lz4/tree/v1.10.0 |
| `unrar/` | unRAR | 7.0.x | unRAR License | https://www.rarlab.com/rar_add.htm |
| `duilib/` | Duilib | master | MIT | https://github.com/duilib/duilib |

## 许可证兼容性

本项目主协议为 **AGPLv3**，与上述所有许可证兼容：

- zlib License、MIT、BSD-2/3-Clause、Public Domain 均为宽松许可证，
  允许与 AGPLv3 组合
- unRAR License 允许用于解压 RAR 文件（禁止用于创建 RAR），
  解压用途与 AGPLv3 兼容

组合后的整体作品仍受 AGPLv3 约束。详见项目根目录的 `THIRDPARTY.md`。

## 各库的原始 LICENSE 文件位置

- `zlib-1.3.1/LICENSE`
- `minizip-ng-4.0.7/LICENSE`
- `lzma2301/LICENSE`（本仓库补足的 Public Domain 声明）
- `bzip2-1.0.8/LICENSE`
- `xz-5.6.3/COPYING`（0BSD 公共领域）
- `zstd-1.5.6/LICENSE`
- `lz4-1.10.1/LICENSE`
- `unrar/license.txt`
- `duilib/LICENSE`

## 构建说明

CMake 会自动从本目录查找各库源码并编译为静态库，无需 vcpkg 或手动下载。
