# Check that the Retro68 toolchain runs on this host (issue #269).
#
#   .\check_retro68.ps1 [-ToolsOnly] [-InstallDir <dir>]
#
# PowerShell counterpart of check_retro68.sh, which describes the steps: run
# the compiler, archiver, linker, MakePEF, MakeImport and Rez the build uses,
# and unless -ToolsOnly is given, compile and link a C file with the
# CMakeLists.txt flags, convert it with MakePEF and build an application with
# Rez. On Windows a missing DLL shows up as exit status 0xC0000135.
#
# Exit status: 0 when the toolchain runs; 1 when it is broken (the message
# names the failed step); 3 when the check itself could not run, for example
# because the scratch directory under TMPDIR/TEMP could not be created or
# written. Callers must treat only status 1 as a broken toolchain.

param(
    [string]$InstallDir = "",
    [switch]$ToolsOnly
)

if (-not $InstallDir) {
    $InstallDir = Join-Path (Join-Path $PSScriptRoot "tools") "Retro68-build"
}
$Bin = Join-Path $InstallDir "bin"
$Target = "powerpc-apple-macos"

# Keep in step with CMakeLists.txt, cmake/Retro68.toolchain.cmake and
# check_retro68.sh.
$CompileFlags = @("-std=gnu99", "-fgnu89-inline", "-O0", "-g",
    "-fno-strict-aliasing", "-fsigned-char", "-D__MACOS__", "-D__POWERPC__")
$CxxFlags = @("-fsigned-char")

# Messages from a tool that could not write its output for lack of space.
$SpaceErrors = "No space left on device|Disk quota exceeded|Read-only file system|not enough space on the disk"

$Work = $null
$PreviousPath = $env:PATH

function Remove-CheckState {
    if ($Work -and (Test-Path $Work)) {
        Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
    }
    $env:PATH = $PreviousPath
}

function Write-ToolOutput {
    param([string]$Output)

    if ($Output) {
        $Output -split "`r?`n" | Select-Object -First 20 |
            ForEach-Object { Write-Host "  | $_" }
    }
}

# Status 3: the check could not run. Says nothing about the toolchain.
function Stop-CannotCheck {
    param([string]$Problem, [string]$Output = "")

    Write-Host "Error: could not check the Retro68 toolchain in ${InstallDir}:" -ForegroundColor Red
    Write-Host "  $Problem"
    Write-ToolOutput $Output
    Write-Host "The toolchain itself was not judged. Make the scratch directory ($TempRoot)"
    Write-Host "an existing, writable directory with free space and run the check again."
    Remove-CheckState
    exit 3
}

# Succeeds when the scratch directory still takes $Size KiB.
function Test-ScratchWritable {
    param([int]$Size)

    $Probe = Join-Path $Work "space.probe"
    try {
        [System.IO.File]::WriteAllBytes($Probe, (New-Object byte[] ($Size * 1024)))
        return ((Get-Item $Probe).Length -eq ($Size * 1024))
    }
    catch {
        return $false
    }
    finally {
        Remove-Item -Force $Probe -ErrorAction SilentlyContinue
    }
}

