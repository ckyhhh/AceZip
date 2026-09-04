// ============================================================================
// test_archive.cpp - 归档驱动单元测试
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "../src/core/archive.h"
#include "../src/core/archive_zip.h"
#include "../src/core/archive_7z.h"
#include "../src/core/archive_tar.h"
#include "../src/core/archive_gz.h"
#include "../src/core/archive_bz2.h"
#include "../src/core/archive_xz.h"
#include "../src/core/archive_zstd.h"
#include "../src/core/archive_lz4.h"
#include "../src/core/archive_rar.h"
#include "../src/core/codec_detector.h"
#include "../src/utils/string_utils.h"
#include "../src/utils/file_utils.h"
#include "../src/utils/path_utils.h"

#include <gtest/gtest.h>
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <random>
#include <filesystem>

namespace bandzip {
namespace test {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// 测试工具
// ---------------------------------------------------------------------------
class ArchiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "bandzip_test";
        fs::create_directories(temp_dir_);
        CreateTestFiles();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_dir_, ec);
    }

    void CreateTestFiles() {
        {
            std::ofstream f(temp_dir_ / "test1.txt");
            f << "Hello, World!\n";
            f << "This is a test file for BandzipClone.\n";
            f << "中文测试内容。\n";
        }
        {
            std::ofstream f(temp_dir_ / "test2.bin", std::ios::binary);
            std::mt19937 rng(42);
            std::vector<char> data(65536);
            for (auto& c : data) c = static_cast<char>(rng());
            f.write(data.data(), data.size());
        }
        {
            fs::create_directories(temp_dir_ / "subdir");
            std::ofstream f(temp_dir_ / "subdir" / "test3.txt");
            f << "Subdirectory test.\n";
        }
        {
            std::ofstream f(temp_dir_ / "empty.txt");
        }
    }

    fs::path temp_dir_;
};

// ---------------------------------------------------------------------------
// ZIP 测试
// ---------------------------------------------------------------------------
TEST_F(ArchiveTest, ZipCompressExtract) {
    tstring archive_path = (temp_dir_ / "test.zip").wstring();
    tstring extract_dir = (temp_dir_ / "extract_zip").wstring();

    auto archive = create_archive(ArchiveFormat::Zip);
    ASSERT_NE(archive, nullptr);

    CreateOptions opts;
    opts.archive_path = archive_path;
    opts.format = ArchiveFormat::Zip;
    opts.level = 6;
    auto ec = archive->create(opts);
    ASSERT_FALSE(ec) << ec.message();

    CompressOptions copts;
    copts.method = CompressionMethod::Deflate;
    copts.level = 6;
    ec = archive->add_files({
        (temp_dir_ / "test1.txt").wstring(),
        (temp_dir_ / "test2.bin").wstring(),
        (temp_dir_ / "subdir").wstring(),
        (temp_dir_ / "empty.txt").wstring(),
    }, copts);
    ASSERT_FALSE(ec) << ec.message();

    archive->close();

    auto archive2 = open_archive(archive_path);
    ASSERT_NE(archive2, nullptr);

    auto entries = archive2->entries();
    EXPECT_EQ(entries.size(), 4u);

    ExtractOptions eopts;
    eopts.overwrite = OverwriteMode::All;
    eopts.create_dir = true;

    std::vector<u32> indices;
    for (size_t i = 0; i < entries.size(); ++i) {
        indices.push_back(static_cast<u32>(i));
    }

    ec = archive2->extract_files(indices, extract_dir, eopts);
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_TRUE(fs::exists(extract_dir / "test1.txt"));
    EXPECT_TRUE(fs::exists(extract_dir / "test2.bin"));
    EXPECT_TRUE(fs::exists(extract_dir / "subdir" / "test3.txt"));
    EXPECT_TRUE(fs::exists(extract_dir / "empty.txt"));
}

