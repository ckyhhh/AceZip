# ============================================================================
# BandzipClone - 类 Bandizip 的开源解压缩软件
# Copyright (C) 2024 BandzipClone Contributors
# Licensed under AGPLv3
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(AceZip
    VERSION 0.9.0
    DESCRIPTION "Open-source Bandizip-like archiver (AGPLv3)"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# 全局选项
# ---------------------------------------------------------------------------
option(USE_STATIC_CRT          "Use static C runtime (/MT)"              ON)
option(ENABLE_AES              "Enable AES encryption"                   ON)
option(ENABLE_SFX              "Enable self-extracting archives"         ON)
option(ENABLE_SHELL_EXTENSION  "Build Shell extension DLL"               ON)
option(ENABLE_TESTS            "Build unit tests"                        OFF)
option(ENABLE_LTO              "Enable Link-Time Optimization"          ON)
option(ENABLE_UPX              "Compress final binary with UPX"          OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# 仅支持 Windows
if(NOT WIN32)
    message(FATAL_ERROR "AceZip only supports Windows.")
endif()

# ---------------------------------------------------------------------------
# 静态 CRT
# ---------------------------------------------------------------------------
if(USE_STATIC_CRT AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# LTO
# ---------------------------------------------------------------------------
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# 输出目录
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# 第三方库根目录
# ---------------------------------------------------------------------------
set(TP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ===========================================================================
# zlib (1.3.1)
# ===========================================================================
set(ZLIB_ROOT ${TP_DIR}/zlib-1.3.1)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${ZLIB_ROOT} ${CMAKE_BINARY_DIR}/zlib EXCLUDE_FROM_ALL)
set_target_properties(zlibstatic PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_INCLUDE_DIRS ${ZLIB_ROOT})

# ===========================================================================
# minizip-ng (4.0.7)
# ===========================================================================
set(MZ_ROOT ${TP_DIR}/minizip-ng-4.0.7)
set(MZ_PROJECT_NAME "minizip-ng" CACHE STRING "" FORCE)
set(MZ_LIB_SUFFIX "" CACHE STRING "" FORCE)
set(MZ_COMPAT ON CACHE BOOL "" FORCE)
set(MZ_ZLIB ON CACHE BOOL "" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "" FORCE)
set(MZ_LZMA OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL OFF CACHE BOOL "" FORCE)
set(MZ_LIBCOMP OFF CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS OFF CACHE BOOL "" FORCE)
set(MZ_ICONV OFF CACHE BOOL "" FORCE)
add_subdirectory(${MZ_ROOT} ${CMAKE_BINARY_DIR}/minizip-ng EXCLUDE_FROM_ALL)
set(MINIZIP_LIBRARIES minizip)
set(MINIZIP_INCLUDE_DIRS ${MZ_ROOT}/include)

# ===========================================================================
# bzip2 (1.0.8) - 无 CMakeLists，手动编译
# ===========================================================================
set(BZIP2_ROOT ${TP_DIR}/bzip2-1.0.8)
add_library(bzip2 STATIC
    ${BZIP2_ROOT}/blocksort.c
    ${BZIP2_ROOT}/huffman.c
    ${BZIP2_ROOT}/crctable.c
    ${BZIP2_ROOT}/randtable.c
    ${BZIP2_ROOT}/compress.c
    ${BZIP2_ROOT}/decompress.c
    ${BZIP2_ROOT}/bzlib.c
)
target_include_directories(bzip2 PUBLIC ${BZIP2_ROOT})
set_target_properties(bzip2 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)
set(BZIP2_LIBRARIES bzip2)
set(BZIP2_INCLUDE_DIRS ${BZIP2_ROOT})

# ===========================================================================
# xz / liblzma (5.6.3)
# ===========================================================================
set(XZ_ROOT ${TP_DIR}/xz-5.6.3)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_LIBLZMA ON CACHE BOOL "" FORCE)
set(XZ_BUILD_LIBLZMA_LZMA2ENC ON CACHE BOOL "" FORCE)
set(XZ_BUILD_LZMADEC OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_LZMAINFO OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_XZ OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_XZDEC OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_SCRIPTS OFF CACHE BOOL "" FORCE)
set(XZ_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(XZ_ENABLE_SANDBOX OFF CACHE BOOL "" FORCE)
add_subdirectory(${XZ_ROOT} ${CMAKE_BINARY_DIR}/xz EXCLUDE_FROM_ALL)
set_target_properties(liblzma PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)
set(LZMA_LIBRARIES liblzma)
set(LZMA_INCLUDE_DIRS ${XZ_ROOT}/src/liblzma/api)

# ===========================================================================
# zstd (1.5.6)
# ===========================================================================
set(ZSTD_ROOT ${TP_DIR}/zstd-1.5.6)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
add_subdirectory(${ZSTD_ROOT}/build/cmake ${CMAKE_BINARY_DIR}/zstd EXCLUDE_FROM_ALL)
set_target_properties(libzstd_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)
set(ZSTD_LIBRARIES libzstd_static)
set(ZSTD_INCLUDE_DIRS ${ZSTD_ROOT}/lib)

# ===========================================================================
# lz4 (1.10.0)
# ===========================================================================
set(LZ4_ROOT ${TP_DIR}/lz4-1.10.1)
set(LZ4_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
add_subdirectory(${LZ4_ROOT}/build/cmake ${CMAKE_BINARY_DIR}/lz4 EXCLUDE_FROM_ALL)
set_target_properties(lz4_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)
set(LZ4_LIBRARIES lz4_static)
set(LZ4_INCLUDE_DIRS ${LZ4_ROOT}/lib)

# ===========================================================================
# LZMA SDK (23.01) - 7z 格式读写
# ===========================================================================
set(LZMA_SDK_DIR ${TP_DIR}/lzma2301/C)
add_library(lzma_sdk STATIC
    ${LZMA_SDK_DIR}/7zArcIn.c
    ${LZMA_SDK_DIR}/7zAlloc.c
    ${LZMA_SDK_DIR}/7zBuf.c
    ${LZMA_SDK_DIR}/7zCrc.c
    ${LZMA_SDK_DIR}/7zCrcOpt.c
    ${LZMA_SDK_DIR}/7zDec.c
    ${LZMA_SDK_DIR}/7zFile.c
    ${LZMA_SDK_DIR}/7zStream.c
    ${LZMA_SDK_DIR}/Aes.c
    ${LZMA_SDK_DIR}/AesOpt.c
    ${LZMA_SDK_DIR}/Alloc.c
    ${LZMA_SDK_DIR}/Bcj2.c
    ${LZMA_SDK_DIR}/Bcj2Enc.c
    ${LZMA_SDK_DIR}/Bra.c
    ${LZMA_SDK_DIR}/Bra86.c
    ${LZMA_SDK_DIR}/BraIA64.c
    ${LZMA_SDK_DIR}/CpuArch.c
    ${LZMA_SDK_DIR}/Delta.c
    ${LZMA_SDK_DIR}/LzFind.c
    ${LZMA_SDK_DIR}/LzFindOpt.c
    ${LZMA_SDK_DIR}/Lzma2Dec.c
    ${LZMA_SDK_DIR}/Lzma2DecMt.c
    ${LZMA_SDK_DIR}/Lzma2Enc.c
    ${LZMA_SDK_DIR}/LzmaDec.c
    ${LZMA_SDK_DIR}/LzmaEnc.c
    ${LZMA_SDK_DIR}/Ppmd7.c
    ${LZMA_SDK_DIR}/Ppmd7Dec.c
    ${LZMA_SDK_DIR}/Ppmd7Enc.c
    ${LZMA_SDK_DIR}/Sha256.c
    ${LZMA_SDK_DIR}/Sha256Opt.c
    ${LZMA_SDK_DIR}/Xz.c
    ${LZMA_SDK_DIR}/XzCrc64.c
    ${LZMA_SDK_DIR}/XzCrc64Opt.c
    ${LZMA_SDK_DIR}/XzDec.c
    ${LZMA_SDK_DIR}/XzEnc.c
)
target_include_directories(lzma_sdk PUBLIC ${LZMA_SDK_DIR})
target_compile_definitions(lzma_sdk PRIVATE _7ZIP_ST)
set_target_properties(lzma_sdk PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)

# ===========================================================================
# unRAR (7.x) - RAR 解压
# unRAR 的 rar.cpp 通过 #include 包含了所有其他 .cpp 文件，
# 所以只需编译 rar.cpp 一个文件即可（否则会重复定义符号）
# ===========================================================================
set(UNRAR_ROOT ${TP_DIR}/unrar)
add_library(unrar STATIC ${UNRAR_ROOT}/rar.cpp)
target_include_directories(unrar PUBLIC ${UNRAR_ROOT})
target_compile_definitions(unrar PRIVATE
    SILENT RAR_NOCRYPT UNRAR _CONSOLE
    _CRT_SECURE_NO_WARNINGS NOMINMAX
    _UNICODE UNICODE
    WINVER=0x0601 _WIN32_WINNT=0x0601)
set_target_properties(unrar PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ON)

# ===========================================================================
# Duilib - UI 框架
# ===========================================================================
set(DUILIB_ROOT ${TP_DIR}/duilib)
set(DUILIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(DUILIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(${DUILIB_ROOT}/DuiLib ${CMAKE_BINARY_DIR}/duilib EXCLUDE_FROM_ALL)
set(DUILIB_LIBRARIES DuiLib)
set(DUILIB_INCLUDE_DIRS ${DUILIB_ROOT})

# ---------------------------------------------------------------------------
# 主程序源文件
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/app/app.cpp
    src/app/command_line.cpp
    src/app/file_association.cpp
    src/app/sfx.cpp
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
    src/ui/main_window.cpp
    src/ui/dialogs.cpp
    src/ui/drag_drop.cpp
    src/shell/shell_extension.cpp
    src/utils/string_utils.cpp
    src/utils/file_utils.cpp
    src/utils/path_utils.cpp
    src/utils/logger.cpp
)

add_executable(acezip WIN32 ${APP_SOURCES})

target_include_directories(acezip PRIVATE
    src
    ${DUILIB_INCLUDE_DIRS}
    ${MINIZIP_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${BZIP2_INCLUDE_DIRS}
    ${LZMA_INCLUDE_DIRS}
    ${ZSTD_INCLUDE_DIRS}
    ${LZ4_INCLUDE_DIRS}
    ${LZMA_SDK_DIR}
    ${UNRAR_ROOT}
)

target_link_libraries(acezip PRIVATE
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

target_compile_definitions(acezip PRIVATE
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
    _CRT_SECURE_NO_WARNINGS
    BANDZIP_VERSION="${PROJECT_VERSION}"
    BANDZIP_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BANDZIP_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BANDZIP_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(MSVC)
    target_compile_options(acezip PRIVATE /W3 /EHsc /utf-8)
    set_target_properties(acezip PROPERTIES
        LINK_FLAGS "/MANIFEST:NO /DEBUG:NONE"
        LINK_FLAGS_MINSIZEREL "/MANIFEST:NO /DEBUG:NONE /SUBSYSTEM:WINDOWS,5.01"
        LINK_FLAGS_RELEASE "/MANIFEST:NO /DEBUG:NONE /SUBSYSTEM:WINDOWS,5.01"
    )
endif()

# 复制资源到输出目录
add_custom_command(TARGET acezip POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res/skins
        $<TARGET_FILE_DIR:acezip>/skins
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res/icons
        $<TARGET_FILE_DIR:acezip>/icons
    COMMENT "Copying resources to output directory"
)

# ---------------------------------------------------------------------------
# Shell 扩展 DLL
# ---------------------------------------------------------------------------
if(ENABLE_SHELL_EXTENSION)
    add_library(bzshell SHARED
        src/shell/shell_extension.cpp
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
        _CRT_SECURE_NO_WARNINGS
    )
    if(MSVC)
        target_compile_options(bzshell PRIVATE /W3 /EHsc /utf-8)
        set_target_properties(bzshell PROPERTIES
            LINK_FLAGS_MINSIZEREL "/MANIFEST:NO /DEBUG:NONE"
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
        add_custom_command(TARGET acezip POST_BUILD
            COMMAND ${UPX} --best --lzma $<TARGET_FILE:acezip>
            COMMENT "Compressing acezip.exe with UPX"
        )
    else()
        message(WARNING "UPX not found, skipping compression")
    endif()
endif()

# ---------------------------------------------------------------------------
# 体积检查
# ---------------------------------------------------------------------------
find_program(PYTHON_EXECUTABLE python)
add_custom_target(check_size
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_size.py
        $<TARGET_FILE_DIR:acezip>
    DEPENDS acezip
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Checking final binary size (<5MB)"
)

# ---------------------------------------------------------------------------
# 安装
# ---------------------------------------------------------------------------
install(TARGETS acezip RUNTIME DESTINATION bin)
if(ENABLE_SHELL_EXTENSION)
    install(TARGETS bzshell RUNTIME DESTINATION bin)
endif()
install(DIRECTORY res/skins DESTINATION bin)
install(DIRECTORY res/icons DESTINATION bin)

message(STATUS "")
message(STATUS "AceZip configuration:")
message(STATUS "  Version:               ${PROJECT_VERSION}")
message(STATUS "  Static CRT:            ${USE_STATIC_CRT}")
message(STATUS "  AES encryption:        ${ENABLE_AES}")
message(STATUS "  SFX support:           ${ENABLE_SFX}")
message(STATUS "  Shell extension:       ${ENABLE_SHELL_EXTENSION}")
message(STATUS "  LTO:                   ${ENABLE_LTO}")
message(STATUS "  UPX compression:       ${ENABLE_UPX}")
message(STATUS "  Build type:            ${CMAKE_BUILD_TYPE}")
message(STATUS "")
