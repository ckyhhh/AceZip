#!/usr/bin/env python3
# ============================================================================
# download_deps.py - 下载第三方库
#
# 自动下载并解压所有第三方库到 third_party/ 目录。
#
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================
import os
import sys
import urllib.request
import tarfile
import zipfile
import shutil
from pathlib import Path

# 第三方库下载地址
DEPS = {
    "zlib-1.3.1": {
        "url": "https://zlib.net/zlib-1.3.1.tar.gz",
        "type": "tar.gz",
    },
    "minizip-ng-4.0.7": {
        "url": "https://github.com/zlib-ng/minizip-ng/archive/refs/tags/4.0.7.tar.gz",
        "type": "tar.gz",
    },
    "bzip2-1.0.8": {
        "url": "https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz",
        "type": "tar.gz",
    },
    "xz-5.6.3": {
        "url": "https://tukaani.org/xz/xz-5.6.3.tar.gz",
        "type": "tar.gz",
    },
    "zstd-1.5.6": {
        "url": "https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz",
        "type": "tar.gz",
    },
    "lz4-1.10.1": {
        "url": "https://github.com/lz4/lz4/releases/download/v1.10.1/lz4-1.10.1.tar.gz",
        "type": "tar.gz",
    },
    "unrar700": {
        "url": "https://www.rarlab.com/rar/unrarsrc-7.0.0.tar.gz",
        "type": "tar.gz",
    },
    "duilib": {
        "url": "https://github.com/duilib/duilib/archive/refs/heads/master.zip",
        "type": "zip",
    },
}

# LZMA SDK 需要手动下载（7z 格式）
LZMA_URL = "https://www.7-zip.org/a/lzma2301.7z"


def download(url, dest):
    """下载文件"""
    print(f"  下载: {url}")
    try:
        urllib.request.urlretrieve(url, dest)
        return True
    except Exception as e:
        print(f"  错误: {e}")
        return False


def extract_tar_gz(path, dest_dir):
    """解压 tar.gz"""
    with tarfile.open(path, "r:gz") as tar:
        tar.extractall(dest_dir)


def extract_zip(path, dest_dir):
    """解压 zip"""
    with zipfile.ZipFile(path, "r") as zip_ref:
        zip_ref.extractall(dest_dir)


def main():
    script_dir = Path(__file__).parent.parent
    third_party = script_dir / "third_party"
    cache_dir = third_party / ".cache"

    third_party.mkdir(exist_ok=True)
    cache_dir.mkdir(exist_ok=True)

    print("=" * 60)
    print("BandzipClone 第三方库下载工具")
    print("=" * 60)

    for name, info in DEPS.items():
        dest = third_party / name
        if dest.exists():
            print(f"[SKIP] {name} (已存在)")
            continue

        print(f"\n[下载] {name}")

        ext = ".tar.gz" if info["type"] == "tar.gz" else ".zip"
        cache_file = cache_dir / f"{name}{ext}"

        if not cache_file.exists():
            if not download(info["url"], str(cache_file)):
                print(f"[FAIL] 下载 {name} 失败")
                continue

        print(f"  解压: {cache_file}")
        try:
            if info["type"] == "tar.gz":
                extract_tar_gz(str(cache_file), str(third_party))
            else:
                extract_zip(str(cache_file), str(third_party))

            # GitHub 下载的目录名可能不同，重命名
            extracted = list(third_party.glob(f"{name}*"))
            for d in extracted:
                if d.is_dir() and d.name != name:
                    d.rename(dest)
                    break

            print(f"[OK] {name}")
        except Exception as e:
            print(f"[FAIL] 解压 {name} 失败: {e}")

    # LZMA SDK
    lzma_dest = third_party / "lzma2301"
    if not lzma_dest.exists():
        print(f"\n[注意] LZMA SDK 需要手动下载:")
        print(f"  1. 下载: {LZMA_URL}")
        print(f"  2. 解压到: {lzma_dest}")

    print("\n" + "=" * 60)
    print("完成！")
    print("=" * 60)


if __name__ == "__main__":
    main()
