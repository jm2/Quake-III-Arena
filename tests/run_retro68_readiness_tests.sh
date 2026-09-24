#!/usr/bin/env bash
# Issue #269: the Retro68 readiness checks must run the toolchain, not only
# find its files. After a host OS upgrade `powerpc-apple-macos-gcc --version`
# still worked while cc1 (libisl), ar (libfl), Rez and MakeImport (Boost)
# could not load, build_mac.sh declared the toolchain ready, and
# setup_retro68.sh resumed with --skip-thirdparty, which never rebuilds them.
#
# The toolchain here is made of shell stubs in a scratch directory, so no
# Retro68 install or $CC is needed and every CI job runs the same checks. The
# runner checks check_retro68.sh with working and broken stubs, then runs
# build_mac.sh and setup_retro68.sh in scratch copies of the repository with
# stub cmake, setup and build-toolchain.bash commands: a broken toolchain must
# stop build_mac.sh before CMake with the missing library named, and must make
# setup_retro68.sh clean and rebuild everything. When pwsh is installed the
# same stubs go through check_retro68.ps1. When a real toolchain is present
# ($Q3_RETRO68_DIR, default tools/Retro68-build) it must pass the full check.
set -uo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
export LC_ALL=C
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-retro68-readiness.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_WORK"' EXIT

Q3_CHECK_TMP="$Q3_TEST_WORK/tmp"
Q3_OUT="$Q3_TEST_WORK/output.log"
Q3_STATUS=0
Q3_FAILED=0
mkdir -p "$Q3_CHECK_TMP"

# expect NAME STATUS [TEXT...]: the last command exited with STATUS and
# printed every TEXT.
expect() {
    local name="$1" want="$2" text ok=1
    shift 2
    [ "$Q3_STATUS" -eq "$want" ] || ok=0
    for text in "$@"; do
        grep -q -F -- "$text" "$Q3_OUT" || { ok=0; echo "  missing: $text"; }
    done
    if [ "$ok" -eq 1 ]; then
        echo "PASS: $name"
    else
        echo "FAIL: $name (exit status $Q3_STATUS, expected $want); output:"
        sed 's/^/    /' "$Q3_OUT"
        Q3_FAILED=1
    fi
}

# check NAME CONDITION...: a shell test that must hold.
check() {
    local name="$1"
    shift
    if "$@"; then
        echo "PASS: $name"
    else
        echo "FAIL: $name"
        Q3_FAILED=1
    fi
}

