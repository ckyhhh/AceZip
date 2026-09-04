// ============================================================================
// codec_detector.cpp - 文本编码自动检测实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "codec_detector.h"
#include "../utils/string_utils.h"

#include <windows.h>
#include <cstring>
#include <cmath>

namespace bandzip {

// ---------------------------------------------------------------------------
// 公开接口
// ---------------------------------------------------------------------------
NameEncoding CodecDetector::detect(const std::string& bytes) {
    if (bytes.empty()) return NameEncoding::Ascii;

    // 1. 纯 ASCII
    if (is_ascii(bytes)) return NameEncoding::Ascii;

    // 2. 合法 UTF-8（且包含非 ASCII 字符）
    if (is_valid_utf8(bytes)) {
        // 但要排除一些"恰好是合法 UTF-8 但实际是 GBK"的情况
        // 如果包含 3 字节序列（CJK 范围），更可能是 UTF-8
        bool has_multibyte = false;
        for (size_t i = 0; i < bytes.size(); ++i) {
            if ((bytes[i] & 0x80) && (bytes[i] & 0xE0) == 0xC0) {
                has_multibyte = true;
                break;
            }
        }
        if (has_multibyte) return NameEncoding::Utf8;
    }

    // 3. 双字节编码检测
    // 优先级：GBK > Big5 > ShiftJIS > EUC-KR
    // 因为 GBK 在中国大陆最常见
    double gbk_score  = score_gbk(bytes);
    double big5_score = score_big5(bytes);
    double sjis_score  = score_sjis(bytes);
    double kr_score    = score_euckr(bytes);

    // 取最高分
    NameEncoding best = NameEncoding::Gbk;
    double max_score = gbk_score;

    if (big5_score > max_score) { max_score = big5_score; best = NameEncoding::Big5; }
    if (sjis_score > max_score) { max_score = sjis_score; best = NameEncoding::ShiftJis; }
    if (kr_score   > max_score) { max_score = kr_score;   best = NameEncoding::EucKr; }

    // 如果都不太像，回退到 CP1252
    if (max_score < 0.3) {
        return NameEncoding::Cp1252;
    }

    return best;
}

std::wstring CodecDetector::decode(const std::string& bytes, NameEncoding enc) {
    if (bytes.empty()) return L"";

    int cp = codepage(enc);
    if (cp == 0) {
        // UTF-8
        return util::utf8_to_utf16(bytes);
    }

    return util::cp_to_utf16(bytes, cp);
}

std::wstring CodecDetector::decode_auto(const std::string& bytes) {
    NameEncoding enc = detect(bytes);
    return decode(bytes, enc);
}

const tchar* CodecDetector::encoding_name(NameEncoding enc) {
    switch (enc) {
    case NameEncoding::Utf8:    return _T("UTF-8");
    case NameEncoding::Gbk:     return _T("GBK");
    case NameEncoding::Big5:    return _T("Big5");
    case NameEncoding::ShiftJis:return _T("Shift-JIS");
    case NameEncoding::EucKr:   return _T("EUC-KR");
    case NameEncoding::Cp1252:  return _T("Windows-1252");
    case NameEncoding::Cp1251:  return _T("Windows-1251");
    case NameEncoding::Cp866:   return _T("CP866");
    case NameEncoding::Cp437:   return _T("CP437");
    case NameEncoding::Ascii:   return _T("ASCII");
    default:                    return _T("Unknown");
    }
}

int CodecDetector::codepage(NameEncoding enc) {
    switch (enc) {
    case NameEncoding::Utf8:    return 0;       // 特殊处理
    case NameEncoding::Gbk:     return 936;
    case NameEncoding::Big5:    return 950;
    case NameEncoding::ShiftJis:return 932;
    case NameEncoding::EucKr:   return 949;
    case NameEncoding::Cp1252:  return 1252;
    case NameEncoding::Cp1251:  return 1251;
    case NameEncoding::Cp866:   return 866;
    case NameEncoding::Cp437:   return 437;
    case NameEncoding::Ascii:   return 20127;
    default:                    return 1252;
    }
}

// ---------------------------------------------------------------------------
// 内部实现
// ---------------------------------------------------------------------------
bool CodecDetector::is_ascii(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return false;
    }
    return true;
}

bool CodecDetector::is_valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ++i;
            continue;
        }

        int extra = 0;
        if      ((c & 0xE0) == 0xC0) extra = 1;
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0) extra = 3;
        else return false;

        if (i + extra >= s.size()) return false;
        for (int j = 1; j <= extra; ++j) {
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) return false;
        }
        i += 1 + extra;
    }
    return true;
}

