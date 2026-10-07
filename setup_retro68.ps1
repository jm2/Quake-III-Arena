# Retro68 Setup Script (PowerShell)
# This script sets up the Retro68 toolchain on Windows.
# Note: Retro68 requires a Unix-like environment (Cygwin or MSYS2) to build.
# This script mainly checks dependencies and invokes the build system.

$ErrorActionPreference = "Stop"

# Work from the repository root whatever the caller's location (issue #231):
# run again from there, then give the caller back its location.
if (-not $Quake3AtRepositoryRoot -and (Get-Location).ProviderPath -ne $PSScriptRoot) {
    $Quake3AtRepositoryRoot = $true
    Push-Location -LiteralPath $PSScriptRoot -StackName Quake3Caller
    try {
        & $PSCommandPath @args
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location -StackName Quake3Caller
    }
}

# Add default MSYS2 binary path if it exists
if (Test-Path "C:\msys64\usr\bin") {
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$env:PATH"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Retro68 Setup Script (Windows)" -ForegroundColor Cyan
Write-Host "=========================================="

$INSTALL_DIR = Join-Path (Get-Location) "tools\Retro68-build"
$SOURCE_DIR = Join-Path (Get-Location) "tools\Retro68-src"

# The Retro68 commit, its submodules and the SDK archive digests are pinned
# in retro68-versions.txt, which setup_retro68.sh reads too (issue #227).
function Read-Retro68Versions {
    param([string]$Path)

    $Patterns = @{
        RETRO68_URL = "."; RETRO68_COMMIT = "^[0-9a-f]{40}$"
        RETRO68_SUBMODULE = "^\S+ [0-9a-f]{40}$"
        MPW_FILE = "."; MPW_URL = "."; MPW_SHA256 = "^[0-9a-f]{64}$"
        OPENGL_FILE = "."; OPENGL_URL = "."; OPENGL_SHA256 = "^[0-9a-f]{64}$"
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Path is missing; it pins the Retro68 commit and the SDK archives."
    }
    $Pins = @{ RETRO68_SUBMODULE = @() }
    foreach ($Line in @(Get-Content -LiteralPath $Path)) {
        if ($Line -eq "" -or $Line.StartsWith("#")) {
            continue
        }
        $Key, $Value = $Line -split "=", 2
        if ($null -eq $Value -or $Key -cnotmatch "^[A-Z0-9_]+$" -or -not $Patterns.ContainsKey($Key)) {
            throw "${Path}: unknown line: $Line"
        }
        if ($Value -cnotmatch $Patterns[$Key]) {
            throw "${Path}: $Key has an invalid value: $Value"
        }
        if ($Key -ceq "RETRO68_SUBMODULE") {
            $Pins.RETRO68_SUBMODULE += $Value
        }
        else {
            $Pins[$Key] = $Value
        }
    }
    foreach ($Key in $Patterns.Keys) {
        if (-not $Pins[$Key]) {
            throw "${Path}: $Key is missing."
        }
    }
    return $Pins
}
$Pins = Read-Retro68Versions (Join-Path (Get-Location) "retro68-versions.txt")

# Retro68 at the pinned commit, as in setup_retro68.sh. An existing checkout
# is never pulled, switched or updated, its submodules included (issue #406):
# one at another commit, or with a submodule that is missing, at another
# commit or in conflict, stops setup before anything changes. That includes
# the early exit for a working toolchain and the unar build below.
function Invoke-Git {
    param([string[]]$Arguments)

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed (exit status $LASTEXITCODE)."
    }
}

# " <commit> <path>" for every submodule, as `git submodule status` flags it:
# ' ' at the commit Retro68 records, '-' not checked out, '+' at another
# commit, 'U' in conflict.
function Get-CheckedOutSubmodules {
    param([string]$Dir)

    $Status = @(& git -C $Dir submodule status --recursive)
    if ($LASTEXITCODE -ne 0) {
        throw "git -C $Dir submodule status --recursive failed (exit status $LASTEXITCODE)."
    }
    return @($Status | ForEach-Object {
        $_.Substring(0, 41) + " " + ($_.Substring(42) -replace " \(.*\)$", "")
    } | Sort-Object -CaseSensitive)
}
$PinnedSubmodules = @($Pins.RETRO68_SUBMODULE | ForEach-Object {
    $SubmodulePath, $SubmoduleCommit = $_ -split " ", 2
    " $SubmoduleCommit $SubmodulePath"
} | Sort-Object -CaseSensitive)