TEST_F(ArchiveTest, ZipPassword) {
    tstring archive_path = (temp_dir_ / "test_pwd.zip").wstring();
    tstring extract_dir = (temp_dir_ / "extract_pwd").wstring();
    tstring password = _T("test123");

    auto archive = create_archive(ArchiveFormat::Zip);
    ASSERT_NE(archive, nullptr);

    CreateOptions opts;
    opts.archive_path = archive_path;
    opts.format = ArchiveFormat::Zip;
    opts.password = password;
    opts.encrypt = true;
    opts.encryption = EncryptionMethod::Aes256;
    auto ec = archive->create(opts);
    ASSERT_FALSE(ec) << ec.message();

    CompressOptions copts;
    copts.method = CompressionMethod::Deflate;
    copts.level = 6;
    ec = archive->add_files({
        (temp_dir_ / "test1.txt").wstring(),
    }, copts);
    ASSERT_FALSE(ec) << ec.message();

    archive->close();

    auto archive2 = open_archive(archive_path, _T("wrong"));
    ASSERT_NE(archive2, nullptr);
    auto entries = archive2->entries();
    ASSERT_FALSE(entries.empty());

    ExtractOptions eopts;
    eopts.overwrite = OverwriteMode::All;
    ec = archive2->extract_entry(0,
        (extract_dir / "wrong.txt").wstring(), eopts);
    EXPECT_TRUE(ec == make_error_code(ArchiveError::WrongPassword) ||
                ec == make_error_code(ArchiveError::PasswordRequired));

    auto archive3 = open_archive(archive_path, password);
    ASSERT_NE(archive3, nullptr);

    entries = archive3->entries();
    ASSERT_FALSE(entries.empty());

    ec = archive3->extract_entry(0,
        (extract_dir / "correct.txt").wstring(), eopts);
    EXPECT_FALSE(ec) << ec.message();
}

TEST_F(ArchiveTest, ZipTest) {
    tstring archive_path = (temp_dir_ / "test.zip").wstring();

    auto archive = open_archive(archive_path);
    ASSERT_NE(archive, nullptr);

    auto ec = archive->test();
    EXPECT_FALSE(ec) << ec.message();
}

// ---------------------------------------------------------------------------
// 7Z 测试
// ---------------------------------------------------------------------------
TEST_F(ArchiveTest, SevenZipCompressExtract) {
    tstring archive_path = (temp_dir_ / "test.7z").wstring();
    tstring extract_dir = (temp_dir_ / "extract_7z").wstring();

    auto archive = create_archive(ArchiveFormat::SevenZip);
    ASSERT_NE(archive, nullptr);

    CreateOptions opts;
    opts.archive_path = archive_path;
    opts.format = ArchiveFormat::SevenZip;
    opts.level = 5;
    auto ec = archive->create(opts);
    ASSERT_FALSE(ec) << ec.message();

    CompressOptions copts;
    copts.method = CompressionMethod::Lzma2;
    copts.level = 5;
    ec = archive->add_files({
        (temp_dir_ / "test1.txt").wstring(),
        (temp_dir_ / "test2.bin").wstring(),
    }, copts);
    ASSERT_FALSE(ec) << ec.message();

    archive->close();

    auto archive2 = open_archive(archive_path);
    ASSERT_NE(archive2, nullptr);

    auto entries = archive2->entries();
    EXPECT_EQ(entries.size(), 2u);

    ExtractOptions eopts;
    eopts.overwrite = OverwriteMode::All;
    eopts.create_dir = true;

    std::vector<u32> indices;
    for (size_t i = 0; i < entries.size(); ++i) {
        indices.push_back(static_cast<u32>(i));
    }

    ec = archive2->extract_files(indices, extract_dir, eopts);
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_TRUE(fs::exists(extract_dir / "test1.txt"));
    EXPECT_TRUE(fs::exists(extract_dir / "test2.bin"));
}

// ---------------------------------------------------------------------------
// TAR 测试
// ---------------------------------------------------------------------------
TEST_F(ArchiveTest, TarCompressExtract) {
    tstring archive_path = (temp_dir_ / "test.tar").wstring();
    tstring extract_dir = (temp_dir_ / "extract_tar").wstring();

    auto archive = create_archive(ArchiveFormat::Tar);
    ASSERT_NE(archive, nullptr);

    CreateOptions opts;
    opts.archive_path = archive_path;
    opts.format = ArchiveFormat::Tar;
    auto ec = archive->create(opts);
    ASSERT_FALSE(ec) << ec.message();

    CompressOptions copts;
    copts.method = CompressionMethod::Copy;
    ec = archive->add_files({
        (temp_dir_ / "test1.txt").wstring(),
        (temp_dir_ / "subdir").wstring(),
    }, copts);
    ASSERT_FALSE(ec) << ec.message();

    archive->close();

    auto archive2 = open_archive(archive_path);
    ASSERT_NE(archive2, nullptr);

    auto entries = archive2->entries();
    EXPECT_GE(entries.size(), 2u);

    ExtractOptions eopts;
    eopts.overwrite = OverwriteMode::All;
    eopts.create_dir = true;

    std::vector<u32> indices;
    for (size_t i = 0; i < entries.size(); ++i) {
        indices.push_back(static_cast<u32>(i));
    }

    ec = archive2->extract_files(indices, extract_dir, eopts);
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_TRUE(fs::exists(extract_dir / "test1.txt"));
    EXPECT_TRUE(fs::exists(extract_dir / "subdir" / "test3.txt"));
}

