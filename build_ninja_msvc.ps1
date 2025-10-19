# PowerShell build script for trace-scope using Ninja + MSVC
# Usage: .\build_ninja_msvc.ps1 [-Clean] [-Test] [-DoubleBuf]

param(
    [switch]$Clean,
    [switch]$Test,
    [switch]$DoubleBuf
)

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Building with Ninja + MSVC" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

$BuildDir = "build_ninja_msvc"

# Find Visual Studio using vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "ERROR: vswhere.exe not found. Is Visual Studio installed?" -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio installation not found" -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green

# Setup Visual Studio environment
$vcvarsall = "$vsPath\VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvarsall)) {
    Write-Host "ERROR: vcvarsall.bat not found" -ForegroundColor Red
    exit 1
}

# Import Visual Studio environment into PowerShell
cmd /c "`"$vcvarsall`" x64 & set" | ForEach-Object {
    if ($_ -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}

# Check for Ninja
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Ninja not found in PATH" -ForegroundColor Red
    Write-Host "Install ninja or ensure Visual Studio's ninja is in PATH" -ForegroundColor Yellow
    exit 1
}

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
    Write-Host "`nConfiguring CMake (Ninja + MSVC)..." -ForegroundColor Cyan
    
    $cmakeArgs = @(
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER=cl",
        "-DCMAKE_CXX_COMPILER=cl"
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