# True when the checkout at $Dir is at the pinned commit with the pinned
# submodules; otherwise says why.
function Test-PinnedCheckout {
    param([string]$Dir)

    $Commit = "not a git checkout"
    if (Test-Path (Join-Path $Dir ".git")) {
        $Commit = "$(& git -C $Dir rev-parse --verify HEAD)"
        if ($LASTEXITCODE -ne 0) {
            $Commit = "unknown"
        }
    }
    if ($Commit -cne $Pins.RETRO68_COMMIT) {
        Write-Host "Error: $Dir is at $Commit, but" -ForegroundColor Red
        Write-Host "retro68-versions.txt pins Retro68 $($Pins.RETRO68_COMMIT)."
        return $false
    }
    $CheckedOutSubmodules = Get-CheckedOutSubmodules $Dir
    if (($CheckedOutSubmodules -join "`n") -cne ($PinnedSubmodules -join "`n")) {
        Write-Host "Error: the submodules of $Dir are not the ones retro68-versions.txt pins." -ForegroundColor Red
        Write-Host "  Checked out (git submodule status --recursive):"
        $CheckedOutSubmodules | ForEach-Object { Write-Host "    $_" }
        Write-Host "  Pinned:"
        $PinnedSubmodules | ForEach-Object { Write-Host "    $_" }
        return $false
    }
    return $true
}

if (Test-Path $SOURCE_DIR) {
    Write-Host "Retro68 source already present at $SOURCE_DIR (not pulling)."
    if (-not (Get-Command "git" -ErrorAction SilentlyContinue)) {
        Write-Host "Error: 'git' is required to check $SOURCE_DIR against retro68-versions.txt." -ForegroundColor Red
        exit 1
    }
    if (-not (Test-PinnedCheckout $SOURCE_DIR)) {
        Write-Host "Nothing was changed: setup never pulls, switches or updates an existing"
        Write-Host "checkout or its submodules. To build the pinned toolchain, move tools\Retro68-src,"
        Write-Host "tools\Retro68-build and tools\Retro68-work aside and run setup_retro68.ps1 again."
        exit 1
    }
}

# A compiler alone is not a complete install for this project. The renderer
# also requires prepared OpenGL headers and the generated import library.
# The tools must also run: after a host OS upgrade they can remain installed
# but fail to load a DLL (issue #269), and only a full rebuild repairs them.
# check_retro68.ps1 exits 1 only for tools that cannot run. Any other status
# means the check itself could not run (for example no usable TEMP
# directory), which says nothing about the toolchain: stop and change nothing.
$PreparedOpenGLDir = Join-Path $INSTALL_DIR "powerpc-apple-macos\include"
$PreparedGl = Join-Path $PreparedOpenGLDir "gl.h"
$PreparedAgl = Join-Path $PreparedOpenGLDir "agl.h"
$OpenGLStubLib = Join-Path $SOURCE_DIR "InterfacesAndLibraries\SharedLibraries\libOpenGLLibraryStub.a"
$MoveBrokenToolchain = $false
if (((Test-Path "$INSTALL_DIR\bin\powerpc-apple-macos-gcc.exe") -or
     (Test-Path "$INSTALL_DIR\bin\powerpc-apple-macos-gcc")) -and
    (Test-Path $PreparedGl) -and
    (Test-Path $PreparedAgl) -and
    (Test-Path $OpenGLStubLib)) {
    $CheckScript = Join-Path (Get-Location) "check_retro68.ps1"
    $CheckStatus = 3
    if (Test-Path $CheckScript -PathType Leaf) {
        try {
            & $CheckScript -InstallDir $INSTALL_DIR
            $CheckStatus = $LASTEXITCODE
        }
        catch {
            Write-Host "check_retro68.ps1 failed: $_"
        }
    }
    else {
        Write-Host "check_retro68.ps1 was not found at $CheckScript."
    }
    if ($CheckStatus -eq 0) {
        Write-Host "Retro68 appears to be installed in $INSTALL_DIR."
        exit 0
    }
    if ($CheckStatus -ne 1) {
        Write-Host "Error: could not check the installed Retro68 toolchain (exit status $CheckStatus)." -ForegroundColor Red
        Write-Host "Nothing was changed; fix the problem above and run setup_retro68.ps1 again."
        exit 1
    }
    Write-Host "The installed Retro68 toolchain cannot run (see above); rebuilding it." -ForegroundColor Yellow
    $MoveBrokenToolchain = $true
}

