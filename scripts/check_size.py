#!/usr/bin/env python3
# ============================================================================
# check_size.py - 检查构建产物体积
#
# 确保最终构建产物（bandzip.exe + bzshell.dll + 资源）总大小不超过 5MB。
#
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================
import os
import sys
import subprocess
from pathlib import Path

# 体积上限（字节）
MAX_SIZE = 5 * 1024 * 1024  # 5 MB

# 各组件体积上限（字节）
COMPONENT_LIMITS = {
    "bandzip.exe": 4 * 1024 * 1024,  # 4 MB
    "bzshell.dll": 512 * 1024,        # 512 KB
    "skins": 256 * 1024,               # 256 KB
    "icons": 128 * 1024,               # 128 KB
}


def get_dir_size(path):
    """获取目录总大小"""
    total = 0
    for dirpath, dirnames, filenames in os.walk(path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            if not os.path.islink(fp):
                total += os.path.getsize(fp)
    return total


def get_file_size(path):
    """获取文件大小"""
    return os.path.getsize(path)


def format_size(size):
    """格式化文件大小"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size < 1024:
            return f"{size:.2f} {unit}"
        size /= 1024
    return f"{size:.2f} TB"


def check_component(name, path, limit):
    """检查单个组件"""
    if not os.path.exists(path):
        print(f"  [SKIP] {name}: {path} (不存在)")
        return 0

    if os.path.isdir(path):
        size = get_dir_size(path)
    else:
        size = get_file_size(path)

    status = "OK" if size <= limit else "OVER"
    print(f"  [{status}] {name}: {format_size(size)} / {format_size(limit)}")

    return size


def main():
    if len(sys.argv) < 2:
        print("用法: check_size.py <build_dir>")
        sys.exit(1)

    build_dir = sys.argv[1]
    if not os.path.isdir(build_dir):
        print(f"错误: 目录不存在: {build_dir}")
        sys.exit(1)

    print("=" * 60)
    print("BandzipClone 构建产物体积检查")
    print("=" * 60)
    print(f"构建目录: {build_dir}")
    print(f"总大小上限: {format_size(MAX_SIZE)}")
    print()

    total = 0

    # 检查各组件
    for name, limit in COMPONENT_LIMITS.items():
        path = os.path.join(build_dir, name)
        total += check_component(name, path, limit)

    print()
    print("-" * 60)
    print(f"总大小: {format_size(total)} / {format_size(MAX_SIZE)}")

    if total <= MAX_SIZE:
        print(f"[PASS] 体积检查通过！")
        print(f"  剩余空间: {format_size(MAX_SIZE - total)}")
        sys.exit(0)
    else:
        print(f"[FAIL] 体积超出限制！")
        print(f"  超出: {format_size(total - MAX_SIZE)}")
        print()
        print("优化建议:")
        print("  1. 启用 LTO: cmake -DENABLE_LTO=ON")
        print("  2. 启用 UPX: cmake -DENABLE_UPX=ON")
        print("  3. 使用 /MT 静态链接 CRT: cmake -DUSE_STATIC_CRT=ON")
        print("  4. 使用 Release 模式: cmake -DCMAKE_BUILD_TYPE=Release")
        print("  5. 裁剪未使用的第三方库功能")
        print("  6. 使用 /O1 而非 /O2（体积优先）")
        print("  7. 移除调试符号: strip")
        sys.exit(1)


if __name__ == "__main__":
    main()
