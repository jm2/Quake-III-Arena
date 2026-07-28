# Quake III Arena Build Script for Windows (Retro68)
# Builds the monolithic Classic Mac OS app using the Retro68 toolchain.

$ErrorActionPreference = "Stop"

function Show-Usage {
    Write-Host "Usage: .\build_mac.ps1 [package|--package] [--base-only|--team-arena] [-h|--help]"
    Write-Host ""
    Write-Host "  (no args)            Configure, build, and PEF-validate Quake3 only."
    Write-Host "  package, --package   Build, then assemble a Mac OS 9 install image."
    Write-Host "  --base-only          Build only Quake3 (the default)."
    Write-Host "  --team-arena         Build Quake3 and Quake3_TeamArena."
}

$PackageMode = $false
$TeamArenaMode = "OFF"
$PackageModeSet = $false
$TeamArenaModeSet = $false
foreach ($Argument in $args) {
    switch ($Argument) {
        { $_ -in @("package", "--package") } {
            if ($PackageModeSet) {
                throw "Package mode was specified more than once."
            }
            $PackageMode = $true
            $PackageModeSet = $true
            break
        }
        "--base-only" {
            if ($TeamArenaModeSet) {
                throw "Choose exactly one of --base-only or --team-arena."
            }
            $TeamArenaMode = "OFF"
            $TeamArenaModeSet = $true
            break
        }
        "--team-arena" {
            if ($TeamArenaModeSet) {
                throw "Choose exactly one of --base-only or --team-arena."
            }
            $TeamArenaMode = "ON"
            $TeamArenaModeSet = $true
            break
        }
        { $_ -in @("-h", "--help") } {
            Show-Usage
            exit 0
        }
        default {
            Show-Usage
            throw "Unknown argument '$Argument'."
        }
    }
}

# Add default MSYS2 binary path if it exists
if (Test-Path "C:\msys64\usr\bin") {
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$env:PATH"
}

$BuildDir = "build_mac"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$ToolsDir = Join-Path (Get-Location) "tools\Retro68-build\bin"
$CompilerName = "powerpc-apple-macos-gcc.exe"
$CompilerPath = Join-Path $ToolsDir $CompilerName
$PreparedOpenGLDir = Join-Path (Get-Location) "tools\Retro68-build\powerpc-apple-macos\include"
$RawOpenGLDir = Join-Path (Get-Location) "tools\Retro68-src\InterfacesAndLibraries\Interfaces\CIncludes"
$OpenGLSharedDir = Join-Path (Get-Location) "tools\Retro68-src\InterfacesAndLibraries\SharedLibraries"
$OpenGLStubLib = Join-Path $OpenGLSharedDir "libOpenGLLibraryStub.a"

function Install-PreparedOpenGLSupport {
    if (-not (Test-Path $CompilerPath)) {
        return $false
    }

    $PreparedGl = Join-Path $PreparedOpenGLDir "gl.h"
    $PreparedAgl = Join-Path $PreparedOpenGLDir "agl.h"
    if (-not (Test-Path $PreparedGl) -or -not (Test-Path $PreparedAgl)) {
        $RawGl = Join-Path $RawOpenGLDir "gl.h"
        $RawAgl = Join-Path $RawOpenGLDir "agl.h"
        if (-not (Test-Path $RawGl) -or -not (Test-Path $RawAgl)) {
            return $false
        }

        Write-Host "Installing missing OpenGL SDK headers into the prepared toolchain..."
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
    }

    if (-not (Test-Path $OpenGLStubLib)) {
        $StubSource = Join-Path $OpenGLSharedDir "OpenGLLibraryStub"
        $StubResource = Join-Path $OpenGLSharedDir "OpenGLLibraryStub.rsrc"
        $StubAppleDouble = Join-Path $OpenGLSharedDir "%OpenGLLibraryStub"
        $MakeImport = Join-Path $ToolsDir "MakeImport.exe"
        if (-not (Test-Path $MakeImport)) {
            $MakeImport = Join-Path $ToolsDir "MakeImport"
        }
        if (-not (Test-Path $MakeImport) -or
            -not (Test-Path $StubSource) -or
            -not (Test-Path $StubResource)) {
            return $false
        }

        Write-Host "Generating missing OpenGL import library..."
        Copy-Item $StubResource $StubAppleDouble -Force
        & $MakeImport $StubSource $OpenGLStubLib
        if ($LASTEXITCODE -ne 0) {
            return $false
        }
    }

    return (Test-Path $PreparedGl) -and
        (Test-Path $PreparedAgl) -and
        (Test-Path $OpenGLStubLib)
}

