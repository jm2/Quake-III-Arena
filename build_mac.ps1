# Quake III Arena Build Script for Windows (Retro68)
# Builds the monolithic Classic Mac OS app using the Retro68 toolchain.

$ErrorActionPreference = "Stop"

$BuildDir = "build_mac"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$ToolsDir = Join-Path (Get-Location) "tools\Retro68-build\bin"
$CompilerName = "powerpc-apple-macos-gcc.exe"
$CompilerPath = Join-Path $ToolsDir $CompilerName

# Check if compiler exists
if (-not (Test-Path $CompilerPath)) {
    # Check PATH as well
    if (-not (Get-Command "powerpc-apple-macos-gcc" -ErrorAction SilentlyContinue)) {
        Write-Host "Retro68 compiler not found." -ForegroundColor Yellow
        Write-Host "Running setup_retro68.ps1..."
        .\setup_retro68.ps1
    }
}

# Add tools dir to PATH for this session
$env:PATH = "$ToolsDir;$env:PATH"

# Enter build dir
Push-Location $BuildDir

# Configure CMake
# We assume 'make' generator. If using Ninja, user might need to adjust.
# Using -G "Unix Makefiles" or "MinGW Makefiles" usually requires MSYS/MinGW.
# If bash is present (MinGW/Cygwin), it usually brings 'make'.
Write-Host "Configuring CMake..." -ForegroundColor Green
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Retro68.toolchain.cmake -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed." -ForegroundColor Red
    Pop-Location
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Green
# Use cmake --build to be generator-agnostic (handles make, ninja, visual studio, etc)
# -j for parallel build
cmake --build . --parallel

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build complete." -ForegroundColor Green
} else {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}

Pop-Location