run_check() {
    Q3_STATUS=0
    TMPDIR="$Q3_CHECK_TMP" bash "$Q3_TEST_ROOT/check_retro68.sh" "$@" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

# write_stub PATH LINE...: a tool that logs its arguments to calls.log in the
# toolchain directory, then runs LINE...
write_stub() {
    local path="$1"
    shift
    {
        echo '#!/bin/sh'
        printf 'echo "${0##*/} $*" >> "%s/calls.log"\n' "$(dirname "$(dirname "$path")")"
        printf '%s\n' "$@"
    } > "$path"
    chmod +x "$path"
}

# The value after -o.
Q3_OUTPUT_ARG='out=""; prev=""; for arg in "$@"; do [ "$prev" = "-o" ] && out="$arg"; prev="$arg"; done'

# loader_error PATH LIBRARY: a tool the host loader cannot start.
loader_error() {
    write_stub "$1" \
        "echo \"\$0: error while loading shared libraries: $2: cannot open shared object file: No such file or directory\" >&2" \
        'exit 127'
}

# make_toolchain DIR: a working stub Retro68 install.
make_toolchain() {
    local dir="$1" driver
    mkdir -p "$dir/bin" "$dir/powerpc-apple-macos/include"
    : > "$dir/calls.log"
    for driver in gcc g++; do
        write_stub "$dir/bin/powerpc-apple-macos-$driver" \
            'case " $* " in *" --version "*) echo "powerpc-apple-macos-gcc (GCC) 12.2.0"; exit 0;; esac' \
            "$Q3_OUTPUT_ARG" \
            '[ -n "$out" ] || exit 1' \
            'echo stub > "$out"'
    done
    write_stub "$dir/bin/powerpc-apple-macos-ar" \
        '[ "$1" = --version ] && { echo "GNU ar (GNU Binutils) 2.39"; exit 0; }' \
        '[ "$1" = qc ] && [ -f "$3" ] || exit 1' \
        'echo "!<arch>" > "$2"'
    write_stub "$dir/bin/powerpc-apple-macos-ranlib" '[ -f "$1" ]'
    write_stub "$dir/bin/powerpc-apple-macos-ld" \
        '[ "$1" = --version ] && { echo "GNU ld (GNU Binutils) 2.39"; exit 0; }' \
        'exit 1'
    write_stub "$dir/bin/MakePEF" \
        '[ "$#" -eq 0 ] && { echo "makepef: no input file specified." >&2; exit 1; }' \
        "$Q3_OUTPUT_ARG" \
        '[ -f "$1" ] && [ -n "$out" ] || exit 1' \
        'printf "Joy!peffpwpc\000\000\000\001" > "$out"'
    write_stub "$dir/bin/MakeImport" \
        'echo "Usage: makeimport <peflib> <libname>"' \
        'exit 1'
    write_stub "$dir/bin/Rez" \
        '[ "$1" = --help ] && { echo "Usage: Rez [options] input-file"; exit 0; }' \
        "$Q3_OUTPUT_ARG" \
        '[ -n "$out" ] || exit 1' \
        'echo "stub application" > "$out"'
    write_stub "$dir/bin/ConvertDiskImage" \
        'echo "Usage: ConvertDiskImage input.img output.dsk"' \
        'exit 1'
}

# A driver that still prints its version but whose cc1 cannot load, as
# observed on the Fedora 44 host.
break_cc1() {
    write_stub "$1/bin/powerpc-apple-macos-gcc" \
        'case " $* " in *" --version "*) echo "powerpc-apple-macos-gcc (GCC) 12.2.0"; exit 0;; esac' \
        "echo \"\${0%/*}/../libexec/gcc/powerpc-apple-macos/12.2.0/cc1: error while loading shared libraries: libisl.so.23: cannot open shared object file: No such file or directory\" >&2" \
        'exit 1'
}

TC="$Q3_TEST_WORK/toolchain"
fresh_toolchain() {
    rm -rf "$TC"
    make_toolchain "$TC"
}

# ---- check_retro68.sh ----

fresh_toolchain
run_check "$TC"
expect "working toolchain passes the full check" 0 "Retro68 toolchain check passed: $TC"
# Every flag CMakeLists.txt appends to CMAKE_C_FLAGS, and the definitions it
# and the toolchain file add, must reach the check's compile.
compiles_with_build_flags() {
    local flags line flag
    flags="$(sed -n 's/^set(CMAKE_C_FLAGS "\${CMAKE_C_FLAGS} \(.*\)")$/\1/p' "$Q3_TEST_ROOT/CMakeLists.txt")"
    [ -n "$flags" ] || { echo "  no CMAKE_C_FLAGS in CMakeLists.txt"; return 1; }
    line="$(grep -m 1 -E '^powerpc-apple-macos-gcc .* -c ' "$TC/calls.log")"
    for flag in $flags -D__MACOS__ -D__POWERPC__; do
        case " $line " in
            *" $flag "*) ;;
            *) echo "  compile lacks $flag: $line"; return 1 ;;
        esac
    done
}
check "full check compiles with the CMakeLists.txt flags" compiles_with_build_flags
check "full check links the archive into an XCOFF image" \
    grep -q -E -- "^powerpc-apple-macos-gcc .*-Wl,--whole-archive .*libcheck\.a -Wl,--no-whole-archive -lm -lInterfaceLib -o .*check\.xcoff$" "$TC/calls.log"
