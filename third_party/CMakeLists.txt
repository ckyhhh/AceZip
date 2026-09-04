# ============================================================================
# third_party/CMakeLists.txt - 第三方库构建配置
# ============================================================================
# 所有第三方库源码已随仓库提交到本目录，CI 无需从外部下载。
# 全部以静态库方式构建，并启用最小体积优化。
# ============================================================================

# ---------------------------------------------------------------------------
# zlib (1.3.1) - 压缩基础库
# ---------------------------------------------------------------------------
set(ZLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/zlib-1.3.1 CACHE PATH "zlib root")
if(EXISTS ${ZLIB_ROOT}/CMakeLists.txt)
    set(ZLIB_STATIC ON CACHE BOOL "" FORCE)
    set(ZLIB_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "" FORCE)
    add_subdirectory(${ZLIB_ROOT} ${CMAKE_BINARY_DIR}/zlib EXCLUDE_FROM_ALL)
    set_target_properties(zlibstatic PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
    # 暴露 zlib 头文件路径
    target_include_directories(zlibstatic PUBLIC ${ZLIB_ROOT})
endif()

# ---------------------------------------------------------------------------
# minizip-ng (4.0.7) - ZIP 格式读写
# ---------------------------------------------------------------------------
set(MZ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/minizip-ng-4.0.7 CACHE PATH "minizip-ng root")
if(EXISTS ${MZ_ROOT}/CMakeLists.txt)
    set(MZ_LIB ON CACHE BOOL "" FORCE)
    set(MZ_LIB_SUFFIX "ng" CACHE STRING "" FORCE)
    set(MZ_ZLIB ON CACHE BOOL "" FORCE)
    set(MZ_BZIP2 ON CACHE BOOL "" FORCE)
    set(MZ_LZMA ON CACHE BOOL "" FORCE)
    set(MZ_ZSTD ON CACHE BOOL "" FORCE)
    set(MZ_LIBCOMPRESS OFF CACHE BOOL "" FORCE)
    set(MZ_LIBBSD OFF CACHE BOOL "" FORCE)
    set(MZ_FETCH_LIBS OFF CACHE BOOL "" FORCE)
    set(MZ_FORCE_FETCH_LIBS OFF CACHE BOOL "" FORCE)
    set(MZ_PKCRYPT OFF CACHE BOOL "" FORCE)
    set(MZ_WZAES OFF CACHE BOOL "" FORCE)
    set(MZ_OPENSSL OFF CACHE BOOL "" FORCE)
    set(MZ_BRG ON CACHE BOOL "" FORCE)
    set(MZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${MZ_ROOT} ${CMAKE_BINARY_DIR}/minizip-ng EXCLUDE_FROM_ALL)
    set_target_properties(minizip PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# bzip2 (1.0.8) - BZ2 压缩
# ---------------------------------------------------------------------------
set(BZIP2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/bzip2-1.0.8 CACHE PATH "bzip2 root")
if(EXISTS ${BZIP2_ROOT}/blocksort.c)
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
endif()

# ---------------------------------------------------------------------------
# liblzma / XZ Utils (5.6.3) - XZ/LZMA 压缩
# ---------------------------------------------------------------------------
set(XZ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/xz-5.6.3 CACHE PATH "xz root")
if(EXISTS ${XZ_ROOT}/CMakeLists.txt)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(XZ_BUILD_BINARIES OFF CACHE BOOL "" FORCE)
    set(XZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(XZ_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_XZ OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_XZDEC OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_LZMADEC OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_LZMAINFO OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_SCRIPTS OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_SHARED OFF CACHE BOOL "" FORCE)
    set(XZ_ENABLE_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory(${XZ_ROOT} ${CMAKE_BINARY_DIR}/xz EXCLUDE_FROM_ALL)
    set_target_properties(liblzma PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# zstd (1.5.6) - Zstandard 压缩
# ---------------------------------------------------------------------------
set(ZSTD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/zstd-1.5.6 CACHE PATH "zstd root")
set(ZSTD_LIB_DIR ${ZSTD_ROOT}/lib CACHE PATH "zstd lib dir")
if(EXISTS ${ZSTD_LIB_DIR}/CMakeLists.txt)
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
    add_subdirectory(${ZSTD_LIB_DIR} ${CMAKE_BINARY_DIR}/zstd EXCLUDE_FROM_ALL)
    set_target_properties(libzstd_static PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# lz4 (1.10.0) - LZ4 压缩
# ---------------------------------------------------------------------------
set(LZ4_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lz4-1.10.1 CACHE PATH "lz4 root")
set(LZ4_LIB_DIR ${LZ4_ROOT}/lib CACHE PATH "lz4 lib dir")
if(EXISTS ${LZ4_LIB_DIR}/CMakeLists.txt)
    set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(LZ4_BUILD_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory(${LZ4_LIB_DIR} ${CMAKE_BINARY_DIR}/lz4 EXCLUDE_FROM_ALL)
    set_target_properties(lz4_static PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# LZMA SDK (23.01) - 7z 格式读写
# ---------------------------------------------------------------------------
set(LZMA_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lzma2301 CACHE PATH "lzma sdk root")
if(EXISTS ${LZMA_SDK_ROOT}/C/7z.h)
    add_library(lzma_sdk STATIC
        ${LZMA_SDK_ROOT}/C/7zAlloc.c
        ${LZMA_SDK_ROOT}/C/7zArcIn.c
        ${LZMA_SDK_ROOT}/C/7zBuf.c
        ${LZMA_SDK_ROOT}/C/7zBuf2.c
        ${LZMA_SDK_ROOT}/C/7zCrc.c
        ${LZMA_SDK_ROOT}/C/7zCrcOpt.c
        ${LZMA_SDK_ROOT}/C/7zDec.c
        ${LZMA_SDK_ROOT}/C/7zFile.c
        ${LZMA_SDK_ROOT}/C/7zStream.c
        ${LZMA_SDK_ROOT}/C/Aes.c
        ${LZMA_SDK_ROOT}/C/Alloc.c
        ${LZMA_SDK_ROOT}/C/Bcj2.c
        ${LZMA_SDK_ROOT}/C/Bra.c
        ${LZMA_SDK_ROOT}/C/Bra86.c
        ${LZMA_SDK_ROOT}/C/BraIA64.c
        ${LZMA_SDK_ROOT}/C/CpuArch.c
        ${LZMA_SDK_ROOT}/C/Delta.c
        ${LZMA_SDK_ROOT}/C/LzFind.c
        ${LZMA_SDK_ROOT}/C/Lzma2Dec.c
        ${LZMA_SDK_ROOT}/C/Lzma2Enc.c
        ${LZMA_SDK_ROOT}/C/LzmaDec.c
        ${LZMA_SDK_ROOT}/C/LzmaEnc.c
        ${LZMA_SDK_ROOT}/C/MtCoder.c
        ${LZMA_SDK_ROOT}/C/MtDec.c
        ${LZMA_SDK_ROOT}/C/Ppmd7.c
        ${LZMA_SDK_ROOT}/C/Ppmd7Dec.c
        ${LZMA_SDK_ROOT}/C/Ppmd7Enc.c
        ${LZMA_SDK_ROOT}/C/Sha256.c
        ${LZMA_SDK_ROOT}/C/Sort.c
        ${LZMA_SDK_ROOT}/C/Threads.c
        ${LZMA_SDK_ROOT}/C/Xz.c
        ${LZMA_SDK_ROOT}/C/XzCrc64.c
        ${LZMA_SDK_ROOT}/C/XzEnc.c
    )
    target_include_directories(lzma_sdk PUBLIC ${LZMA_SDK_ROOT}/C ${LZMA_SDK_ROOT}/CPP)
    target_compile_definitions(lzma_sdk PRIVATE
        _7ZIP_ST _CRT_SECURE_NO_WARNINGS)
    set_target_properties(lzma_sdk PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# unRAR (7.0.x) - RAR 解压（仅解压，许可证限制）
# ---------------------------------------------------------------------------
set(UNRAR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/unrar CACHE PATH "unrar root")
if(EXISTS ${UNRAR_ROOT}/dll.cpp)
    add_library(unrar STATIC
        ${UNRAR_ROOT}/rarvm.cpp
        ${UNRAR_ROOT}/unpack.cpp
        ${UNRAR_ROOT}/unpack15.cpp
        ${UNRAR_ROOT}/unpack20.cpp
        ${UNRAR_ROOT}/unpack30.cpp
        ${UNRAR_ROOT}/unpack50.cpp
        ${UNRAR_ROOT}/unpackinline.cpp
        ${UNRAR_ROOT}/uowners.cpp
        ${UNRAR_ROOT}/rs16.cpp
        ${UNRAR_ROOT}/recvol3.cpp
        ${UNRAR_ROOT}/recvol5.cpp
        ${UNRAR_ROOT}/rar.cpp
        ${UNRAR_ROOT}/rarpch.cpp
        ${UNRAR_ROOT}/pathfn.cpp
        ${UNRAR_ROOT}/options.cpp
        ${UNRAR_ROOT}/msg4.cpp
        ${UNRAR_ROOT}/model.cpp
        ${UNRAR_ROOT}/misc.cpp
        ${UNRAR_ROOT}/match.cpp
        ${UNRAR_ROOT}/list.cpp
        ${UNRAR_ROOT}/int64.cpp
        ${UNRAR_ROOT}/getbits.cpp
        ${UNRAR_ROOT}/filestr.cpp
        ${UNRAR_ROOT}/file.cpp
        ${UNRAR_ROOT}/extract.cpp
        ${UNRAR_ROOT}/encname.cpp
        ${UNRAR_ROOT}/errhnd.cpp
        ${UNRAR_ROOT}/extinfo.cpp
        ${UNRAR_ROOT}/extract.cpp
        ${UNRAR_ROOT}/filcreat.cpp
        ${UNRAR_ROOT}/file.cpp
        ${UNRAR_ROOT}/filefn.cpp
        ${UNRAR_ROOT}/filestr.cpp
        ${UNRAR_ROOT}/find.cpp
        ${UNRAR_ROOT}/getbits.cpp
        ${UNRAR_ROOT}/global.cpp
        ${UNRAR_ROOT}/hash.cpp
        ${UNRAR_ROOT}/headers.cpp
        ${UNRAR_ROOT}/isnt.cpp
        ${UNRAR_ROOT}/largepage.cpp
        ${UNRAR_ROOT}/secpassword.cpp
        ${UNRAR_ROOT}/sha1.cpp
        ${UNRAR_ROOT}/sha256.cpp
        ${UNRAR_ROOT}/smallfn.cpp
        ${UNRAR_ROOT}/strfn.cpp
        ${UNRAR_ROOT}/strlist.cpp
        ${UNRAR_ROOT}/system.cpp
        ${UNRAR_ROOT}/threadpool.cpp
        ${UNRAR_ROOT}/timefn.cpp
        ${UNRAR_ROOT}/ui.cpp
        ${UNRAR_ROOT}/unicode.cpp
        ${UNRAR_ROOT}/ulinks.cpp
        ${UNRAR_ROOT}/upxtool.cpp
        ${UNRAR_ROOT}/volume.cpp
        ${UNRAR_ROOT}/cmddata.cpp
        ${UNRAR_ROOT}/coder.cpp
        ${UNRAR_ROOT}/consio.cpp
        ${UNRAR_ROOT}/crc.cpp
        ${UNRAR_ROOT}/archive.cpp
        ${UNRAR_ROOT}/arcread.cpp
        ${UNRAR_ROOT}/blake2s.cpp
        ${UNRAR_ROOT}/blake2sp.cpp
        ${UNRAR_ROOT}/blake2s_sse.cpp
        ${UNRAR_ROOT}/cmdfilter.cpp
        ${UNRAR_ROOT}/cmdmix.cpp
        ${UNRAR_ROOT}/cpu.cpp
        ${UNRAR_ROOT}/crypt.cpp
        ${UNRAR_ROOT}/dll.cpp
    )
    target_include_directories(unrar PUBLIC ${UNRAR_ROOT})
    target_compile_definitions(unrar PRIVATE
        SILENT RAR_NOCRYPT UNRAR _CONSOLE
        _CRT_SECURE_NO_WARNINGS NOMINMAX)
    set_target_properties(unrar PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# Duilib (master) - UI 框架
# ---------------------------------------------------------------------------
set(DUILIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/duilib CACHE PATH "duilib root")
if(EXISTS ${DUILIB_ROOT}/DuiLib/CMakeLists.txt)
    set(DUILIB_STATIC ON CACHE BOOL "" FORCE)
    set(DUILIB_SHARED OFF CACHE BOOL "" FORCE)
    set(DUILIB_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    add_subdirectory(${DUILIB_ROOT}/DuiLib
                      ${CMAKE_BINARY_DIR}/duilib EXCLUDE_FROM_ALL)
    set_target_properties(DuiLib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()
