# PowerShell build script for trace-scope using Ninja + Clang
# Usage: .\build_ninja_clang.ps1 [-Clean] [-Test] [-DoubleBuf]

param(
    [switch]$Clean,
    [switch]$Test,
    [switch]$DoubleBuf
)

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Building with Ninja + Clang" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

$BuildDir = "build_ninja_clang"

# Check for Clang
$clang = Get-Command clang++ -ErrorAction SilentlyContinue
if (-not $clang) {
    Write-Host "ERROR: clang++ not found in PATH" -ForegroundColor Red
    Write-Host "Please install LLVM/Clang and add to PATH" -ForegroundColor Yellow
    Write-Host "Download from: https://github.com/llvm/llvm-project/releases" -ForegroundColor Yellow
    exit 1
}

# Get Clang version
$clangVersion = & clang++ --version | Select-String "version" | ForEach-Object { $_ -replace '.*version\s+(\S+).*', '$1' }
Write-Host "Found Clang version: $clangVersion" -ForegroundColor Green

# Check for Ninja
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Ninja not found in PATH" -ForegroundColor Red
    Write-Host "Download from: https://github.com/ninja-build/ninja/releases" -ForegroundColor Yellow
    exit 1
}

$ninjaVersion = & ninja --version
Write-Host "Found Ninja version: $ninjaVersion" -ForegroundColor Green
Write-Host ""

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning $BuildDir directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir

try {
    # Configure CMake
    Write-Host "Configuring CMake (Ninja + Clang)..." -ForegroundColor Cyan
    
    $cmakeArgs = @(
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++"
    )
    
    if ($DoubleBuf) {
        $cmakeArgs += "-DTRACE_DOUBLE_BUFFER=ON"
        Write-Host "  - Double-buffering: ENABLED" -ForegroundColor Yellow
    } else {
        Write-Host "  - Double-buffering: disabled (default)" -ForegroundColor Gray
    }
    
    & cmake @cmakeArgs ..
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nERROR: CMake configuration failed" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    
    # Build
    Write-Host "`nBuilding..." -ForegroundColor Cyan
    cmake --build . --config Release
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nERROR: Build failed" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host "  Build Successful!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Build directory: $BuildDir" -ForegroundColor White
    Write-Host "Compiler: Clang $clangVersion" -ForegroundColor White
    Write-Host ""
    
    # Run tests if requested
    if ($Test) {
        Write-Host "`nRunning tests..." -ForegroundColor Cyan
        Write-Host "========================================`n" -ForegroundColor Cyan
        
        $passed = 0
        $failed = 0
        
        Get-ChildItem -Path tests -Filter *.exe -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "Running $($_.BaseName)... " -NoNewline
            & $_.FullName | Out-Null
            if ($LASTEXITCODE -eq 0) {
                Write-Host "PASSED" -ForegroundColor Green
                $passed++
            } else {
                Write-Host "FAILED" -ForegroundColor Red
                $failed++
            }
        }
        
        Write-Host "`n========================================" -ForegroundColor Cyan
        Write-Host "Results: $passed passed, $failed failed" -ForegroundColor $(if($failed -eq 0){"Green"}else{"Red"})
        Write-Host "========================================`n" -ForegroundColor Cyan
    }
    
} finally {
    Pop-Location
}

Write-Host "Done!" -ForegroundColor Green

