# ============================================================================
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(BandzipClone
    VERSION 0.9.0
    DESCRIPTION "Open-source Bandizip-like archiver (AGPLv3)"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# 全局选项
# ---------------------------------------------------------------------------
option(BUILD_SHARED_LIBS        "Build shared libraries"                  OFF)
option(USE_STATIC_CRT          "Use static C runtime (/MT)"              ON)
option(ENABLE_AES              "Enable AES encryption (needs OpenSSL)"   ON)
option(ENABLE_SFX              "Enable self-extracting archives"         ON)
option(ENABLE_SHELL_EXTENSION  "Build Shell extension DLL"               ON)
option(ENABLE_TESTS            "Build unit tests"                        OFF)
option(ENABLE_LTO              "Enable Link-Time Optimization"          ON)
option(ENABLE_UPX              "Compress final binary with UPX"          OFF)
option(BUILD_32BIT             "Force 32-bit build (Win7 compat)"        OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# 仅支持 Windows
if(NOT WIN32)
    message(FATAL_ERROR "BandzipClone only supports Windows.")
endif()

# ---------------------------------------------------------------------------
# 编译器/链接器全局设置
# ---------------------------------------------------------------------------
if(MSVC)
    if(USE_STATIC_CRT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()

    # 警告等级 4，但禁用部分噪音
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_options(
        /wd4127  # 条件表达式是常量
        /wd4456  # 隐藏局部变量
        /wd4458  # 隐藏类成员
        /wd4505  # 未引用的局部函数已移除
        /wd4820  # 填充
        /wd4668  # 未定义的宏
    )

    if(ENABLE_LTO)
        add_compile_options(/GL)
        add_link_options(/LTCG)
    endif()

    # 32 位兼容 Win7
    if(BUILD_32BIT)
        set(CMAKE_GENERATOR_PLATFORM Win32)
    endif()

    # 减小体积：合并节、去除调试信息（Release）
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/OPT:REF /OPT:ICF /INCREMENTAL:NO /SUBSYSTEM:WINDOWS")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/OPT:REF /OPT:ICF /INCREMENTAL:NO")
else()
    # MinGW
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    if(ENABLE_LTO)
        add_compile_options(-flto)
        add_link_options(-flto -s)
    endif()
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# 输出目录
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# 第三方库
# ---------------------------------------------------------------------------
# 各库以子目录形式存在，使用 FetchContent 或 add_subdirectory
# 此处采用 add_subdirectory，便于裁剪

# zlib (GZIP/Deflate)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib)

# minizip-ng (ZIP)
set(MZ_PROJECT_NAME "minizip-ng" CACHE STRING "" FORCE)
set(MZ_LIB_SUFFIX "" CACHE STRING "" FORCE)
set(MZ_COMPAT ON CACHE BOOL "" FORCE)
set(MZ_ZLIB ON CACHE BOOL "" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "" FORCE)
set(MZ_LZMA OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL ${ENABLE_AES} CACHE BOOL "" FORCE)
set(MZ_LIBCOMP OFF CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS OFF CACHE BOOL "" FORCE)
set(MZ_ICONV OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/minizip-ng EXCLUDE_FROM_ALL)
set(MINIZIP_LIBRARIES minizip)
set(MINIZIP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip-ng)

# bzip2
set(ENABLE_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC_LIB ON CACHE BOOL "" FORCE)
set(ENABLE_APP OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/bzip2 EXCLUDE_FROM_ALL)
set(BZIP2_LIBRARIES bz2_static)
set(BZIP2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bzip2)

# liblzma (XZ)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_MICROARCH OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/xz EXCLUDE_FROM_ALL)
set(LZMA_LIBRARIES liblzma_static)
set(LZMA_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xz/src/liblzma/api)

# zstd
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zstd/build/cmake EXCLUDE_FROM_ALL)
set(ZSTD_LIBRARIES libzstd_static)
set(ZSTD_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd/lib)

# lz4
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/lz4/build/cmake EXCLUDE_FROM_ALL)
set(LZ4_LIBRARIES lz4_static)
set(LZ4_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib)