check "full check converts with MakePEF and builds with Rez" \
    grep -q -E -- "^Rez .*check\.r -t APPL -c IDQ3 --data .*check\.pef -o .*check\.bin$" "$TC/calls.log"
check "full check runs the C++ compiler, ar, ranlib, ld and MakeImport" \
    bash -c 'for tool in g++ ar ranlib ld; do grep -q "^powerpc-apple-macos-$tool " "$1" || exit 1; done; grep -q "^MakeImport" "$1"' _ "$TC/calls.log"

fresh_toolchain
run_check --tools-only "$TC"
expect "working toolchain passes --tools-only" 0 "Retro68 tools run: $TC"
check "--tools-only neither links nor converts" \
    bash -c '! grep -q -e "-o .*check\.xcoff" -e "check\.pef" "$1"' _ "$TC/calls.log"

fresh_toolchain
break_cc1 "$TC"
run_check "$TC"
expect "cc1 that cannot load libisl fails the full check" 1 \
    "compiling a C file (powerpc-apple-macos-gcc -c) failed" "libisl.so.23" \
    "docs/building-mac-os9.md"
run_check --tools-only "$TC"
expect "cc1 that cannot load libisl fails --tools-only" 1 "libisl.so.23"

fresh_toolchain
loader_error "$TC/bin/powerpc-apple-macos-g++" libisl.so.23
run_check "$TC"
expect "C++ compiler that cannot load fails" 1 "compiling a C++ file" "libisl.so.23"

fresh_toolchain
loader_error "$TC/bin/powerpc-apple-macos-ar" libfl.so.2
run_check "$TC"
expect "ar that cannot load libfl fails" 1 "archiving the object" "libfl.so.2"

fresh_toolchain
rm "$TC/bin/MakePEF"
run_check "$TC"
expect "missing MakePEF fails" 1 "MakePEF is missing or not executable in $TC/bin."
run_check --tools-only "$TC"
expect "missing MakePEF fails --tools-only" 1 "MakePEF is missing"

fresh_toolchain
loader_error "$TC/bin/MakePEF" libboost_filesystem.so.1.91.0
run_check "$TC"
expect "MakePEF that cannot load Boost fails" 1 \
    "MakePEF could not start (exit status 127)" "libboost_filesystem.so.1.91.0"
run_check --tools-only "$TC"
expect "MakePEF that cannot load Boost fails --tools-only" 1 "libboost_filesystem.so.1.91.0"

fresh_toolchain
loader_error "$TC/bin/MakeImport" libboost_filesystem.so.1.91.0
run_check --tools-only "$TC"
expect "MakeImport that cannot load Boost fails" 1 "MakeImport could not start" \
    "libboost_filesystem.so.1.91.0"

fresh_toolchain
loader_error "$TC/bin/Rez" libboost_wave.so.1.91.0
run_check --tools-only "$TC"
expect "Rez that cannot load Boost fails" 1 "Rez could not start" "libboost_wave.so.1.91.0"

fresh_toolchain
write_stub "$TC/bin/MakePEF" \
    '[ "$#" -eq 0 ] && exit 1' "$Q3_OUTPUT_ARG" 'echo "not a PEF" > "$out"'
run_check "$TC"
expect "MakePEF output without a PEF header fails" 1 "MakePEF did not write a PowerPC PEF"

fresh_toolchain
write_stub "$TC/bin/Rez" '[ "$1" = --help ] && exit 0' 'exit 0'
run_check "$TC"
expect "Rez without output fails" 1 "Rez did not write an application."