// ---------------------------------------------------------------------------
// GZ 测试
// ---------------------------------------------------------------------------
TEST_F(ArchiveTest, GzCompressExtract) {
    tstring src = (temp_dir_ / "test1.txt").wstring();
    tstring archive_path = (temp_dir_ / "test1.txt.gz").wstring();
    tstring extract_path = (temp_dir_ / "extract_gz.txt").wstring();

    auto archive = create_archive(ArchiveFormat::Gzip);
    ASSERT_NE(archive, nullptr);

    CreateOptions opts;
    opts.archive_path = archive_path;
    opts.format = ArchiveFormat::Gzip;
    opts.level = 6;
    auto ec = archive->create(opts);
    ASSERT_FALSE(ec) << ec.message();

    CompressOptions copts;
    copts.method = CompressionMethod::Deflate;
    copts.level = 6;
    ec = archive->add_files({ src }, copts);
    ASSERT_FALSE(ec) << ec.message();

    archive->close();

    auto archive2 = open_archive(archive_path);
    ASSERT_NE(archive2, nullptr);

    ExtractOptions eopts;
    eopts.overwrite = OverwriteMode::All;
    ec = archive2->extract_entry(0, extract_path, eopts);
    EXPECT_FALSE(ec) << ec.message();

    std::ifstream f1(src, std::ios::binary);
    std::ifstream f2(extract_path, std::ios::binary);
    std::string s1, s2;
    f1.seekg(0, std::ios::end); s1.resize(f1.tellg()); f1.seekg(0); f1.read(&s1[0], s1.size());
    f2.seekg(0, std::ios::end); s2.resize(f2.tellg()); f2.seekg(0); f2.read(&s2[0], s2.size());
    EXPECT_EQ(s1, s2);
}

// ---------------------------------------------------------------------------
// 编码检测测试
// ---------------------------------------------------------------------------
TEST(CodecDetectorTest, DetectAscii) {
    std::string s = "Hello, World!";
    auto enc = CodecDetector::detect(s);
    EXPECT_EQ(enc, NameEncoding::Ascii);
}

TEST(CodecDetectorTest, DetectUtf8) {
    std::string s = "中文";
    auto enc = CodecDetector::detect(s);
    EXPECT_EQ(enc, NameEncoding::Utf8);
}

TEST(CodecDetectorTest, DetectGbk) {
    std::string s;
    s += static_cast<char>(0xD6);
    s += static_cast<char>(0xD0);
    s += static_cast<char>(0xCE);
    s += static_cast<char>(0xC4);
    auto enc = CodecDetector::detect(s);
    EXPECT_EQ(enc, NameEncoding::Gbk);
}

TEST(CodecDetectorTest, DecodeAuto) {
    std::string utf8 = "中文";
    std::wstring w = CodecDetector::decode_auto(utf8);
    EXPECT_EQ(w, L"中文");
}

// ---------------------------------------------------------------------------
// 路径安全测试
// ---------------------------------------------------------------------------
TEST(PathSafetyTest, ZipSlip) {
    EXPECT_TRUE(util::has_parent_ref(_T("../foo")));
    EXPECT_TRUE(util::has_parent_ref(_T("foo/../bar")));
    EXPECT_TRUE(util::has_parent_ref(_T("foo/..")));
    EXPECT_FALSE(util::has_parent_ref(_T("foo/bar")));
    EXPECT_FALSE(util::has_parent_ref(_T("foo")));
}

TEST(PathSafetyTest, SanitizePath) {
    EXPECT_EQ(util::sanitize_path(_T("../foo")), _T("_.._\\foo"));
    EXPECT_EQ(util::sanitize_path(_T("foo:bar")), _T("foo_bar"));
}

// ---------------------------------------------------------------------------
// 工具函数测试
// ---------------------------------------------------------------------------
TEST(StringUtilsTest, FormatSize) {
    EXPECT_EQ(util::format_size(0), _T("0 B"));
    EXPECT_EQ(util::format_size(1023), _T("1023 B"));
    EXPECT_EQ(util::format_size(1024), _T("1.00 KB"));
    EXPECT_EQ(util::format_size(1048576), _T("1.00 MB"));
    EXPECT_EQ(util::format_size(1073741824), _T("1.00 GB"));
}

TEST(StringUtilsTest, Utf8Utf16) {
    std::string utf8 = "中文";
    std::wstring utf16 = util::utf8_to_utf16(utf8);
    EXPECT_EQ(utf16, L"中文");
    EXPECT_EQ(util::utf16_to_utf8(utf16), utf8);
}

} // namespace test
} // namespace bandzip

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