# LZMA SDK (7z)
# LZMA SDK 无 CMake，直接编译所需源文件
add_library(lzma_sdk STATIC
    third_party/lzma-sdk/C/7zArcIn.c
    third_party/lzma-sdk/C/7zAlloc.c
    third_party/lzma-sdk/C/7zBuf.c
    third_party/lzma-sdk/C/7zCrc.c
    third_party/lzma-sdk/C/7zCrcOpt.c
    third_party/lzma-sdk/C/7zDec.c
    third_party/lzma-sdk/C/7zFile.c
    third_party/lzma-sdk/C/7zStream.c
    third_party/lzma-sdk/C/Aes.c
    third_party/lzma-sdk/C/AesOpt.c
    third_party/lzma-sdk/C/Alloc.c
    third_party/lzma-sdk/C/Bcj2.c
    third_party/lzma-sdk/C/Bcj2Enc.c
    third_party/lzma-sdk/C/Bra.c
    third_party/lzma-sdk/C/Bra86.c
    third_party/lzma-sdk/C/BraIA64.c
    third_party/lzma-sdk/C/CpuArch.c
    third_party/lzma-sdk/C/Delta.c
    third_party/lzma-sdk/C/LzFind.c
    third_party/lzma-sdk/C/LzFindOpt.c
    third_party/lzma-sdk/C/Lzma2Dec.c
    third_party/lzma-sdk/C/Lzma2DecMt.c
    third_party/lzma-sdk/C/Lzma2Enc.c
    third_party/lzma-sdk/C/LzmaDec.c
    third_party/lzma-sdk/C/LzmaEnc.c
    third_party/lzma-sdk/C/Ppmd7.c
    third_party/lzma-sdk/C/Ppmd7Dec.c
    third_party/lzma-sdk/C/Ppmd7Enc.c
    third_party/lzma-sdk/C/Sha256.c
    third_party/lzma-sdk/C/Sha256Opt.c
    third_party/lzma-sdk/C/Xz.c
    third_party/lzma-sdk/C/XzCrc64.c
    third_party/lzma-sdk/C/XzCrc64Opt.c
    third_party/lzma-sdk/C/XzDec.c
    third_party/lzma-sdk/C/XzEnc.c
)
target_include_directories(lzma_sdk PUBLIC third_party/lzma-sdk/C)
if(MSVC)
    target_compile_options(lzma_sdk PRIVATE /W3 /D_7ZIP_ST)
else()
    target_compile_options(lzma_sdk PRIVATE -D_7ZIP_ST)
endif()