Write-Host "Retro68 not found locally."
Write-Host "NOTE: This script will download and BUILD Retro68 from source."
Write-Host "This process can take 20-60 minutes."
Write-Host "Retro68 works best on Windows via Cygwin or MSYS2."
Start-Sleep -Seconds 3

# Dependency Checking. Retro68's build-toolchain.bash runs make-multiverse.rb,
# so ruby is needed too, as setup_retro68.sh checks (issue #231).
$Dependencies = @("cmake", "git", "bison", "flex", "makeinfo", "bash", "ruby")
$MissingDeps = $false

foreach ($dep in $Dependencies) {
    if (-not (Get-Command $dep -ErrorAction SilentlyContinue)) {
        Write-Host "Error: Required command '$dep' not found in PATH." -ForegroundColor Red
        $MissingDeps = $true
    }
}

if ($MissingDeps) {
    Write-Host "--------------------------------------------------------" -ForegroundColor Yellow
    Write-Host "Please install missing dependencies manually." -ForegroundColor Yellow
    Write-Host "Recommended installation via Cygwin or MSYS2 (pacman)."
    Write-Host "Packages: cmake git bison flex texinfo ruby gcc g++ make boost libmpc-devel mpfr-devel gmp-devel"
    Write-Host "--------------------------------------------------------"
    exit 1
}


# A fresh clone is made next to tools\Retro68-src and renamed to it only once
# it is at the pinned commit and submodules, so a clone or checkout that fails
# leaves nothing behind that would stop the next run (issue #406).
if (-not (Test-Path $SOURCE_DIR)) {
    Write-Host "Checking out Retro68 $($Pins.RETRO68_COMMIT)..." -ForegroundColor Green
    New-Item -ItemType Directory -Force -Path "tools" | Out-Null
    $PartialSourceDir = "$SOURCE_DIR.partial"
    if (Test-Path $PartialSourceDir) {
        Remove-Item -Recurse -Force $PartialSourceDir
    }
    $Cloned = $false
    try {
        Invoke-Git @("clone", "--no-checkout", $Pins.RETRO68_URL, $PartialSourceDir)
        Invoke-Git @("-C", $PartialSourceDir, "-c", "advice.detachedHead=false",
            "checkout", "--detach", $Pins.RETRO68_COMMIT)
        Invoke-Git @("-C", $PartialSourceDir, "submodule", "update", "--init", "--recursive")
        $Cloned = Test-PinnedCheckout $PartialSourceDir
    }
    catch {
        Write-Host "$_" -ForegroundColor Red
    }
    if (-not $Cloned) {
        if (Test-Path $PartialSourceDir) {
            Remove-Item -Recurse -Force $PartialSourceDir
        }
        Write-Host "Error: could not check out Retro68 $($Pins.RETRO68_COMMIT) and its pinned" -ForegroundColor Red
        Write-Host "submodules (see above). The partial clone was removed; nothing else changed."
        exit 1
    }
    Rename-Item -LiteralPath $PartialSourceDir -NewName (Split-Path $SOURCE_DIR -Leaf)
}