# Status 1: the toolchain is broken, unless the scratch directory explains
# the failure.
function Stop-Check {
    param([string]$Step, [string]$Output = "")

    if ($Work) {
        if ($Output -match $SpaceErrors) {
            Stop-CannotCheck "$Step failed for lack of space in the scratch directory ${Work}:" $Output
        }
        if (-not (Test-ScratchWritable 64)) {
            Stop-CannotCheck "$Step failed, and the scratch directory $Work no longer takes writes." $Output
        }
    }
    Write-Host "Error: Retro68 toolchain check failed in ${InstallDir}:" -ForegroundColor Red
    Write-Host "  $Step"
    Write-ToolOutput $Output
    Write-Host "If a host shared library or DLL is missing (for example after an OS"
    Write-Host "upgrade), install it or rebuild the toolchain with setup_retro68.ps1. See"
    Write-Host "`"Checking and rebuilding the toolchain`" in docs/building-mac-os9.md."
    Remove-CheckState
    exit 1
}

function Find-Tool {
    param([string]$Name)

    foreach ($Candidate in @("$Name.exe", $Name)) {
        $Path = Join-Path $Bin $Candidate
        if (Test-Path $Path -PathType Leaf) {
            return $Path
        }
    }
    Stop-Check "$Name is missing from $Bin."
}

function Get-ExitStatusText {
    param([int]$Code)

    switch ($Code) {
        -1073741515 { return "0xC0000135: a DLL it needs was not found" }
        -1073741511 { return "0xC0000139: a DLL it loads lacks an entry point" }
        -1073741701 { return "0xC000007B: a DLL it loads is not a valid image" }
    }
    return "$Code"
}

# Runs a native tool with no input and returns its exit status and combined
# output. Native stderr must not become a terminating error under a caller's
# "Stop" setting.
function Invoke-Tool {
    param([string]$Path, [string[]]$Arguments)

    $PreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $Lines = $null | & $Path @Arguments 2>&1 | ForEach-Object { "$_" }
        $Code = $LASTEXITCODE
    }
    catch {
        $Lines = @("$_")
        $Code = 127
    }
    finally {
        $ErrorActionPreference = $PreviousPreference
    }
    return @{ Code = $Code; Output = ($Lines -join "`n") }
}

# A tool that must succeed.
function Invoke-Step {
    param([string]$Step, [string]$Path, [string[]]$Arguments)

    $Result = Invoke-Tool $Path $Arguments
    if ($Result.Code -ne 0) {
        Stop-Check "$Step failed (exit status $(Get-ExitStatusText $Result.Code)):" $Result.Output
    }
}

# A tool run without input: its own usage error is fine, but the loader must
# not stop it.
function Invoke-Probe {
    param([string]$Name, [string]$Path, [string[]]$Arguments)

    $Result = Invoke-Tool $Path $Arguments
    if ($Result.Code -ge 126 -or $Result.Code -lt 0 -or
        $Result.Output -match "error while loading shared libraries|symbol lookup error|Library not loaded|not found \(required by") {
        Stop-Check "$Name could not start (exit status $(Get-ExitStatusText $Result.Code)):" $Result.Output
    }
}

# Writes lines to a scratch file and reads them back.
function Write-ScratchFile {
    param([string]$Path, [string[]]$Lines)

    try {
        Set-Content -Path $Path -Value $Lines -Encoding Ascii -ErrorAction Stop
        $Written = Get-Content -Path $Path -ErrorAction Stop
        if (($Written -join "`n") -cne ($Lines -join "`n")) {
            throw "read back different contents"
        }
    }
    catch {
        Stop-CannotCheck "could not write ${Path}: $_"
    }
}

if ($env:TMPDIR) {
    $TempRoot = $env:TMPDIR
}
elseif ($env:OS -eq "Windows_NT") {
    $TempRoot = [System.IO.Path]::GetTempPath()
}
else {
    $TempRoot = "/var/tmp"
}

$Gcc = Find-Tool "$Target-gcc"
$Gxx = Find-Tool "$Target-g++"
$Ar = Find-Tool "$Target-ar"
$Ranlib = Find-Tool "$Target-ranlib"
$Ld = Find-Tool "$Target-ld"
$MakePEF = Find-Tool "MakePEF"
$MakeImport = Find-Tool "MakeImport"
$Rez = Find-Tool "Rez"

