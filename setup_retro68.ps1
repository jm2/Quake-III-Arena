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
$Dependencies = @("cmake", "git", "bison", "flex", "makeinfo", "bash", "unar")
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

# 2. Prepare InterfacesAndLibraries
Write-Host "Step 2: Preparing SDKs..." -ForegroundColor Green
$SDK_DEST = Join-Path $SOURCE_DIR "InterfacesAndLibraries"
if (-not (Test-Path $SDK_DEST)) {
    New-Item -ItemType Directory -Path $SDK_DEST | Out-Null
}

$MPW_URL = "https://download.macintoshgarden.org/apps/MPW_fully_updated.sit"
$OPENGL_URL = "https://download.macintoshgarden.org/apps/OpenGL_SDK_1.2.sit"

# Helper for download
function Download-FileIfMissing {
    param($Url, $Dest)
    if (-not (Test-Path $Dest)) {
        Write-Host "Downloading $dest..."
        Invoke-WebRequest -Uri $Url -OutFile $Dest
    }
}

Download-FileIfMissing -Url $MPW_URL -Dest "tools\MPW_fully_updated.sit"
Download-FileIfMissing -Url $OPENGL_URL -Dest "tools\OpenGL_SDK_1.2.sit"

# Extract function using unar (assuming available as checked)
function Extract-Site {
    param($File, $DestDir)
    if (Test-Path $DestDir) { Remove-Item -Recurse -Force $DestDir }
    Write-Host "Extracting $File..."
    # unar usage: unar -f file.sit -o output_dir
    # we need to be careful with paths in powershell calling binaries
    & unar -f $File -o $DestDir | Out-Null
}

# Process MPW
Extract-Site -File "tools\MPW_fully_updated.sit" -DestDir "tools\temp_mpw"
$MPW_I_AND_L = Get-ChildItem -Path "tools\temp_mpw" -Recurse -Directory -Filter "Interfaces&Libraries" | Select-Object -First 1
if ($MPW_I_AND_L) {
    Write-Host "Injecting MPW Interfaces&Libraries..."
    Copy-Item -Path "$($MPW_I_AND_L.FullName)\*" -Destination $SDK_DEST -Recurse -Force
} else {
    Write-Host "Error: Could not find Interfaces&Libraries in MPW" -ForegroundColor Red
    exit 1
}

# Process OpenGL
Extract-Site -File "tools\OpenGL_SDK_1.2.sit" -DestDir "tools\temp_opengl"
$OGL_LIBS = Get-ChildItem -Path "tools\temp_opengl" -Recurse -Directory -Filter "Libraries" | Select-Object -First 1
$OGL_HEADERS = Get-ChildItem -Path "tools\temp_opengl" -Recurse -Directory -Filter "Headers" | Select-Object -First 1

if (-not (Test-Path "$SDK_DEST\Libraries")) { New-Item -ItemType Directory -Path "$SDK_DEST\Libraries" | Out-Null }
if (-not (Test-Path "$SDK_DEST\Interfaces\CIncludes")) { New-Item -ItemType Directory -Path "$SDK_DEST\Interfaces\CIncludes" | Out-Null }

if ($OGL_LIBS) {
    Write-Host "Injecting OpenGL Libraries..."
    Copy-Item -Path "$($OGL_LIBS.FullName)\*" -Destination "$SDK_DEST\Libraries" -Recurse -Force
}
if ($OGL_HEADERS) {
    Write-Host "Injecting OpenGL Headers..."
    Copy-Item -Path "$($OGL_HEADERS.FullName)\*" -Destination "$SDK_DEST\Interfaces\CIncludes" -Recurse -Force
}

# Cleanup Temps
Remove-Item -Recurse -Force "tools\temp_mpw" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "tools\temp_opengl" -ErrorAction SilentlyContinue

# Prune Conflicting Headers
Write-Host "Pruning conflicting MPW headers..." -ForegroundColor Yellow
$ConflictingHeaders = @("assert.h", "ctype.h", "errno.h", "float.h", "limits.h", "locale.h", "math.h", "setjmp.h", "signal.h", "stdarg.h", "stddef.h", "stdio.h", "stdlib.h", "string.h", "time.h")
foreach ($header in $ConflictingHeaders) {
    Get-ChildItem -Path "$SDK_DEST\Interfaces\CIncludes" -Filter $header -Recurse | Remove-Item -Force
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
