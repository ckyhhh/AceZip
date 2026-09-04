@echo off
REM ============================================================================
REM build.bat - Windows 构建脚本
REM
REM 用法:
REM   build.bat [Release|Debug|MinSizeRel] [x86|x64]
REM
REM BandzipClone - 类 Bandizip 的开源解压缩软件
REM Copyright (C) 2024 BandzipClone Contributors
REM Licensed under AGPLv3
REM ============================================================================

setlocal enabledelayedexpansion

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=MinSizeRel

set ARCH=%2
if "%ARCH%"=="" set ARCH=x64

set BUILD_DIR=build\%BUILD_TYPE%-%ARCH%
set SOURCE_DIR=%~dp0

echo ============================================================
echo BandzipClone Build Script
echo ============================================================
echo Build Type: %BUILD_TYPE%
echo Architecture: %ARCH%
echo Source Dir: %SOURCE_DIR%
echo Build Dir: %BUILD_DIR%
echo.

REM 检查 CMake
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found. Please install CMake 3.16+.
    exit /b 1
)

REM 检查 Visual Studio
set VS_GENERATOR=
if "%ARCH%"=="x64" (
    set VS_GENERATOR=-G "Visual Studio 17 2022" -A x64
) else (
    set VS_GENERATOR=-G "Visual Studio 17 2022" -A Win32
)

REM 创建构建目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM CMake 配置
echo [1/4] Configuring CMake...
cmake %SOURCE_DIR% %VS_GENERATOR% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DUSE_STATIC_CRT=ON ^
    -DENABLE_LTO=ON ^
    -DENABLE_AES=ON ^
    -DENABLE_SFX=ON ^
    -DENABLE_SHELL_EXTENSION=ON ^
    -B "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

REM 编译
echo.
echo [2/4] Building...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM 复制资源
echo.
echo [3/4] Copying resources...
set OUTPUT_DIR=%BUILD_DIR%\%BUILD_TYPE%
if not exist "%OUTPUT_DIR%\skins" mkdir "%OUTPUT_DIR%\skins"
if not exist "%OUTPUT_DIR%\icons" mkdir "%OUTPUT_DIR%\icons"
xcopy /E /I /Y "res\skins" "%OUTPUT_DIR%\skins" >nul
xcopy /E /I /Y "res\icons" "%OUTPUT_DIR%\icons" >nul

REM 体积检查
echo.
echo [4/4] Checking size...
python scripts\check_size.py "%OUTPUT_DIR%"

echo.
echo ============================================================
echo Build completed successfully!
echo ============================================================
echo Output: %OUTPUT_DIR%
echo.

REM 显示产物
dir "%OUTPUT_DIR%\*.exe" "%OUTPUT_DIR%\*.dll"