try {
    if (-not (Test-Path $TempRoot -PathType Container)) {
        throw "$TempRoot is not a directory"
    }
    $Work = Join-Path $TempRoot ("q3-retro68-check." + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $Work -ErrorAction Stop | Out-Null
}
catch {
    $Work = $null
    Stop-CannotCheck "could not create a scratch directory under ${TempRoot}: $_"
}
# The check writes well under 1 MiB; make sure that much fits before any tool
# runs, so a full scratch directory is not mistaken for a broken compiler.
if (-not (Test-ScratchWritable 1024)) {
    Stop-CannotCheck "the scratch directory $Work does not take 1 MiB (full or read-only)."
}
$env:PATH = $Bin + [System.IO.Path]::PathSeparator + $env:PATH

$CheckC = Join-Path $Work "check.c"
$CheckCxx = Join-Path $Work "check_cxx.cc"
$CheckObject = Join-Path $Work "check.o"
$CheckLibrary = Join-Path $Work "libcheck.a"
if ($ToolsOnly) {
    $CSource = @("int retro68_check(void)", "{", "`treturn 0;", "}")
}
else {
    $CSource = @("#include <MacTypes.h>", "#include <math.h>", "",
        "int main(int argc, char **argv)", "{",
        "`tBoolean ok = argc > 0 && argv != NULL;",
        "`treturn ok && floor(1.5) == 1.0 ? 0 : 1;", "}")
}
Write-ScratchFile $CheckC $CSource
Write-ScratchFile $CheckCxx @("int retro68_check_cxx(int value)", "{",
    "`treturn value + 1;", "}")

Invoke-Step "$Target-gcc --version" $Gcc @("--version")
Invoke-Step "compiling a C file ($Target-gcc -c)" $Gcc `
    ($CompileFlags + @("-c", $CheckC, "-o", $CheckObject))
Invoke-Step "compiling a C++ file ($Target-g++ -c)" $Gxx `
    ($CxxFlags + @("-c", $CheckCxx, "-o", (Join-Path $Work "check_cxx.o")))
Invoke-Step "archiving the object ($Target-ar qc)" $Ar @("qc", $CheckLibrary, $CheckObject)
Invoke-Step "indexing the archive ($Target-ranlib)" $Ranlib @($CheckLibrary)
Invoke-Step "$Target-ld --version" $Ld @("--version")
Invoke-Probe "MakePEF" $MakePEF @()
Invoke-Probe "MakeImport" $MakeImport @()
Invoke-Probe "Rez" $Rez @("--help")

if (-not $ToolsOnly) {
    $CheckXcoff = Join-Path $Work "check.xcoff"
    $CheckPef = Join-Path $Work "check.pef"
    $CheckRez = Join-Path $Work "check.r"
    $CheckApplication = Join-Path $Work "check.bin"
    Invoke-Step "linking an XCOFF image ($Target-gcc)" $Gcc `
        ($CompileFlags + @("-Wl,--whole-archive", $CheckLibrary,
            "-Wl,--no-whole-archive", "-lm", "-lInterfaceLib", "-o", $CheckXcoff))
    Invoke-Step "converting the XCOFF image to PEF (MakePEF)" $MakePEF `
        @($CheckXcoff, "-o", $CheckPef)
    # "Joy!" "peff" and the "pwpc" architecture: a PowerPC PEF container.
    $Header = ""
    if (Test-Path $CheckPef) {
        $Bytes = [System.IO.File]::ReadAllBytes($CheckPef)
        if ($Bytes.Length -ge 12) {
            $Header = [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 12)
        }
    }
    if ($Header -cne "Joy!peffpwpc") {
        Stop-Check "MakePEF did not write a PowerPC PEF (no Joy!peff/pwpc header)."
    }
    Write-ScratchFile $CheckRez @("data 'Q3ck' (128) {", "`t`$`"00`"", "};")
    Invoke-Step "building an application with Rez" $Rez `
        @($CheckRez, "-t", "APPL", "-c", "IDQ3", "--data", $CheckPef, "-o", $CheckApplication)
    if (-not (Test-Path $CheckApplication) -or (Get-Item $CheckApplication).Length -eq 0) {
        Stop-Check "Rez did not write an application."
    }
}

Remove-CheckState
if ($ToolsOnly) {
    Write-Host "Retro68 tools run: $InstallDir"
}
else {
    Write-Host "Retro68 toolchain check passed: $InstallDir"
}
exit 0