# A toolchain whose target libraries are missing: a resumed setup rebuilds
# them, so --tools-only must still pass while the full check fails.
fresh_toolchain
write_stub "$TC/bin/powerpc-apple-macos-gcc" \
    'case " $* " in *" --version "*) echo "powerpc-apple-macos-gcc (GCC) 12.2.0"; exit 0;; esac' \
    "$Q3_OUTPUT_ARG" \
    'case " $* " in *" -c "*) echo stub > "$out"; exit 0;; esac' \
    'echo "ld: cannot find -lretrocrt" >&2' 'exit 1'
run_check "$TC"
expect "missing target libraries fail the full check" 1 \
    "linking an XCOFF image (powerpc-apple-macos-gcc) failed" "cannot find -lretrocrt"
run_check --tools-only "$TC"
expect "missing target libraries pass --tools-only" 0 "Retro68 tools run"

check "the check leaves no scratch files" \
    bash -c '[ -z "$(ls -A "$1")" ]' _ "$Q3_CHECK_TMP"
check "the documentation the check points to exists" \
    grep -q -x -F "## Checking and rebuilding the toolchain" "$Q3_TEST_ROOT/docs/building-mac-os9.md"

# ---- build_mac.sh ----

# make_root DIR: a scratch repository with the build scripts, a stub toolchain
# and the prepared OpenGL files build_mac.sh looks for.
make_root() {
    local root="$1"
    rm -rf "$root"
    mkdir -p "$root/stubs" "$root/tools/Retro68-src/InterfacesAndLibraries/SharedLibraries"
    cp "$Q3_TEST_ROOT/build_mac.sh" "$Q3_TEST_ROOT/setup_retro68.sh" \
        "$Q3_TEST_ROOT/check_retro68.sh" "$root/"
    chmod +x "$root/build_mac.sh" "$root/setup_retro68.sh" "$root/check_retro68.sh"
    make_toolchain "$root/tools/Retro68-build"
    : > "$root/tools/Retro68-build/powerpc-apple-macos/include/gl.h"
    : > "$root/tools/Retro68-build/powerpc-apple-macos/include/agl.h"
    echo "!<arch>" > "$root/tools/Retro68-src/InterfacesAndLibraries/SharedLibraries/libOpenGLLibraryStub.a"
    printf '#!/bin/sh\necho "cmake $*" >> "%s/cmake.called"\nexit 3\n' "$root" > "$root/stubs/cmake"
    chmod +x "$root/stubs/cmake"
}

# A stub setup_retro68.sh: build_mac.sh must not start a rebuild on its own
# when the toolchain is installed but cannot run.
stub_setup() {
    printf '#!/bin/sh\necho setup >> "%s/setup.called"\n' "$1" > "$1/setup_retro68.sh"
}