# unRAR (RAR 解压)
add_library(unrar STATIC
    third_party/unrar/strfn.cpp
    third_party/unrar/strlist.cpp
    third_party/unrar/array.hpp
    third_party/unrar/list.cpp
    third_party/unrar/unpack.cpp
    third_party/unrar/unpack15.cpp
    third_party/unrar/unpack20.cpp
    third_party/unrar/unpack30.cpp
    third_party/unrar/unpack50.cpp
    third_party/unrar/unpack50frag.cpp
    third_party/unrar/rar.cpp
    third_party/unrar/file.cpp
    third_party/unrar/filefn.cpp
    third_party/unrar/filestr.cpp
    third_party/unrar/getbits.cpp
    third_party/unrar/rdwrfn.cpp
    third_party/unrar/crc.cpp
    third_party/unrar/options.cpp
    third_party/unrar/pathfn.cpp
    third_party/unrar/smallfn.cpp
    third_party/unrar/global.cpp
    third_party/unrar/system.cpp
    third_party/unrar/timefn.cpp
    third_party/unrar/blake2s.cpp
    third_party/unrar/hash.cpp
    third_party/unrar/headers.cpp
    third_party/unrar/headers5.cpp
    third_party/unrar/secpassword.cpp
    third_party/unrar/crypt.cpp
    third_party/unrar/crypt3.cpp
    third_party/unrar/crypt5.cpp
    third_party/unrar/secpassword.cpp
    third_party/unrar/cmddata.cpp
    third_party/unrar/ui.cpp
    third_party/unrar/uiwin.cpp
    third_party/unrar/blake2sp.cpp
    third_party/unrar/arccmt.cpp
    third_party/unrar/archive.cpp
    third_party/unrar/arcread.cpp
    third_party/unrar/unicode.cpp
    third_party/unrar/isnt.cpp
    third_party/unrar/recvol3.cpp
    third_party/unrar/extract.cpp
    third_party/unrar/rarvm.cpp
    third_party/unrar/suballoc.cpp
    third_party/unrar/ppmd.cpp
    third_party/unrar/ppmddecode.cpp
    third_party/unrar/dll.cpp
)
target_include_directories(unrar PUBLIC third_party/unrar)
target_compile_definitions(unrar PRIVATE
    SILENT RARDLL
    _UNICODE UNICODE
    NOMINMAX
    WINVER=0x0601
    _WIN32_WINNT=0x0601
)
if(MSVC)
    target_compile_options(unrar PRIVATE /W3 /EHsc)
endif()

# Duilib
set(DUILIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(DUILIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/duilib EXCLUDE_FROM_ALL)
set(DUILIB_LIBRARIES duilib_static)
set(DUILIB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/duilib)

# ---------------------------------------------------------------------------
# 主程序
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/app/app.cpp
    src/app/config.cpp
    src/ui/main_window.cpp
    src/ui/archive_list.cpp
    src/ui/extract_dialog.cpp
    src/ui/compress_dialog.cpp
    src/ui/password_dialog.cpp
    src/ui/progress_dialog.cpp
    src/core/archive.cpp
    src/core/archive_manager.cpp
    src/core/archive_zip.cpp
    src/core/archive_7z.cpp
    src/core/archive_rar.cpp
    src/core/archive_tar.cpp
    src/core/archive_gz.cpp
    src/core/archive_bz2.cpp
    src/core/archive_xz.cpp
    src/core/archive_zstd.cpp
    src/core/archive_lz4.cpp
    src/core/archive_cab.cpp
    src/core/codec_detector.cpp
    src/core/password_callback.cpp
    src/shell/drag_drop.cpp
    src/shell/file_assoc.cpp
    src/utils/string_utils.cpp
    src/utils/file_utils.cpp
    src/utils/path_utils.cpp
    src/utils/time_utils.cpp
    src/utils/logger.cpp
)

add_executable(bandzip WIN32 ${APP_SOURCES})