if (-not (Install-PreparedOpenGLSupport)) {
    Write-Host "Retro68 compiler or prepared OpenGL SDK support not found." -ForegroundColor Yellow
    Write-Host "Running setup_retro68.ps1..."
    .\setup_retro68.ps1
    if (-not (Install-PreparedOpenGLSupport)) {
        throw "Retro68 setup did not install the compiler, prepared gl.h/agl.h headers, and OpenGL import library."
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
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Retro68.toolchain.cmake `
    -DCMAKE_BUILD_TYPE=Release `
    -DBUILD_TEAM_ARENA=$TeamArenaMode

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed." -ForegroundColor Red
    Pop-Location
    exit 1
}

# Gate post-build and package steps on the option that produced this build,
# never on a possibly stale executable left in build_mac.
$TeamArenaCacheLine = Get-Content "CMakeCache.txt" |
    Where-Object { $_ -match "^BUILD_TEAM_ARENA:BOOL=" } |
    Select-Object -Last 1
$BuildTeamArena = $TeamArenaCacheLine -match "=(1|ON|TRUE|YES|Y)$"
if ($BuildTeamArena -ne ($TeamArenaMode -eq "ON")) {
    throw "CMake did not preserve the requested BUILD_TEAM_ARENA=$TeamArenaMode setting."
}

# Build
Write-Host "Building..." -ForegroundColor Green
# Use cmake --build to be generator-agnostic (handles make, ninja, visual studio, etc)
# -j for parallel build
cmake --build . --parallel

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build complete." -ForegroundColor Green
    
    # Convert the linked XCOFF image to a validated PowerPC PEF.
    $MakePEF = Join-Path $ToolsDir "MakePEF.exe"
    if (-not (Test-Path $MakePEF)) {
        $MakePEF = Join-Path $ToolsDir "MakePEF"
    }
    if (-not (Test-Path $MakePEF)) {
        throw "MakePEF not found; a Classic Mac application cannot be produced."
    }

    function Test-IsPEF {
        param([string]$Path)

        if (-not (Test-Path $Path)) {
            return $false
        }

        $Stream = $null
        try {
            $Stream = [System.IO.File]::OpenRead((Convert-Path $Path))
            if ($Stream.Length -lt 1MB) {
                return $false
            }
            $Buffer = New-Object byte[] 12
            if ($Stream.Read($Buffer, 0, 12) -ne 12) {
                return $false
            }
            return $Buffer[0] -eq 0x4A -and $Buffer[1] -eq 0x6F -and
                $Buffer[2] -eq 0x79 -and $Buffer[3] -eq 0x21 -and
                $Buffer[4] -eq 0x70 -and $Buffer[5] -eq 0x65 -and
                $Buffer[6] -eq 0x66 -and $Buffer[7] -eq 0x66 -and
                $Buffer[8] -eq 0x70 -and $Buffer[9] -eq 0x77 -and
                $Buffer[10] -eq 0x70 -and $Buffer[11] -eq 0x63
        }
        finally {
            if ($null -ne $Stream) {
                $Stream.Dispose()
            }
        }
    }

    function Convert-ToPEF {
        param(
            [string]$InputPath,
            [string]$TemporaryPath
        )

        if (-not (Test-IsPEF $InputPath)) {
            Write-Host "Converting $InputPath to PEF..."
            & $MakePEF $InputPath -o $TemporaryPath
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path $TemporaryPath)) {
                throw "MakePEF failed for $InputPath."
            }
            Move-Item -Force $TemporaryPath $InputPath
        }
        if (-not (Test-IsPEF $InputPath)) {
            throw "PEF validation failed for $InputPath (need Joy!peff/pwpc and at least 1 MiB)."
        }
        Write-Host "PEF validation OK: $InputPath"
    }

    $Quake3Binary = if (Test-Path "Quake3") {
        "Quake3"
    }
    elseif (Test-Path "Quake3.exe") {
        "Quake3.exe"
    }
    else {
        throw "Base-game executable is missing."
    }
    Convert-ToPEF $Quake3Binary "Quake3.pef"

    if ($BuildTeamArena) {
        $TeamArenaBinary = if (Test-Path "Quake3_TeamArena") {
            "Quake3_TeamArena"
        }
        elseif (Test-Path "Quake3_TeamArena.exe") {
            "Quake3_TeamArena.exe"
        }
        else {
            throw "Team Arena is enabled but its executable is missing."
        }
        Convert-ToPEF $TeamArenaBinary "Quake3_TeamArena.pef"
    }
}
else {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}

