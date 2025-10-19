@echo off
REM Build script for trace-scope using Ninja + Clang
REM Usage: build_ninja_clang.bat [clean|test]

setlocal enabledelayedexpansion

echo ========================================
echo   Building with Ninja + Clang
echo ========================================
echo.

REM Parse arguments
set DO_CLEAN=0
set RUN_TESTS=0
if "%1"=="clean" set DO_CLEAN=1
if "%1"=="test" set RUN_TESTS=1
if "%2"=="test" set RUN_TESTS=1

REM Check for Clang
where clang++ >nul 2>&1
if errorlevel 1 (
    echo ERROR: clang++ not found in PATH
    echo Please install LLVM/Clang and add to PATH
    exit /b 1
)

REM Get Clang version
for /f "tokens=3" %%v in ('clang++ --version ^| findstr "version"') do (
    set CLANG_VERSION=%%v
    goto :found_version
)
:found_version
echo Found Clang version: %CLANG_VERSION%

REM Check for Ninja
where ninja >nul 2>&1
if errorlevel 1 (
    echo ERROR: Ninja not found in PATH
    echo Install ninja from: https://github.com/ninja-build/ninja/releases
    exit /b 1
)

REM Clean if requested
if %DO_CLEAN%==1 (
    echo Cleaning build_ninja_clang directory...
    if exist build_ninja_clang rmdir /s /q build_ninja_clang
)

REM Create build directory
if not exist build_ninja_clang mkdir build_ninja_clang
cd build_ninja_clang

REM Configure with CMake + Ninja + Clang
echo.
echo Configuring CMake (Ninja + Clang)...
cmake -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=clang ^
    -DCMAKE_CXX_COMPILER=clang++ ^
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
echo Build directory: build_ninja_clang
echo Compiler: Clang %CLANG_VERSION%
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

