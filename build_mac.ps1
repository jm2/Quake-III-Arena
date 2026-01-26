# Quake III Arena Build Script for Windows (Retro68)
# Builds the monolithic Classic Mac OS app using the Retro68 toolchain.

$ErrorActionPreference = "Stop"

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
}
else {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}

Pop-Location

# Packaging Subcommand (Simple Switch)
# PowerShell arguments aren't passed as $1 automatically if strictly defined params, 
# but if script has no param block, $args[0] works.
if ($args[0] -eq "package") {
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host "Packaging for Mac OS 9..." -ForegroundColor Cyan
    Write-Host "=========================================="
    
    $ReleaseRoot = "release_mac"
    $ContentDir = Join-Path $ReleaseRoot "content"
    $TempDir = Join-Path $ReleaseRoot "temp"
    
    $AppDir = Join-Path $ContentDir "Quake 3 Arena"
    $BaseQ3Dir = Join-Path $AppDir "baseq3"
    $MissionPackDir = Join-Path $AppDir "missionpack"
    
    # Cleanup previous temp if exists
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

    # 1b. Copy Binary (Team Arena) - Optional
    if (Test-Path "build_mac\Quake3_TeamArena") {
        Copy-Item "build_mac\Quake3_TeamArena" "$AppDir\Quake3_TeamArena" -Force
        Write-Host "Found Team Arena Binary." -ForegroundColor Green
    }
    elseif (Test-Path "build_mac\Quake3_TeamArena.exe") {
        Copy-Item "build_mac\Quake3_TeamArena.exe" "$AppDir\Quake3_TeamArena" -Force
        Write-Host "Found Team Arena Binary." -ForegroundColor Green
    }

    # 2. Asset Retrieval
    Write-Host "Checking for assets..."
    
    # Search recursively for pak0.pk3 in current directory, excluding release_mac
    $LocalPak = Get-ChildItem -Path . -Filter "pak0.pk3" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match "baseq3" -and $_.FullName -notmatch "release_mac" } | Select-Object -First 1
    $MissionPak = Get-ChildItem -Path . -Filter "pak0.pk3" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match "missionpack" -and $_.FullName -notmatch "release_mac" } | Select-Object -First 1

    if ($LocalPak) {
        Write-Host "Found Base Game Data at $($LocalPak.FullName)"
        Copy-Item $LocalPak.FullName "$BaseQ3Dir\pak0.pk3" -Force
        
        if ($MissionPak) {
            Write-Host "Found Team Arena Data at $($MissionPak.FullName)"
            if (-not (Test-Path $MissionPackDir)) { New-Item -ItemType Directory -Path $MissionPackDir -Force | Out-Null }
            Copy-Item $MissionPak.FullName "$MissionPackDir\pak0.pk3" -Force
        }

        # Check for Updates (pak1-8)
        $NeedUpdate = $false
        if (-not (Test-Path "pak1.pk3") -and -not (Test-Path "$BaseQ3Dir\pak1.pk3")) { $NeedUpdate = $true }
        
        if ($MissionPak -and -not (Test-Path "missionpack\pak1.pk3") -and -not (Test-Path "$MissionPackDir\pak1.pk3")) { $NeedUpdate = $true }

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
                
                if ($MissionPak) {
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
        Write-Host "Local pak0.pk3 not found. Falling back to Demo assets..." -ForegroundColor Yellow
        $DemoInstaller = Join-Path $TempDir "linuxq3ademo.sh"
        $DemoUrl = "https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake3/linux/linuxq3ademo-1.11-6.x86.gz.sh"
        
        if (-not (Test-Path $DemoInstaller)) {
            Write-Host "Downloading Quake 3 Demo..."
            try {
                Invoke-WebRequest -Uri $DemoUrl -OutFile $DemoInstaller
            }
            catch {
                Write-Host "Error downloading demo: $_" -ForegroundColor Red
                exit 1
            }
        }
        
        Write-Host "Extracting Demo Assets..."
        # Try unar or 7z if available
        if (Get-Command "unar" -ErrorAction SilentlyContinue) {
            # Extraction dir
            $ExtractDir = Join-Path $TempDir "extracted"
            if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }
             
            & unar -f $DemoInstaller -o $ExtractDir | Out-Null
            $DemoPak = Get-ChildItem -Path $ExtractDir -Recurse -Filter "pak0.pk3" | Select-Object -First 1
            if ($DemoPak) {
                Copy-Item $DemoPak.FullName "$BaseQ3Dir\pak0.pk3" -Force
                Write-Host "Demo assets packaged." -ForegroundColor Green
            }
            else {
                Write-Host "Error: pak0.pk3 not found in extraction." -ForegroundColor Red
                exit 1
            }
        }
        else {
            Write-Host "Error: 'unar' not found. Cannot extract .sh installer on Windows easily without it." -ForegroundColor Red
            Write-Host "Please run 'setup_retro68.ps1' to automatically build 'unar', or install it manually."
            exit 1
        }
    }
   
    # 3. Compile Mac Resources
    Write-Host "Compiling Mac Resources..."
    $RezTool = Join-Path $ToolsDir "Rez.exe"
    $RIncludes = Join-Path $ToolsDir "..\RIncludes"
    $RsrcFile = Join-Path $BuildDir "Quake3.rsrc"
   
    if (Test-Path $RezTool) {
        & $RezTool -o $RsrcFile -I $RIncludes "code\mac\mac_resources.r"
    }
    else {
        Write-Host "Warning: Rez tool not found at $RezTool" -ForegroundColor Yellow
    }

    # 4. Generate AppleDouble
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
        Write-Host "Warning: Quake3.rsrc not found. Icons/Type/Creator will be missing." -ForegroundColor Yellow
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
        Write-Host "Warning: HFS creation tool (genisoimage/mkisofs) not found." -ForegroundColor Yellow
        Write-Host "Creating a standard ZIP archive instead." -ForegroundColor Yellow
        $ZipName = Join-Path $ReleaseRoot "Quake3_Install_Win.zip"
        Compress-Archive -Path "$ContentDir\*" -DestinationPath $ZipName -Force
        exit 0
    }
    
    Write-Host "Creating HFS Disk Image..."
    
    # Mapping file
    $MappingFile = Join-Path $TempDir "hfs_mapping.txt"
    Set-Content -Path $MappingFile -Value (
        ".pk3   Raw   Q3A  Stak 'Quake 3 Data'",
        ".cfg   Ascii Q3A  TEXT 'Quake 3 Config'",
        "Quake3 Raw   Q3A  APPL 'Quake 3 App'",
        "Quake3_TeamArena Raw Q3A APPL 'Quake 3 Team Arena'"
    )
    
    # Run mkisofs (Hybrid HFS)
    # Using 'iso ' type code later to identify it as ISO9660
    $ImageName = Join-Path $ReleaseRoot "Quake3_Install.img"
    & $MkIsoFs -hfs -double -map $MappingFile -o $ImageName -V "Quake 3 Arena" $ContentDir | Out-Null
    
    if (Test-Path $ImageName) {
        Write-Host "Image created: $ImageName" -ForegroundColor Green
        
        # MacBinary Encode
        # Type: 'iso ' (ISO Image), Creator: dCpy (Disk Copy)
        Write-Host "Encoding as MacBinary II..."
        $BinName = Join-Path $ReleaseRoot "Quake3_Install.img.bin"
        python macbinary_encode.py $ImageName $BinName "iso " "dCpy"
        
        if (Test-Path $BinName) {
            Write-Host "Package created: $BinName" -ForegroundColor Green
            Remove-Item $ImageName -Force
        }
        else {
            Write-Host "Error: MacBinary encoding failed." -ForegroundColor Red
        }
    }
    else {
        Write-Host "Error creating image." -ForegroundColor Red
    }
    
    # Cleanup
    Write-Host "Cleaning up temp files..."
    Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue
}