# Check for unar, build if missing. A unar built here earlier is in tools\unar-bin.
$BuiltUnarDir = Join-Path (Get-Location) "tools\unar-bin"
if (Test-Path $BuiltUnarDir) { $env:PATH = "$BuiltUnarDir$([System.IO.Path]::PathSeparator)$env:PATH" }
if (-not (Get-Command "unar" -ErrorAction SilentlyContinue)) {
    Write-Host "unar not found. Preparing to build XADMaster (unar/lsar)..." -ForegroundColor Yellow
    
    # Ensure git/bash are present for this
    if (-not (Get-Command "git" -ErrorAction SilentlyContinue) -or -not (Get-Command "bash" -ErrorAction SilentlyContinue)) {
        Write-Host "Error: 'git' and 'bash' are required to build unar." -ForegroundColor Red
        Write-Host "Please install them via MSYS2/Cygwin."
        exit 1
    }

    $ToolsDir = "tools"
    if (-not (Test-Path $ToolsDir)) { New-Item -ItemType Directory -Path $ToolsDir | Out-Null }
    
    # 1. Clone repositories, pinned like Retro68 (issue #406) so every setup
    # builds the same unar.
    $XAD_URL = "https://github.com/MacPaw/XADMaster.git"
    $XAD_COMMIT = "7cb9ee0abbb163f261e4cb74501e15067032319c"
    $UD_URL = "https://github.com/MacPaw/universal-detector.git"
    $UD_COMMIT = "4eb832d999628edcd3d134e46bd35357c8c99a85"
    
    $XAD_DIR = Join-Path (Get-Location) "$ToolsDir\XADMaster"
    $UD_DIR = Join-Path (Get-Location) "$ToolsDir\UniversalDetector"
    
    foreach ($Repository in @(@($XAD_URL, $XAD_DIR, $XAD_COMMIT), @($UD_URL, $UD_DIR, $UD_COMMIT))) {
        $RepositoryUrl, $RepositoryDir, $RepositoryCommit = $Repository
        if (-not (Test-Path $RepositoryDir)) {
            Write-Host "Cloning $RepositoryUrl at $RepositoryCommit..."
            Invoke-Git @("clone", "--no-checkout", $RepositoryUrl, $RepositoryDir)
            Invoke-Git @("-C", $RepositoryDir, "-c", "advice.detachedHead=false",
                "checkout", "--detach", $RepositoryCommit)
        }
        $RepositoryHead = "$(& git -C $RepositoryDir rev-parse --verify HEAD)"
        if ($LASTEXITCODE -ne 0 -or $RepositoryHead -cne $RepositoryCommit) {
            Write-Host "Error: $RepositoryDir is at $RepositoryHead, not the pinned $RepositoryCommit." -ForegroundColor Red
            Write-Host "Move it aside and run setup_retro68.ps1 again."
            exit 1
        }
    }
    
    # 1.5 Patch Makefiles for Clang/MSYS2
    $MakefileWin = Join-Path $XAD_DIR "Makefile.windows"
    $UDMakefileWin = Join-Path $UD_DIR "Makefile.windows"
    
    # Capture gnustep-config flags via bash
    Write-Host "Capturing gnustep-config flags..."
    # Use absolute path for gnustep-config inside bash to avoid PATH issues
    # We strip any newline characters from the output
    $ObjCFlags = bash -c "/mingw64/bin/gnustep-config --objc-flags" 2>&1 | Out-String
    $BaseLibs = bash -c "/mingw64/bin/gnustep-config --base-libs" 2>&1 | Out-String
    
    $ObjCFlags = $ObjCFlags.Trim()
    $BaseLibs = $BaseLibs.Trim()

    if ($LASTEXITCODE -ne 0 -or $ObjCFlags -match "command not found" -or [string]::IsNullOrWhiteSpace($ObjCFlags)) {
        Write-Host "Error: Could not capture gnustep-config output." -ForegroundColor Red
        Write-Host "Output: $ObjCFlags"
        Write-Host "Ensure mingw-w64-x86_64-gnustep-base is installed and /mingw64/bin/gnustep-config exists."
        exit 1
    }
    
    if (Test-Path $MakefileWin) {
        Write-Host "Patching XADMaster/Makefile.windows for Clang..."
        $mkContent = Get-Content $MakefileWin -Raw
        
        # 1. Switch compilers to clang
        $mkContent = $mkContent -replace "OBJCC = gcc", "OBJCC = clang"
        $mkContent = $mkContent -replace "CC = gcc", "CC = clang"
        $mkContent = $mkContent -replace "CXX = g\+\+", "CXX = clang++"
        $mkContent = $mkContent -replace "LD = gcc", "LD = clang"
        
        # 2. Update GNUSTEP_OPTS
        # Case A: Original multi-line (ends with NSConstantString)
        if ($mkContent -match "(?s)GNUSTEP_OPTS\s*=.*?NSConstantString") {
            $mkContent = $mkContent -replace "(?s)GNUSTEP_OPTS\s*=.*?NSConstantString", "GNUSTEP_OPTS = $ObjCFlags"
        } 
        # Case B: Already patched single-line (contains gnustep-config or just starts with GNUSTEP_OPTS =)
        else {
            $mkContent = $mkContent -replace "(?m)^GNUSTEP_OPTS\s*=.*$", "GNUSTEP_OPTS = $ObjCFlags"
        }
        
        # 3. Update LIBS
        # Case A: Original multi-line (ends with -lgdi32)
        if ($mkContent -match "(?s)LIBS\s*=.*?-lgdi32") {
            $mkContent = $mkContent -replace "(?s)LIBS\s*=.*?-lgdi32", "LIBS = $BaseLibs -lz -lbz2 -lstdc++ -lm -lwinmm -lgdi32"
        }
        # Case B: Already patched single-line
        else {
            $mkContent = $mkContent -replace "(?m)^LIBS\s*=.*$", "LIBS = $BaseLibs -lz -lbz2 -lstdc++ -lm -lwinmm -lgdi32"
        }
        
        # 4. Remove old hardcoded paths (if they persist)
        $mkContent = $mkContent -replace "-isystem C:\\GNUstep\\GNUstep\\System\\Library\\Headers", ""
        $mkContent = $mkContent -replace "-LC:\\GNUstep\\GNUstep\\System\\Library\\Libraries", ""
        
        # 5. Fix clean target to use Makefile.windows for dependency
        $mkContent = $mkContent -replace "make -C \.\./UniversalDetector -f Makefile\.linux clean", "make -C ../UniversalDetector -f Makefile.windows clean"

        Set-Content -Path $MakefileWin -Value $mkContent
    }

    if (Test-Path $UDMakefileWin) {
        Write-Host "Patching UniversalDetector/Makefile.windows for Clang..."
        $udContent = Get-Content $UDMakefileWin -Raw
        
        # 1. Switch compilers
        $udContent = $udContent -replace "OBJCC = gcc", "OBJCC = clang"
        $udContent = $udContent -replace "CC = gcc", "CC = clang"
        
        # 2. Update GNUSTEP_OPTS
        # Case A: Original multi-line
        if ($udContent -match "(?s)GNUSTEP_OPTS\s*=.*?NSConstantString") {
            $udContent = $udContent -replace "(?s)GNUSTEP_OPTS\s*=.*?NSConstantString", "GNUSTEP_OPTS = $ObjCFlags"
        }
        # Case B: Already patched
        else {
            $udContent = $udContent -replace "(?m)^GNUSTEP_OPTS\s*=.*$", "GNUSTEP_OPTS = $ObjCFlags"
        }
        
        Set-Content -Path $UDMakefileWin -Value $udContent
    }

    # 2. Build using bash/make (Makefile.windows)
    Write-Host "Building XADMaster via MSYS2/Bash..."
    
    # Convert paths to Unix style
    $XAD_DIR_UNIX = $XAD_DIR -replace '\\', '/'
    
    # Run make using Makefile.windows with explicit PATH export
    bash -c "export PATH=/mingw64/bin:/usr/bin:`$PATH; cd '$XAD_DIR_UNIX' && make -f Makefile.windows clean && make -f Makefile.windows unar lsar"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: building XADMaster failed (exit status $LASTEXITCODE)." -ForegroundColor Red
        Write-Host "Ensure MSYS2 packages: clang, gnustep-base, and gnustep-make are installed."
        exit 1
    }
    
    # 3. Copy binaries
    $UnarExeExe = Join-Path $XAD_DIR "unar.exe"
    $LsarExeExe = Join-Path $XAD_DIR "lsar.exe"
    
    # Check for .exe or no extension (in case)
    if (-not (Test-Path $UnarExeExe)) { $UnarExeExe = Join-Path $XAD_DIR "unar" }
    
    if (Test-Path $UnarExeExe) {
        Write-Host "Build successful. Installing unar/lsar..." -ForegroundColor Green
        # Not into the toolchain prefix: build-toolchain.bash refuses to
        # build into a prefix that is not empty (issue #387).
        $UnarBinDir = Join-Path (Get-Location) "$ToolsDir\unar-bin"
        if (-not (Test-Path $UnarBinDir)) { New-Item -ItemType Directory -Path $UnarBinDir -Force | Out-Null }
        
        Copy-Item $UnarExeExe $UnarBinDir -Force
        if (Test-Path $LsarExeExe) { Copy-Item $LsarExeExe $UnarBinDir -Force }
        
        # Add to current PATH
        $env:PATH = "$UnarBinDir$([System.IO.Path]::PathSeparator)$env:PATH"
    }
    else {
        Write-Host "Error: the XADMaster build wrote no unar." -ForegroundColor Red
        Write-Host "Ensure MSYS2 packages: clang, gnustep-base, and gnustep-make are installed."
        exit 1
    }
}

