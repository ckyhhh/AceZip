// ============================================================================
// codec_detector.h - 文本编码自动检测
//
// 用于解决 ZIP 文件名中文乱码问题。
// Bandizip 的核心特性之一：自动检测 ZIP 文件名编码。
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <string>

namespace bandzip {

// 文件名编码
enum class NameEncoding {
    Utf8,           // UTF-8（带 UTF-8 标志位）
    Gbk,            // 简体中文 GBK
    Big5,           // 繁体中文 Big5
    ShiftJis,       // 日文 Shift-JIS
    EucKr,          // 韩文 EUC-KR
    Cp1252,         // 西欧 Latin1
    Cp1251,         // 西里尔
    Cp866,          // 俄文 DOS
    Cp437,          // 美式 DOS
    Ascii,          // 纯 ASCII
    Unknown,
};

class CodecDetector {
public:
    // 检测字节流的编码
    static NameEncoding detect(const std::string& bytes);

    // 将字节流按检测到的编码转为 UTF-16
    static std::wstring decode(const std::string& bytes, NameEncoding enc);

    // 自动检测并解码
    static std::wstring decode_auto(const std::string& bytes);

    // 编码名称
    static const tchar* encoding_name(NameEncoding enc);

    // 编码对应的 Windows 代码页
    static int codepage(NameEncoding enc);

private:
    // 各编码的置信度评分
    static double score_utf8(const std::string& s);
    static double score_gbk(const std::string& s);
    static double score_big5(const std::string& s);
    static double score_sjis(const std::string& s);
    static double score_euckr(const std::string& s);
    static double score_cp1252(const std::string& s);

    // 检测是否为纯 ASCII
    static bool is_ascii(const std::string& s);

    // 检测是否为合法 UTF-8
    static bool is_valid_utf8(const std::string& s);

    // 检测双字节编码的合法性
    static bool is_valid_gbk(const std::string& s);
    static bool is_valid_big5(const std::string& s);
    static bool is_valid_sjis(const std::string& s);
    static bool is_valid_euckr(const std::string& s);
};

} // namespace bandzip
