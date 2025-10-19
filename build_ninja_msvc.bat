@echo off
REM Build script for trace-scope using Ninja + MSVC
REM Usage: build_ninja_msvc.bat [clean|test]

setlocal enabledelayedexpansion

echo ========================================
echo   Building with Ninja + MSVC
echo ========================================
echo.

REM Parse arguments
set DO_CLEAN=0
set RUN_TESTS=0
if "%1"=="clean" set DO_CLEAN=1
if "%1"=="test" set RUN_TESTS=1
if "%2"=="test" set RUN_TESTS=1

REM Find Visual Studio installation using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Is Visual Studio installed?
    exit /b 1
)

REM Get Visual Studio installation path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo ERROR: Visual Studio not found
    exit /b 1
)

echo Found Visual Studio at: %VS_PATH%

REM Setup Visual Studio environment for x64
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo ERROR: Failed to setup Visual Studio environment
    exit /b 1
)

REM Check for Ninja
where ninja >nul 2>&1
if errorlevel 1 (
    echo ERROR: Ninja not found in PATH
    echo Install ninja or ensure Visual Studio's ninja is in PATH
    exit /b 1
)

REM Clean if requested
if %DO_CLEAN%==1 (
    echo Cleaning build_ninja_msvc directory...
    if exist build_ninja_msvc rmdir /s /q build_ninja_msvc
)

REM Create build directory
if not exist build_ninja_msvc mkdir build_ninja_msvc
cd build_ninja_msvc

REM Configure with CMake + Ninja + MSVC
echo.
echo Configuring CMake (Ninja + MSVC)...
cmake -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=cl ^
    -DCMAKE_CXX_COMPILER=cl ^
    ..

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    cd ..
    exit /b 1
)

REM Build
echo.
echo Building...
cmake --build . --config Release

if errorlevel 1 (
    echo ERROR: Build failed
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo Build directory: build_ninja_msvc
echo.

REM Run tests if requested
if %RUN_TESTS%==1 (
    echo.
    echo Running tests...
    echo ========================================
    
    for %%t in (tests\*.exe) do (
        echo Running %%~nt...
        %%t
        if errorlevel 1 (
            echo   FAILED: %%~nt
        ) else (
            echo   PASSED: %%~nt
        )
        echo.
    )
)

cd ..
echo Done!