Pop-Location

# Packaging Subcommand (Simple Switch)
# PowerShell arguments aren't passed as $1 automatically if strictly defined params, 
# but if script has no param block, $args[0] works.
if ($PackageMode) {
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Packaging for Mac OS 9..." -ForegroundColor Cyan
    Write-Host "=========================================="
    
    $ReleaseRoot = "release_mac"
    $ContentDir = Join-Path $ReleaseRoot "content"
    $TempDir = Join-Path $ReleaseRoot "temp"
    
    $AppDir = Join-Path $ContentDir "Quake 3 Arena"
    $BaseQ3Dir = Join-Path $AppDir "baseq3"
    $MissionPackDir = Join-Path $AppDir "missionpack"
    
    # These are generated staging trees. Reusing content can silently package
    # binaries or assets left by a different CMake configuration.
    if (Test-Path $ContentDir) { Remove-Item -Recurse -Force $ContentDir }
    if (Test-Path $TempDir) { Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue }
    if (-not (Test-Path $BaseQ3Dir)) { New-Item -ItemType Directory -Path $BaseQ3Dir -Force | Out-Null }
    if (-not (Test-Path $TempDir)) { New-Item -ItemType Directory -Path $TempDir -Force | Out-Null }
    
    # 1. Copy Binary (Base Game)
    if (Test-Path "build_mac\Quake3") {
        Copy-Item "build_mac\Quake3" "$AppDir\Quake3" -Force
    }
    elseif (Test-Path "build_mac\Quake3.exe") {
        Copy-Item "build_mac\Quake3.exe" "$AppDir\Quake3" -Force 
    }
    else {
        Write-Host "Error: Binary build_mac\Quake3 not found." -ForegroundColor Red
        exit 1
    }

    # 1b. Copy Team Arena only when this configuration built it.
    if ($BuildTeamArena) {
        if (Test-Path "build_mac\Quake3_TeamArena") {
            Copy-Item "build_mac\Quake3_TeamArena" "$AppDir\Quake3_TeamArena" -Force
            Write-Host "Found Team Arena Binary." -ForegroundColor Green
        }
        elseif (Test-Path "build_mac\Quake3_TeamArena.exe") {
            Copy-Item "build_mac\Quake3_TeamArena.exe" "$AppDir\Quake3_TeamArena" -Force
            Write-Host "Found Team Arena Binary." -ForegroundColor Green
        }
        else {
            throw "Team Arena is enabled but its executable is missing."
        }
    }

    # 2. Asset Retrieval
    Write-Host "Checking for assets..."
    
    # Search recursively for pak0.pk3 in current directory, excluding release_mac
    $LocalPak = Get-ChildItem -Path . -Filter "pak0.pk3" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match "baseq3" -and $_.FullName -notmatch "release_mac" } | Select-Object -First 1
    $MissionPak = Get-ChildItem -Path . -Filter "pak0.pk3" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match "missionpack" -and $_.FullName -notmatch "release_mac" } | Select-Object -First 1

    if ($LocalPak) {
        Write-Host "Found Base Game Data at $($LocalPak.FullName)"
        Get-ChildItem -Path $LocalPak.Directory.FullName -Filter "pak*.pk3" -File |
            Copy-Item -Destination $BaseQ3Dir -Force
        
        if ($BuildTeamArena) {
            if (-not $MissionPak) {
                throw "Team Arena is enabled but missionpack\pak0.pk3 was not found."
            }
            Write-Host "Found Team Arena Data at $($MissionPak.FullName)"
            if (-not (Test-Path $MissionPackDir)) { New-Item -ItemType Directory -Path $MissionPackDir -Force | Out-Null }
            Get-ChildItem -Path $MissionPak.Directory.FullName -Filter "pak*.pk3" -File |
                Copy-Item -Destination $MissionPackDir -Force
        }

        # Check for Updates (pak1-8)
        $NeedUpdate = $false
        if (-not (Test-Path "$BaseQ3Dir\pak1.pk3")) { $NeedUpdate = $true }
        
        if ($BuildTeamArena -and
            -not (Test-Path "$MissionPackDir\pak1.pk3")) {
            $NeedUpdate = $true
        }

        if ($NeedUpdate) {
            Write-Host "Downloading Updates (BaseQ3 / MissionPack)..." -ForegroundColor Yellow
            $PrUrl = "https://files.ioquake3.org/quake3-latest-pk3s.zip"
            $PrFile = Join-Path $TempDir "quake3-latest-pk3s.zip"
            
            Invoke-WebRequest -Uri $PrUrl -OutFile $PrFile
            
            if (Test-Path $PrFile) {
                Write-Host "Extracting Updates..."
                $PrExtract = Join-Path $TempDir "pr_extract"
                Expand-Archive -Path $PrFile -DestinationPath $PrExtract -Force
                
                Write-Host "Copying BaseQ3 Updates..."
                Get-ChildItem -Path $PrExtract -Recurse -Filter "pak*.pk3" | Where-Object { $_.FullName -match "baseq3" } | ForEach-Object {
                    Copy-Item $_.FullName "$BaseQ3Dir" -Force
                }
                
                if ($BuildTeamArena -and $MissionPak) {
                    Write-Host "Copying MissionPack Updates..."
                    Get-ChildItem -Path $PrExtract -Recurse -Filter "pak*.pk3" | Where-Object { $_.FullName -match "missionpack" } | ForEach-Object {
                        Copy-Item $_.FullName "$MissionPackDir" -Force
                    }
                }
            }
            else {
                Write-Host "Warning: Failed to download Updates." -ForegroundColor Yellow
            }
        }
    }
    else {
        throw ("A retail baseq3\pak0.pk3 was not found. The demo pak belongs " +
            "to demoq3 and is not compatible with this full-game build. " +
            "Place legally obtained retail data under a baseq3 directory and retry.")
    }
   
    # 3. Compile Mac Resources
    Write-Host "Compiling Mac Resources..."
    $RezTool = Join-Path $ToolsDir "Rez.exe"
    if (-not (Test-Path $RezTool)) {
        $RezTool = Join-Path $ToolsDir "Rez"
    }
    $Retro68InstallRoot = Split-Path $ToolsDir -Parent
    $RIncludeCandidates = @(
        (Join-Path $Retro68InstallRoot "universal\RIncludes"),
        (Join-Path (Get-Location) "tools\Retro68-src\InterfacesAndLibraries\Interfaces\RIncludes")
    )
    $RIncludes = $RIncludeCandidates |
        Where-Object {
            (Test-Path (Join-Path $_ "Types.r")) -and
            (Test-Path (Join-Path $_ "CodeFragments.r"))
        } |
        Select-Object -First 1
    if (-not $RIncludes) {
        throw ("Retro68 Rez includes are incomplete. Expected Types.r and " +
            "CodeFragments.r under the prepared or source RIncludes directory.")
    }
    $RsrcFile = Join-Path $BuildDir "Quake3.rsrc"
   
    # An application without its BNDL/FREF/icon resources is not a complete
    # Classic Mac release.
    if (-not (Test-Path "code\mac\quake3_icons.r")) {
        throw "code\mac\quake3_icons.r is missing."
    }

    if (Test-Path $RezTool) {
        & $RezTool -o $RsrcFile -I $RIncludes "code\mac\mac_resources.r"
        if ($LASTEXITCODE -ne 0) {
            throw "Rez failed to compile the Classic Mac resources."
        }
    }
    else {
        throw "Rez tool not found at $RezTool."
    }

    # 4. Generate AppleDouble
    $RsrcFile = Join-Path $BuildDir "Quake3.rsrc"
    $SplitRsrcFile = Join-Path $BuildDir ".rsrc\Quake3.rsrc"
    
    if (Test-Path $SplitRsrcFile) {
        $RsrcFile = $SplitRsrcFile
    }

    if (Test-Path $RsrcFile) {
        Write-Host "Generating AppleDouble resource fork..."
        # mkisofs expects '%' prefix for AppleDouble files to merge them
        $AppleDoubleFile = Join-Path $AppDir "%Quake3"
        python create_appledouble.py "$RsrcFile" "$AppleDoubleFile"
       
        if (Test-Path "$AppDir\Quake3_TeamArena") {
            Copy-Item $AppleDoubleFile "$AppDir\%Quake3_TeamArena" -Force
        }
    }
    else {
        throw "Quake3.rsrc not found; refusing an incomplete Classic application."
    }

    # 5. Create Image
    # Check for mkisofs/genisoimage
    if (Get-Command "genisoimage" -ErrorAction SilentlyContinue) {
        $MkIsoFs = "genisoimage"
    }
    elseif (Get-Command "mkisofs" -ErrorAction SilentlyContinue) {
        $MkIsoFs = "mkisofs"
    }
    else {
        throw ("An HFS-capable genisoimage or mkisofs is required. A normal ZIP " +
            "does not preserve this application's resource fork.")
    }
    
    Write-Host "Creating HFS Disk Image..."
    
    # Mapping file
    $MappingFile = Join-Path $TempDir "hfs_mapping.txt"
    Set-Content -Path $MappingFile -Value (
        ".pk3   Raw   IDQ3 Stak 'Quake 3 Data'",
        ".cfg   Ascii IDQ3 TEXT 'Quake 3 Config'",
        "Quake3 Raw   IDQ3 APPL 'Quake 3 App'",
        "Quake3_TeamArena Raw IDQ3 APPL 'Quake 3 Team Arena'"
    )
    
    # Run mkisofs (Hybrid HFS)
    # Using 'iso ' type code later to identify it as ISO9660
    $ImageName = Join-Path $ReleaseRoot "Quake3_Install.img"
    if (Test-Path $ImageName) { Remove-Item $ImageName -Force }
    & $MkIsoFs -hfs -double -map $MappingFile -o $ImageName -V "Quake 3 Arena" $ContentDir | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "HFS image creation failed for $ImageName."
    }
    
    if ((Test-Path $ImageName) -and (Get-Item $ImageName).Length -gt 0) {
        Write-Host "Image created: $ImageName" -ForegroundColor Green
        
        # MacBinary Encode
        # Type: 'iso ' (ISO Image), Creator: dCpy (Disk Copy)
        Write-Host "Encoding as MacBinary II..."
        $BinName = Join-Path $ReleaseRoot "Quake3_Install.img.bin"
        if (Test-Path $BinName) { Remove-Item $BinName -Force }
        python macbinary_encode.py $ImageName $BinName "iso " "dCpy"
        if ($LASTEXITCODE -ne 0) {
            throw "MacBinary encoding failed for $ImageName."
        }
        
        if ((Test-Path $BinName) -and (Get-Item $BinName).Length -gt 128) {
            Write-Host "Package created: $BinName" -ForegroundColor Green
            Remove-Item $ImageName -Force
        }
        else {
            throw "MacBinary output is missing or empty: $BinName"
        }
    }
    else {
        throw "HFS image output is missing or empty: $ImageName"
    }
    
    # ---------------------------------------------------------
    # Create Binary-Only Image
    # ---------------------------------------------------------
    Write-Host "Creating Binaries-Only Image..."
    $BinImgName = Join-Path $ReleaseRoot "Quake3_Bin.img"
    if (Test-Path $BinImgName) { Remove-Item $BinImgName -Force }
    $BinContentDir = Join-Path $ReleaseRoot "bin_content"
    if (Test-Path $BinContentDir) { Remove-Item -Recurse -Force $BinContentDir }
    New-Item -ItemType Directory -Path $BinContentDir | Out-Null
    
    # Copy Binaries and AppleDouble
    if (Test-Path "$AppDir\Quake3") { Copy-Item "$AppDir\Quake3" $BinContentDir }
    if (Test-Path "$AppDir\%Quake3") { Copy-Item "$AppDir\%Quake3" $BinContentDir }
    if (Test-Path "$AppDir\Quake3_TeamArena") { Copy-Item "$AppDir\Quake3_TeamArena" $BinContentDir }
    if (Test-Path "$AppDir\%Quake3_TeamArena") { Copy-Item "$AppDir\%Quake3_TeamArena" $BinContentDir }
    
    & $MkIsoFs -hfs -double -map $MappingFile -o $BinImgName -V "Quake 3 Binaries" $BinContentDir | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "HFS image creation failed for $BinImgName."
    }
    
    if ((Test-Path $BinImgName) -and (Get-Item $BinImgName).Length -gt 0) {
        Write-Host "Encoding Binaries Image..."
        $BinBinName = Join-Path $ReleaseRoot "Quake3_Bin.img.bin"
        if (Test-Path $BinBinName) { Remove-Item $BinBinName -Force }
        python macbinary_encode.py $BinImgName $BinBinName "iso " "dCpy"
        if ($LASTEXITCODE -ne 0) {
            throw "MacBinary encoding failed for $BinImgName."
        }
        if ((Test-Path $BinBinName) -and (Get-Item $BinBinName).Length -gt 128) {
            Write-Host "Package created: $BinBinName" -ForegroundColor Green
            Remove-Item $BinImgName -Force
        }
        else {
            throw "MacBinary output is missing or empty: $BinBinName"
        }
    }
    else {
        throw "HFS image output is missing or empty: $BinImgName"
    }
    
    # Cleanup
    Write-Host "Cleaning up temp files..."
    Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue
}
