# Retro68 Setup Script (PowerShell)
# This script sets up the Retro68 toolchain on Windows.
# Note: Retro68 requires a Unix-like environment (Cygwin or MSYS2) to build.
# This script mainly checks dependencies and invokes the build system.

$ErrorActionPreference = "Stop"

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

# Check for unar, build if missing
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
    
    # 1. Clone repositories
    $XAD_URL = "https://github.com/MacPaw/XADMaster.git"
    $UD_URL = "https://github.com/MacPaw/universal-detector.git"
    
    $XAD_DIR = Join-Path (Get-Location) "$ToolsDir\XADMaster"
    $UD_DIR = Join-Path (Get-Location) "$ToolsDir\UniversalDetector"
    
    if (-not (Test-Path $XAD_DIR)) {
        Write-Host "Cloning XADMaster..."
        git clone "$XAD_URL" "$XAD_DIR"
    }
    
    if (-not (Test-Path $UD_DIR)) {
        Write-Host "Cloning universal-detector..."
        git clone "$UD_URL" "$UD_DIR"
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
    
    # 3. Copy binaries
    $UnarExeExe = Join-Path $XAD_DIR "unar.exe"
    $LsarExeExe = Join-Path $XAD_DIR "lsar.exe"
    
    # Check for .exe or no extension (in case)
    if (-not (Test-Path $UnarExeExe)) { $UnarExeExe = Join-Path $XAD_DIR "unar" }
    
    if (Test-Path $UnarExeExe) {
        Write-Host "Build successful. Installing unar/lsar..." -ForegroundColor Green
        if (-not (Test-Path "$INSTALL_DIR\bin")) { New-Item -ItemType Directory -Path "$INSTALL_DIR\bin" -Force | Out-Null }
        
        Copy-Item $UnarExeExe "$INSTALL_DIR\bin" -Force
        if (Test-Path $LsarExeExe) { Copy-Item $LsarExeExe "$INSTALL_DIR\bin" -Force }
        
        # Add to current PATH
        $env:PATH = "$INSTALL_DIR\bin;$env:PATH"
    }
    else {
        Write-Host "Warning: XADMaster build failed." -ForegroundColor Red
        Write-Host "Ensure MSYS2 packages: clang, gnustep-base, and gnustep-make are installed."
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
if ((Test-Path "$INSTALL_DIR\bin\powerpc-apple-macos-gcc.exe") -and
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

# Dependency Checking
# Dependency Checking
$Dependencies = @("cmake", "git", "bison", "flex", "makeinfo", "bash")
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
    Write-Host "Packages: cmake git bison flex texinfo gcc g++ make boost libmpc-devel mpfr-devel gmp-devel"
    Write-Host "--------------------------------------------------------"
    exit 1
}


# Retro68 at the pinned commit, as in setup_retro68.sh. An existing checkout
# is never pulled or switched: one at another commit stops setup before
# anything changes.
Write-Host "Checking out Retro68 $($Pins.RETRO68_COMMIT)..." -ForegroundColor Green
New-Item -ItemType Directory -Force -Path "tools" | Out-Null

function Invoke-Git {
    param([string[]]$Arguments)

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed (exit status $LASTEXITCODE)."
    }
}

if (-not (Test-Path $SOURCE_DIR)) {
    Invoke-Git @("clone", "--no-checkout", $Pins.RETRO68_URL, $SOURCE_DIR)
    Invoke-Git @("-C", $SOURCE_DIR, "-c", "advice.detachedHead=false",
        "checkout", "--detach", $Pins.RETRO68_COMMIT)
}
else {
    Write-Host "Retro68 source already present at $SOURCE_DIR (not pulling)."
}
$SourceCommit = "not a git checkout"
if (Test-Path (Join-Path $SOURCE_DIR ".git")) {
    $SourceCommit = "$(& git -C $SOURCE_DIR rev-parse --verify HEAD)"
    if ($LASTEXITCODE -ne 0) {
        $SourceCommit = "unknown"
    }
}
if ($SourceCommit -cne $Pins.RETRO68_COMMIT) {
    Write-Host "Error: $SOURCE_DIR is at $SourceCommit, but" -ForegroundColor Red
    Write-Host "retro68-versions.txt pins Retro68 $($Pins.RETRO68_COMMIT). Nothing was changed."
    Write-Host "To build the pinned toolchain, move tools\Retro68-src and tools\Retro68-build"
    Write-Host "aside and run setup_retro68.ps1 again."
    exit 1
}
Invoke-Git @("-C", $SOURCE_DIR, "submodule", "update", "--init", "--recursive")
# " <commit> <path>" for every submodule, as `git submodule status` flags it.
$CheckedOutSubmodules = @(& git -C $SOURCE_DIR submodule status --recursive | ForEach-Object {
    $_.Substring(0, 41) + " " + ($_.Substring(42) -replace " \(.*\)$", "")
} | Sort-Object -CaseSensitive)
$PinnedSubmodules = @($Pins.RETRO68_SUBMODULE | ForEach-Object {
    $SubmodulePath, $SubmoduleCommit = $_ -split " ", 2
    " $SubmoduleCommit $SubmodulePath"
} | Sort-Object -CaseSensitive)
if (($CheckedOutSubmodules -join "`n") -cne ($PinnedSubmodules -join "`n")) {
    Write-Host "Error: the submodules of $SOURCE_DIR are not the ones retro68-versions.txt pins." -ForegroundColor Red
    Write-Host "  Checked out (git submodule status --recursive):"
    $CheckedOutSubmodules | ForEach-Object { Write-Host "    $_" }
    Write-Host "  Pinned:"
    $PinnedSubmodules | ForEach-Object { Write-Host "    $_" }
    exit 1
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
Extract-Site -File $MpwSit -DestDir "tools\temp_mpw"
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
Extract-Site -File $OpenGLSit -DestDir "tools\temp_opengl"
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
if ($MoveBrokenToolchain) {
    $Stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
    $AsideName = "Retro68-build.broken-$Stamp"
    $AsidePath = Join-Path (Split-Path $INSTALL_DIR -Parent) $AsideName
    Rename-Item -Path $INSTALL_DIR -NewName $AsideName
    Write-Host "Moved the toolchain that cannot run to $AsidePath."
    Write-Host "To restore it, delete $INSTALL_DIR and rename $AsideName back to Retro68-build."
    Write-Host "Delete it once the new toolchain works."
}
Write-Host "Invoking build-toolchain.bash via bash..."

# Convert paths to Unix style for bash
$InstallDirUnix = $INSTALL_DIR -replace '\\', '/'
$SourceDirUnix = $SOURCE_DIR -replace '\\', '/'

# We assume 'bash' is available (checked in dependencies)
# We need to run the bash script. 
# Note: Windows path handling in bash can be tricky.
bash -c "cd '$SourceDirUnix' && ./build-toolchain.bash --prefix='$InstallDirUnix' --clean-after-build"

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
        & $MakeImport $StubSource $OpenGLStubLib
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
