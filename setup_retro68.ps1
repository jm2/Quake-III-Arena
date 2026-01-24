# Retro68 Setup Script (PowerShell)
# This script sets up the Retro68 toolchain on Windows.
# Note: Retro68 requires a Unix-like environment (Cygwin or MSYS2) to build.
# This script mainly checks dependencies and invokes the build system.

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Retro68 Setup Script (Windows)" -ForegroundColor Cyan
Write-Host "=========================================="

$INSTALL_DIR = Join-Path (Get-Location) "tools\Retro68-build"
$SOURCE_DIR = Join-Path (Get-Location) "tools\Retro68-src"
$RETRO68_URL = "https://github.com/autc04/Retro68.git"

# Check if installed
if (Test-Path "$INSTALL_DIR\bin\powerpc-apple-macos-gcc.exe") {
    Write-Host "Retro68 appears to be installed in $INSTALL_DIR."
    exit 0
}

Write-Host "Retro68 not found locally."
Write-Host "NOTE: This script will download and BUILD Retro68 from source."
Write-Host "This process can take 20-60 minutes."
Write-Host "Retro68 works best on Windows via Cygwin or MSYS2."
Start-Sleep -Seconds 3

# Dependency Checking
$Dependencies = @("cmake", "git", "bison", "flex", "makeinfo", "bash")
$MissingDeps = $false

foreach ($dep in $Dependencies) {
    if (-not (Get-Command $dep -ErrorAction SilentlyContinue)) {
        Write-Host "Error: Required command '$dep' not found in PATH." -ForegroundColor Red
        $MissingDeps = $true
    }
}

# hfsutils check (hmount)
if (-not (Get-Command "hmount" -ErrorAction SilentlyContinue)) {
    Write-Host "Error: hfsutils (hmount) not found." -ForegroundColor Red
    $MissingDeps = $true
}

if ($MissingDeps) {
    Write-Host "--------------------------------------------------------" -ForegroundColor Yellow
    Write-Host "Please install missing dependencies manually." -ForegroundColor Yellow
    Write-Host "Recommended installation via Cygwin or MSYS2 (pacman)."
    Write-Host "Packages: cmake git bison flex texinfo hfsutils gcc g++ make boost libmpc-devel mpfr-devel gmp-devel"
    Write-Host "--------------------------------------------------------"
    exit 1
}

Write-Host "Cloning Retro68..." -ForegroundColor Green
New-Item -ItemType Directory -Force -Path "tools" | Out-Null

if (-not (Test-Path $SOURCE_DIR)) {
    git clone --recursive "$RETRO68_URL" "$SOURCE_DIR"
} else {
    Write-Host "Source directory exists, pulling updates..."
    Push-Location "$SOURCE_DIR"
    git pull
    git submodule update --init --recursive
    Pop-Location
}

# Patch CMakeLists.txt for Boost 'system' dependency (same logic as bash script)
Write-Host "Patching CMakeLists.txt files to remove Boost::system dependency..." -ForegroundColor Green
Get-ChildItem -Path "$SOURCE_DIR" -Recurse -Filter "CMakeLists.txt" | ForEach-Object {
    $content = Get-Content $_.FullName
    if ($content -match "find_package\(Boost.*system") {
        # Simple string replacement in PowerShell
        $newContent = $content -replace " system", ""
        Set-Content -Path $_.FullName -Value $newContent
    }
}

Write-Host "Building Retro68 Toolchain..." -ForegroundColor Green
Write-Host "Invoking build-toolchain.bash via bash..."

# Convert paths to Unix style for bash
$InstallDirUnix = $INSTALL_DIR -replace '\\','/'
$SourceDirUnix = $SOURCE_DIR -replace '\\','/'

# We assume 'bash' is available (checked in dependencies)
# We need to run the bash script. 
# Note: Windows path handling in bash can be tricky.
bash -c "cd '$SourceDirUnix' && ./build-toolchain.bash --prefix='$InstallDirUnix' --clean-after-build"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Retro68 installed to $INSTALL_DIR" -ForegroundColor Green
} else {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}