# 2. Prepare InterfacesAndLibraries
Write-Host "Step 2: Preparing SDKs..." -ForegroundColor Green
$SDK_DEST = Join-Path $SOURCE_DIR "InterfacesAndLibraries"

# True when the file's SHA-256 is the pinned one; otherwise says why.
function Test-SitArchive {
    param([string]$Path, [string]$Sha256)

    try {
        $Found = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    }
    catch {
        $Found = "(could not read it)"
    }
    if ($Found -ceq $Sha256) {
        return $true
    }
    Write-Host "  $Path does not match the SHA-256 pinned in retro68-versions.txt"
    Write-Host "  (truncated, damaged or another file); it is not used."
    Write-Host "    pinned: $Sha256"
    Write-Host "    found:  $Found, $((Get-Item -LiteralPath $Path).Length) bytes"
    return $false
}

# Resolve an SDK archive whose SHA-256 matches the pin, as setup_retro68.sh
# does: tools\<name>, then <name> at the repository root, else a download to
# tools\<name>.part that becomes tools\<name> only once it matches. Nothing is
# extracted from a copy that does not match, and no local copy is deleted or
# overwritten.
function Resolve-SitArchive {
    param([string]$Name, [string]$Url, [string]$Sha256)

    $Cached = Join-Path "tools" $Name
    foreach ($Candidate in @($Cached, $Name)) {
        if ((Test-Path -LiteralPath $Candidate -PathType Leaf) -and (Test-SitArchive $Candidate $Sha256)) {
            Write-Host "  Using $Candidate (SHA-256 matches retro68-versions.txt)"
            return $Candidate
        }
    }
    if (Test-Path -LiteralPath $Cached) {
        Write-Host "Error: no copy of $Name matches its pinned SHA-256." -ForegroundColor Red
        Write-Host "Replace $Cached with a good copy, or move it away so setup can download one."
        exit 1
    }
    $Partial = "$Cached.part"
    Remove-Item -LiteralPath $Partial -Force -ErrorAction SilentlyContinue
    Write-Host "Downloading $Name..."
    Invoke-WebRequest -Uri $Url -OutFile $Partial
    if (-not (Test-SitArchive $Partial $Sha256)) {
        Remove-Item -LiteralPath $Partial -Force
        Write-Host "Error: the download of $Name from $Url was discarded." -ForegroundColor Red
        Write-Host "Place a copy that matches at the repository root or in tools and run setup again."
        exit 1
    }
    Move-Item -LiteralPath $Partial -Destination $Cached
    return $Cached
}

