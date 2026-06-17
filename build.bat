@echo off
setlocal enabledelayedexpansion
REM ===========================================================================
REM  One-shot Pythia build for Windows.
REM
REM  Produces a ready-to-deploy @Pythia mod folder (binaries + addon PBOs +
REM  templates). Copy @Pythia into your Arma 3 directory and enable it.
REM
REM  Usage:
REM     build.bat            Build x64 binaries + PBOs (recommended)
REM     build.bat x86        Also build 32-bit binaries (needs 32-bit Python)
REM     build.bat nopbo      Skip building addon PBOs (binaries only)
REM
REM  Requirements:
REM     - The Python version in .github\workflows\build.yml, WITH development
REM       headers ("Download debug binaries" / py launcher), 64-bit.
REM     - CMake and Visual Studio 2017/2019/2022/2026 (Desktop C++ workload).
REM     - For PBOs: Mikero's DePboTools (makepbo). Skipped with a warning if
REM       absent (you can drop in prebuilt PBOs from a release instead).
REM ===========================================================================

cd /d "%~dp0"

set "BUILD_X86=0"
set "BUILD_PBO=1"
for %%A in (%*) do (
    if /I "%%A"=="x86"   set "BUILD_X86=1"
    if /I "%%A"=="nopbo" set "BUILD_PBO=0"
)

echo ========================================
echo Building Pythia (Windows)
echo ========================================

REM --- Read the target Python version (single source of truth) --------------
for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "[regex]::Match((Get-Content '.github/workflows/build.yml' -Raw),'PYTHON_VERSION:\s*([0-9.]+)').Groups[1].Value"`) do set "PYVER=%%v"
if "%PYVER%"=="" (
    echo ERROR: Could not read PYTHON_VERSION from .github\workflows\build.yml
    exit /b 1
)
for /f "tokens=1,2 delims=." %%a in ("%PYVER%") do set "PYVER_MM=%%a.%%b"
echo Target Python: %PYVER% (%PYVER_MM%)

REM --- Locate a Python interpreter for staging/PBO steps ---------------------
set "PYCMD="
py -%PYVER_MM% -c "import sys" >nul 2>&1 && set "PYCMD=py -%PYVER_MM%"
if not defined PYCMD ( where python >nul 2>&1 && set "PYCMD=python" )
if not defined PYCMD (
    echo ERROR: Python not found. Install Python %PYVER_MM% from https://python.org
    exit /b 1
)

REM --- Check development headers (Python.h) ----------------------------------
%PYCMD% -c "import os,sysconfig;raise SystemExit(0 if os.path.exists(os.path.join(sysconfig.get_path('include'),'Python.h')) else 1)"
if errorlevel 1 (
    echo.
    echo ERROR: Python development headers not found.
    echo Reinstall Python with the development headers, or modify the install
    echo via Settings ^> Apps ^> Python ^> Modify and tick the dev tools.
    exit /b 1
)
echo Python development headers: OK

REM --- Check CMake -----------------------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files\CMake\bin;%PATH%"
    ) else (
        echo ERROR: CMake not found. Install from https://cmake.org/download/
        exit /b 1
    )
)

REM --- Detect Visual Studio via vswhere -> CMake generator -------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_VERSION="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -property catalog_productLineVersion 2^>nul`) do set "VS_VERSION=%%i"
)

set "VS_GEN="
if "%VS_VERSION%"=="18" set "VS_GEN=Visual Studio 18 2026"
if "%VS_VERSION%"=="17" set "VS_GEN=Visual Studio 17 2022"
if "%VS_VERSION%"=="16" set "VS_GEN=Visual Studio 16 2019"
if "%VS_VERSION%"=="15" set "VS_GEN=Visual Studio 15 2017"
if not defined VS_GEN (
    echo ERROR: Visual Studio 2017/2019/2022/2026 not detected.
    echo Install it with the "Desktop development with C++" workload.
    exit /b 1
)
echo CMake generator: %VS_GEN%

if not exist "@Pythia" mkdir "@Pythia"

REM --- Build x64 -------------------------------------------------------------
call :build_arch x64 x64 build_x64 || exit /b 1

REM --- Optionally build x86 --------------------------------------------------
if "%BUILD_X86%"=="1" (
    call :build_arch x86 Win32 build_x86 || exit /b 1
)

REM --- Stage templates into @Pythia -----------------------------------------
echo.
echo Staging templates...
%PYCMD% tools\stage_templates.py %PYVER%
if errorlevel 1 ( echo ERROR: Failed to stage templates & exit /b 1 )

REM --- Build addon PBOs ------------------------------------------------------
if "%BUILD_PBO%"=="1" (
    echo.
    echo Building addon PBOs...
    %PYCMD% tools\create_pbos.py
    if errorlevel 1 (
        echo.
        echo WARNING: PBO build failed ^(Mikero's makepbo missing or signing error^).
        echo The binaries are ready in @Pythia, but the addon PBOs are not.
        echo Install Mikero's DePboTools and re-run, or copy prebuilt PBOs from a
        echo release into @Pythia\addons and the key into @Pythia\keys.
    )
)

echo.
echo ========================================
echo Build complete: @Pythia
echo ========================================
echo Deploy:
echo   1. Copy the @Pythia folder into your Arma 3 directory.
echo   2. Install Python %PYVER_MM% (64-bit) on every machine that runs it.
echo   3. Enable @Pythia (and disable BattlEye) in the Arma 3 Launcher.
echo See USAGE.md for how to write Python extensions.
exit /b 0

REM ===========================================================================
REM  :build_arch <label> <-A arch> <build dir>
REM ===========================================================================
:build_arch
echo.
echo Building %~1 binaries...
if exist "%~3" rmdir /S /Q "%~3"
mkdir "%~3"
cmake -S . -B "%~3" -G "%VS_GEN%" -A %~2
if errorlevel 1 ( echo ERROR: CMake configuration failed (%~1) & exit /b 1 )
cmake --build "%~3" --config Release
if errorlevel 1 ( echo ERROR: Build failed (%~1) & exit /b 1 )
exit /b 0