run_build() {
    local root="$1"
    Q3_STATUS=0
    (cd "$root" && TMPDIR="$Q3_CHECK_TMP" PATH="$root/stubs:$PATH" \
        bash ./build_mac.sh --base-only) > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

ROOT="$Q3_TEST_WORK/root"
make_root "$ROOT"
stub_setup "$ROOT"
break_cc1 "$ROOT/tools/Retro68-build"
run_build "$ROOT"
expect "build_mac.sh stops on a compiler that cannot load libisl" 1 \
    "libisl.so.23" "Stopping before CMake: the Retro68 toolchain cannot build Quake3."
check "build_mac.sh never reaches CMake with that compiler" test ! -e "$ROOT/cmake.called"
check "build_mac.sh does not start a rebuild on its own" test ! -e "$ROOT/setup.called"

make_root "$ROOT"
stub_setup "$ROOT"
loader_error "$ROOT/tools/Retro68-build/bin/Rez" libboost_wave.so.1.91.0
run_build "$ROOT"
expect "build_mac.sh stops on a Rez that cannot load Boost" 1 "libboost_wave.so.1.91.0"
check "build_mac.sh never reaches CMake with that Rez" test ! -e "$ROOT/cmake.called"

make_root "$ROOT"
stub_setup "$ROOT"
run_build "$ROOT"
expect "build_mac.sh configures with a working toolchain" 3 "Retro68 toolchain check passed"
check "build_mac.sh reached CMake" test -e "$ROOT/cmake.called"

# ---- setup_retro68.sh ----

# make_setup_root DIR: make_root plus the Retro68 sources, SDK archives, an
# earlier work tree and the host commands setup_retro68.sh runs before it
# builds. The stub build-toolchain.bash records its arguments and whether the
# install directory still exists, then stops setup with status 42.
make_setup_root() {
    local root="$1" tool
    make_root "$root"
    mkdir -p "$root/tools/Retro68-src" "$root/tools/Retro68-work"
    echo 'find_package(Boost COMPONENTS system filesystem)' > "$root/tools/Retro68-src/CMakeLists.txt"
    echo "archive" > "$root/tools/MPW_fully_updated.sit"
    echo "archive" > "$root/tools/OpenGL_SDK_1.2.sit"
    cat > "$root/tools/Retro68-src/build-toolchain.bash" <<EOF
prefix=""
for arg in "\$@"; do case "\$arg" in --prefix=*) prefix="\${arg#--prefix=}";; esac; done
{ echo "args: \$*"; if [ -e "\$prefix" ]; then echo "prefix kept"; else echo "prefix removed"; fi; } > "$root/build-toolchain.called"
exit 42
EOF
    for tool in git bison flex makeinfo ruby sleep; do
        printf '#!/bin/sh\nexit 0\n' > "$root/stubs/$tool"
        chmod +x "$root/stubs/$tool"
    done
    cat > "$root/stubs/unar" <<'EOF'
#!/bin/sh
out=""; prev=""; for arg in "$@"; do [ "$prev" = "-o" ] && out="$arg"; prev="$arg"; done
case "$out" in
    *mpw*) mkdir -p "$out/MPW/Interfaces&Libraries/Interfaces/CIncludes"
           : > "$out/MPW/Interfaces&Libraries/Interfaces/CIncludes/Types.h" ;;
    *) mkdir -p "$out/OpenGL SDK/Libraries" "$out/OpenGL SDK/Headers"
       : > "$out/OpenGL SDK/Libraries/OpenGLLibraryStub"
       : > "$out/OpenGL SDK/Headers/gl.h" ;;
esac
EOF
    chmod +x "$root/stubs/unar"
}