# Check both archives before extracting either.
$MpwSit = Resolve-SitArchive $Pins.MPW_FILE $Pins.MPW_URL $Pins.MPW_SHA256
$OpenGLSit = Resolve-SitArchive $Pins.OPENGL_FILE $Pins.OPENGL_URL $Pins.OPENGL_SHA256
if (-not (Test-Path $SDK_DEST)) {
    New-Item -ItemType Directory -Path $SDK_DEST | Out-Null
}

# Extract function using unar (assuming available as checked). The archive is
# hashed again right before it is extracted, since it was checked before the
# other one (issue #406).
function Extract-Site {
    param($File, $Sha256, $DestDir)
    if (-not (Test-SitArchive $File $Sha256)) {
        Write-Host "Error: $File changed after it was checked; it was not extracted." -ForegroundColor Red
        exit 1
    }
    if (Test-Path $DestDir) { Remove-Item -Recurse -Force $DestDir }
    Write-Host "Extracting $File..."
    # unar usage: unar -f file.sit -o output_dir
    # we need to be careful with paths in powershell calling binaries
    & unar -f $File -o $DestDir | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: unar could not extract $File (exit status $LASTEXITCODE)." -ForegroundColor Red
        exit 1
    }
}

# Process MPW
Extract-Site -File $MpwSit -Sha256 $Pins.MPW_SHA256 -DestDir "tools\temp_mpw"
$MPW_I_AND_L = Get-ChildItem -Path "tools\temp_mpw" -Recurse -Directory -Filter "Interfaces&Libraries" | Select-Object -First 1
if ($MPW_I_AND_L) {
    Write-Host "Injecting MPW Interfaces&Libraries..."
    Copy-Item -Path "$($MPW_I_AND_L.FullName)\*" -Destination $SDK_DEST -Recurse -Force
}
else {
    Write-Host "Error: Could not find Interfaces&Libraries in MPW" -ForegroundColor Red
    exit 1
}