target_include_directories(bandzip PRIVATE
    src
    ${DUILIB_INCLUDE_DIRS}
    ${MINIZIP_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${BZIP2_INCLUDE_DIRS}
    ${LZMA_INCLUDE_DIRS}
    ${ZSTD_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
    third_party/lzma-sdk/C
    third_party/unrar
    ${CMAKE_CURRENT_SOURCE_DIR}/res
)

target_link_libraries(bandzip PRIVATE
    ${DUILIB_LIBRARIES}
    ${MINIZIP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${BZIP2_LIBRARIES}
    ${LZMA_LIBRARIES}
    ${ZSTD_LIBRARIES}
    ${LZ4_LIBRARIES}
    lzma_sdk
    unrar
    # Windows 系统库
    user32
    gdi32
    comctl32
    comdlg32
    shell32
    ole32
    oleaut32
    uuid
    advapi32
    shlwapi
    dwmapi
    uxtheme
    psapi
    version
    wininet
    crypt32
    ws2_32
)

target_compile_definitions(bandzip PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    _WIN32_IE=0x0A00
    NOMCX
    NOHELP
    NOGDI
    NOSERVICE
    NOMETAFILE
    NOCRYPT
    NOTAPE
    BANDZIP_VERSION="${PROJECT_VERSION}"
    BANDZIP_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BANDZIP_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BANDZIP_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# 嵌入清单（DPI 感知、UAC、ComCtl32 v6）
if(MSVC)
    set_target_properties(bandzip PROPERTIES
        LINK_FLAGS "/MANIFEST:NO /DEBUG:NONE"
        LINK_FLAGS_RELEASE "/MANIFEST:NO /DEBUG:NONE /SUBSYSTEM:WINDOWS,5.01"
    )
    target_sources(bandzip PRIVATE res/bandzip.manifest)
    set_source_files_properties(res/bandzip.manifest PROPERTIES
        LANGUAGE "RC"
    )
endif()

# 复制资源到输出目录
add_custom_command(TARGET bandzip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res/skins
        $<TARGET_FILE_DIR:bandzip>/skins
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res/icons
        $<TARGET_FILE_DIR:bandzip>/icons
    COMMENT "Copying resources to output directory"
)

# ---------------------------------------------------------------------------
# Shell 扩展 DLL
# ---------------------------------------------------------------------------
if(ENABLE_SHELL_EXTENSION)
    add_library(bzshell SHARED
        src/shell/shell_extension.cpp
        src/shell/class_factory.cpp
        src/utils/string_utils.cpp
        src/utils/logger.cpp
    )
    target_include_directories(bzshell PRIVATE src)
    target_link_libraries(bzshell PRIVATE
        ole32
        shell32
        shlwapi
        user32
        advapi32
    )
    target_compile_definitions(bzshell PRIVATE
        UNICODE _UNICODE NOMINMAX
        WINVER=0x0601 _WIN32_WINNT=0x0601
        BZSHELL_EXPORTS
    )
    if(MSVC)
        set_target_properties(bzshell PROPERTIES
            LINK_FLAGS_RELEASE "/MANIFEST:NO /DEBUG:NONE"
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# UPX 压缩（可选）
# ---------------------------------------------------------------------------
if(ENABLE_UPX)
    find_program(UPX upx)
    if(UPX)
        add_custom_command(TARGET bandzip POST_BUILD
            COMMAND ${UPX} --best --lzma $<TARGET_FILE:bandzip>
            COMMENT "Compressing bandzip.exe with UPX"
        )
    else()
        message(WARNING "UPX not found, skipping compression")
    endif()
endif()

# ---------------------------------------------------------------------------
# 体积检查
# ---------------------------------------------------------------------------
add_custom_target(check_size
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_size.py
        $<TARGET_FILE_DIR:bandzip>
    DEPENDS bandzip
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Checking final binary size (<5MB)"
)

# ---------------------------------------------------------------------------
# 安装
# ---------------------------------------------------------------------------
install(TARGETS bandzip RUNTIME DESTINATION bin)
if(ENABLE_SHELL_EXTENSION)
    install(TARGETS bzshell RUNTIME DESTINATION bin)
endif()
install(DIRECTORY res/skins DESTINATION bin)
install(DIRECTORY res/icons DESTINATION bin)

# ---------------------------------------------------------------------------
# 测试
# ---------------------------------------------------------------------------
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "BandzipClone configuration:")
message(STATUS "  Version:               ${PROJECT_VERSION}")
message(STATUS "  Static CRT:            ${USE_STATIC_CRT}")
message(STATUS "  AES encryption:        ${ENABLE_AES}")
message(STATUS "  SFX support:           ${ENABLE_SFX}")
message(STATUS "  Shell extension:       ${ENABLE_SHELL_EXTENSION}")
message(STATUS "  LTO:                   ${ENABLE_LTO}")
message(STATUS "  UPX compression:       ${ENABLE_UPX}")
message(STATUS "  Build type:            ${CMAKE_BUILD_TYPE}")
message(STATUS "")