bool CodecDetector::is_valid_gbk(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }

        // GBK 双字节：第一字节 0x81-0xFE，第二字节 0x40-0xFE（除 0x7F）
        if (c1 < 0x81 || c1 > 0xFE) return false;
        if (i + 1 >= s.size()) return false;
        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
        if (c2 < 0x40 || c2 == 0x7F || c2 > 0xFE) return false;
        i += 2;
    }
    return true;
}

bool CodecDetector::is_valid_big5(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }

        // Big5：第一字节 0x81-0xFE，第二字节 0x40-0x7E 或 0xA1-0xFE
        if (c1 < 0x81 || c1 > 0xFE) return false;
        if (i + 1 >= s.size()) return false;
        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
        if (!((c2 >= 0x40 && c2 <= 0x7E) || (c2 >= 0xA1 && c2 <= 0xFE))) return false;
        i += 2;
    }
    return true;
}

bool CodecDetector::is_valid_sjis(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }

        // Shift-JIS：
        // 单字节：0xA1-0xDF（半角片假名）
        // 双字节：第一字节 0x81-0x9F 或 0xE0-0xFC
        //         第二字节 0x40-0x7E 或 0x80-0xFC
        if (c1 >= 0xA1 && c1 <= 0xDF) { ++i; continue; }

        if (!((c1 >= 0x81 && c1 <= 0x9F) || (c1 >= 0xE0 && c1 <= 0xFC))) return false;
        if (i + 1 >= s.size()) return false;
        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
        if (!((c2 >= 0x40 && c2 <= 0x7E) || (c2 >= 0x80 && c2 <= 0xFC))) return false;
        i += 2;
    }
    return true;
}

bool CodecDetector::is_valid_euckr(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }

        // EUC-KR：第一字节 0xA1-0xFE，第二字节 0xA1-0xFE
        if (c1 < 0xA1 || c1 > 0xFE) return false;
        if (i + 1 >= s.size()) return false;
        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
        if (c2 < 0xA1 || c2 > 0xFE) return false;
        i += 2;
    }
    return true;
}

double CodecDetector::score_utf8(const std::string& s) {
    if (!is_valid_utf8(s)) return 0.0;
    // 统计多字节序列数
    int multi = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((s[i] & 0x80) && (s[i] & 0xE0) == 0xC0) ++multi;
    }
    return multi > 0 ? 1.0 : 0.5;
}

double CodecDetector::score_gbk(const std::string& s) {
    if (!is_valid_gbk(s)) return 0.0;

    // 统计常见汉字范围
    int cjk = 0;
    int total = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }
        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);

        // 常用汉字范围：0xB0-0xF7（第一字节），0xA1-0xFE（第二字节）
        if (c1 >= 0xB0 && c1 <= 0xF7 && c2 >= 0xA1 && c2 <= 0xFE) ++cjk;
        ++total;
        i += 2;
    }

    if (total == 0) return 0.0;
    return static_cast<double>(cjk) / total;
}

double CodecDetector::score_big5(const std::string& s) {
    if (!is_valid_big5(s)) return 0.0;

    int cjk = 0;
    int total = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }
        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);

        // Big5 常用字：0xA4-0xC8（第一字节）
        if (c1 >= 0xA4 && c1 <= 0xC8) ++cjk;
        ++total;
        i += 2;
    }

    if (total == 0) return 0.0;
    return static_cast<double>(cjk) / total * 0.9;  // 略低于 GBK
}

double CodecDetector::score_sjis(const std::string& s) {
    if (!is_valid_sjis(s)) return 0.0;

    int cjk = 0;
    int total = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c1 = static_cast<unsigned char>(s[i]);
        if (c1 < 0x80) { ++i; continue; }
        if (c1 >= 0xA1 && c1 <= 0xDF) { ++i; continue; }

        unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
        // 常用汉字：0x82-0x83, 0x88, 0x98, 0xE0-0xE9（第一字节）
        if (c1 == 0x82 || c1 == 0x83 || c1 == 0x88 ||
            c1 == 0x98 || (c1 >= 0xE0 && c1 <= 0xE9)) ++cjk;
        ++total;
        i += 2;
    }

    if (total == 0) return 0.0;
    return static_cast<double>(cjk) / total * 0.85;
}

double CodecDetector::score_euckr(const std::string& s) {
    if (!is_valid_euckr(s)) return 0.0;
    // 简单返回 0.5（韩文场景较少）
    return 0.5;
}

double CodecDetector::score_cp1252(const std::string& s) {
    // CP1252 包含 0x80-0x9F 范围的特殊字符
    int valid = 0;
    for (unsigned char c : s) {
        if (c < 0x80) continue;
        // CP1252 中 0x81, 0x8D, 0x8F, 0x90, 0x9D 是未定义的
        if (c == 0x81 || c == 0x8D || c == 0x8F ||
            c == 0x90 || c == 0x9D) return 0.0;
        ++valid;
    }
    return valid > 0 ? 0.3 : 0.0;
}

} // namespace bandzip