# Process OpenGL
Extract-Site -File $OpenGLSit -Sha256 $Pins.OPENGL_SHA256 -DestDir "tools\temp_opengl"
$OGL_LIBS = Get-ChildItem -Path "tools\temp_opengl" -Recurse -Directory -Filter "Libraries" | Select-Object -First 1
$OGL_HEADERS = Get-ChildItem -Path "tools\temp_opengl" -Recurse -Directory -Filter "Headers" | Select-Object -First 1

if (-not (Test-Path "$SDK_DEST\Libraries")) { New-Item -ItemType Directory -Path "$SDK_DEST\Libraries" | Out-Null }
if (-not (Test-Path "$SDK_DEST\Interfaces\CIncludes")) { New-Item -ItemType Directory -Path "$SDK_DEST\Interfaces\CIncludes" | Out-Null }

if ($OGL_LIBS) {
    Write-Host "Injecting OpenGL Libraries..."
    # Copy to SharedLibraries so MakeImport handles them (they are PEF)
    if (-not (Test-Path "$SDK_DEST\SharedLibraries")) { New-Item -ItemType Directory -Path "$SDK_DEST\SharedLibraries" | Out-Null }
    Copy-Item -Path "$($OGL_LIBS.FullName)\*" -Destination "$SDK_DEST\SharedLibraries" -Recurse -Force
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
# build-toolchain.bash refuses to install a full build into a non-empty
# prefix. Never delete the old toolchain: move it aside so it can be restored.
# The same goes for an incomplete earlier build (issue #387), and for the work
# tree, whose configure caches may name host libraries that are gone.
$WORK_DIR = Join-Path (Get-Location) "tools\Retro68-work"
$Stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$AsideSuffix = if ($MoveBrokenToolchain) { "broken" } else { "previous" }
foreach ($Previous in @($INSTALL_DIR, $WORK_DIR)) {
    if (-not (Test-Path $Previous)) {
        continue
    }
    $AsideName = "$(Split-Path $Previous -Leaf).$AsideSuffix-$Stamp"
    $AsidePath = Join-Path (Split-Path $Previous -Parent) $AsideName
    Rename-Item -Path $Previous -NewName $AsideName
    if ($MoveBrokenToolchain -and $Previous -eq $INSTALL_DIR) {
        Write-Host "Moved the toolchain that cannot run to $AsidePath."
    }
    else {
        Write-Host "Moved $Previous to $AsidePath."
    }
    Write-Host "To restore it, delete $Previous and rename $AsideName back to $(Split-Path $Previous -Leaf)."
    Write-Host "Delete it once the new toolchain works."
}
New-Item -ItemType Directory -Path $WORK_DIR | Out-Null

# A host C++ compiler that defaults to C++20 or later (GCC 16) cannot build
# GCC 12.2; pin C++11 in CXXFLAGS as setup_retro68.sh does (issue #387).
$HostCxxDialect = "$(bash -c "printf '__cplusplus\n' | g++ -x c++ -E -P - 2> /dev/null | tr -d ' L\r' | tail -n 1")"
if ($HostCxxDialect -match "^[0-9]+$" -and [long]$HostCxxDialect -gt 201703) {
    if (" $env:CXXFLAGS " -match " -std=") {
        Write-Host "The host C++ compiler defaults to C++ $HostCxxDialect; keeping CXXFLAGS=$env:CXXFLAGS."
    }
    else {
        $env:CXXFLAGS = "$(if ($env:CXXFLAGS) { $env:CXXFLAGS } else { "-O2" }) -std=gnu++11"
        Write-Host "The host C++ compiler defaults to C++ $HostCxxDialect, which GCC 12.2 does not"
        Write-Host "build under; building with CXXFLAGS=$env:CXXFLAGS."
    }
}
Write-Host "Invoking build-toolchain.bash via bash..."

# Convert paths to Unix style for bash
$InstallDirUnix = $INSTALL_DIR -replace '\\', '/'
$SourceDirUnix = $SOURCE_DIR -replace '\\', '/'
$WorkDirUnix = $WORK_DIR -replace '\\', '/'

# build-toolchain.bash refuses to run from its source directory, so run it
# from the work tree, as setup_retro68.sh does (issue #387).
bash -c "cd '$WorkDirUnix' && bash '$SourceDirUnix/build-toolchain.bash' --prefix='$InstallDirUnix' --clean-after-build"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Installing OpenGL support into the prepared toolchain..." -ForegroundColor Green
    $RawOpenGLDir = Join-Path $SOURCE_DIR "InterfacesAndLibraries\Interfaces\CIncludes"
    New-Item -ItemType Directory -Path $PreparedOpenGLDir -Force | Out-Null
    $OpenGLHeaders = @(
        "gl.h", "glu.h", "glm.h", "agl.h", "aglContext.h",
        "aglMacro.h", "aglRenderers.h", "glext.h", "GL_gl.h",
        "GL_glext.h", "GL_glut.h", "gliContext.h", "gliDispatch.h",
        "glut.h"
    )
    foreach ($Header in $OpenGLHeaders) {
        $SourceHeader = Join-Path $RawOpenGLDir $Header
        if (Test-Path $SourceHeader) {
            Copy-Item $SourceHeader $PreparedOpenGLDir -Force
        }
    }

    $OpenGLSharedDir = Join-Path $SOURCE_DIR "InterfacesAndLibraries\SharedLibraries"
    $StubSource = Join-Path $OpenGLSharedDir "OpenGLLibraryStub"
    $StubResource = Join-Path $OpenGLSharedDir "OpenGLLibraryStub.rsrc"
    $StubAppleDouble = Join-Path $OpenGLSharedDir "%OpenGLLibraryStub"
    $MakeImport = Join-Path $INSTALL_DIR "bin\MakeImport.exe"
    if (-not (Test-Path $MakeImport)) {
        $MakeImport = Join-Path $INSTALL_DIR "bin\MakeImport"
    }
    if ((Test-Path $StubSource) -and
        (Test-Path $StubResource) -and
        (Test-Path $MakeImport)) {
        Copy-Item $StubResource $StubAppleDouble -Force
        # MakeImport runs powerpc-apple-macos-as from PATH (issue #387).
        $env:PATH = "$(Join-Path $INSTALL_DIR "bin")$([System.IO.Path]::PathSeparator)$env:PATH"
        & $MakeImport $StubSource $OpenGLStubLib
        if ($LASTEXITCODE -ne 0) {
            throw "MakeImport could not generate $OpenGLStubLib (exit status $LASTEXITCODE)."
        }
    }

    if (-not (Test-Path $PreparedGl) -or
        -not (Test-Path $PreparedAgl) -or
        -not (Test-Path $OpenGLStubLib)) {
        throw "Retro68 built, but prepared OpenGL headers/import library are incomplete."
    }

    Write-Host "Retro68 installed to $INSTALL_DIR" -ForegroundColor Green
}
else {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}
exit 0