run_setup() {
    local root="$1"
    Q3_STATUS=0
    (cd "$root" && TMPDIR="$Q3_CHECK_TMP" PATH="$root/stubs:$PATH" \
        bash ./setup_retro68.sh) > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

make_setup_root "$ROOT"
break_cc1 "$ROOT/tools/Retro68-build"
loader_error "$ROOT/tools/Retro68-build/bin/powerpc-apple-macos-ar" libfl.so.2
run_setup "$ROOT"
expect "setup_retro68.sh rebuilds a toolchain that cannot run" 42 \
    "libisl.so.23" "rebuilding it all" "Cleaning previous builds"
check "setup_retro68.sh does a full build (no --skip-thirdparty)" \
    bash -c 'grep -q "^args: --prefix=" "$1" && ! grep -q -e "--skip-thirdparty" "$1"' _ "$ROOT/build-toolchain.called"
check "setup_retro68.sh removed the toolchain that cannot run" \
    grep -q -x "prefix removed" "$ROOT/build-toolchain.called"

make_setup_root "$ROOT"
write_stub "$ROOT/tools/Retro68-build/bin/powerpc-apple-macos-gcc" \
    'case " $* " in *" --version "*) echo "powerpc-apple-macos-gcc (GCC) 12.2.0"; exit 0;; esac' \
    "$Q3_OUTPUT_ARG" \
    'case " $* " in *" -c "*) echo stub > "$out"; exit 0;; esac' \
    'echo "ld: cannot find -lretrocrt" >&2' 'exit 1'
run_setup "$ROOT"
expect "setup_retro68.sh resumes when the host tools run" 42 \
    "resuming with --skip-thirdparty"
check "setup_retro68.sh passes --skip-thirdparty and keeps the toolchain" \
    bash -c 'grep -q -e "--skip-thirdparty" "$1" && grep -q -x "prefix kept" "$1"' _ "$ROOT/build-toolchain.called"

check "no scratch files are left behind" \
    bash -c '[ -z "$(ls -A "$1")" ]' _ "$Q3_CHECK_TMP"

# ---- check_retro68.ps1 ----

run_ps_check() {
    Q3_STATUS=0
    TMPDIR="$Q3_CHECK_TMP" pwsh -NoProfile -NonInteractive -File "$Q3_TEST_ROOT/check_retro68.ps1" \
        "$@" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

if command -v pwsh > /dev/null 2>&1; then
    Q3_STATUS=0
    Q3_PS_FILES="$Q3_TEST_ROOT/check_retro68.ps1:$Q3_TEST_ROOT/build_mac.ps1:$Q3_TEST_ROOT/setup_retro68.ps1" \
    pwsh -NoProfile -NonInteractive -Command '
        $failed = $false
        foreach ($path in ($env:Q3_PS_FILES -split ":")) {
            $tokens = $null; $errors = $null
            [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) > $null
            if ($errors.Count -ne 0) { $errors | ForEach-Object { Write-Host "${path}: $_" }; $failed = $true }
        }
        if ($failed) { exit 1 }' > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "PowerShell scripts parse" 0

    fresh_toolchain
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 passes a working toolchain" 0 "Retro68 toolchain check passed: $TC"
    check "check_retro68.ps1 links, converts and runs Rez" \
        bash -c 'grep -q -e "-lInterfaceLib -o .*check\.xcoff$" "$1" && grep -q "^Rez .*--data .*check\.pef" "$1"' _ "$TC/calls.log"

    fresh_toolchain
    break_cc1 "$TC"
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 fails a cc1 that cannot load libisl" 1 \
        "compiling a C file (powerpc-apple-macos-gcc -c) failed" "libisl.so.23" \
        "docs/building-mac-os9.md"

    fresh_toolchain
    rm "$TC/bin/MakePEF"
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 fails a missing MakePEF" 1 "MakePEF is missing"

    fresh_toolchain
    loader_error "$TC/bin/MakePEF" libboost_filesystem.so.1.91.0
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1 fails a MakePEF that cannot load Boost" 1 \
        "MakePEF could not start (exit status 127)" "libboost_filesystem.so.1.91.0"

    fresh_toolchain
    write_stub "$TC/bin/MakePEF" \
        '[ "$#" -eq 0 ] && exit 1' "$Q3_OUTPUT_ARG" 'echo "joy!peffpwpc" > "$out"'
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 fails output without a PEF header" 1 "MakePEF did not write a PowerPC PEF"
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1 -ToolsOnly does not convert" 0 "Retro68 tools run"

    check "check_retro68.ps1 leaves no scratch files" \
        bash -c '[ -z "$(ls -A "$1")" ]' _ "$Q3_CHECK_TMP"
else
    echo "SKIP: pwsh is not installed; check_retro68.ps1 not exercised"
fi

# ---- a real toolchain ----

Q3_REAL="${Q3_RETRO68_DIR:-$Q3_TEST_ROOT/tools/Retro68-build}"
if [ -x "$Q3_REAL/bin/powerpc-apple-macos-gcc" ]; then
    run_check "$Q3_REAL"
    expect "the Retro68 toolchain at $Q3_REAL passes the full check" 0 "Retro68 toolchain check passed"
else
    echo "SKIP: no Retro68 toolchain at $Q3_REAL"
fi

if [ "$Q3_FAILED" -ne 0 ]; then
    echo "Retro68 readiness tests FAILED"
    exit 1
fi
echo "Retro68 readiness tests passed"
