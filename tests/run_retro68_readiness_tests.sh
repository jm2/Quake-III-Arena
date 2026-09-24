#!/usr/bin/env bash
# Issue #269: the Retro68 readiness checks must run the toolchain, not only
# find its files. After a host OS upgrade `powerpc-apple-macos-gcc --version`
# still worked while cc1 (libisl), ar (libfl), Rez and MakeImport (Boost)
# could not load, build_mac.sh declared the toolchain ready, and
# setup_retro68.sh resumed with --skip-thirdparty, which never rebuilds them.
#
# The toolchain here is made of shell stubs in a scratch directory, so no
# Retro68 install or $CC is needed and every CI job runs the same checks. The
# runner checks check_retro68.sh with working and broken stubs and with
# unusable scratch directories, then runs build_mac.sh and setup_retro68.sh
# in scratch copies of the repository with stub cmake, setup and
# build-toolchain.bash commands:
#   - a broken toolchain stops build_mac.sh before CMake with the missing
#     library named, and makes setup_retro68.sh move the toolchain and work
#     tree aside (never delete them) and rebuild everything;
#   - when the check itself cannot run (missing, read-only or full TMPDIR,
#     missing check script), both stop and setup changes nothing.
# When pwsh is installed the same cases go through check_retro68.ps1 and
# setup_retro68.ps1. A real toolchain ($Q3_RETRO68_DIR, default
# tools/Retro68-build) must pass the full check; one that does not is reported
# as SKIP (a local toolchain problem) unless Q3_RETRO68_REQUIRE=1.
set -uo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
export LC_ALL=C
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-retro68-readiness.XXXXXX")"
trap 'chmod -R u+w "$Q3_TEST_WORK" 2> /dev/null; rm -rf -- "$Q3_TEST_WORK"' EXIT

Q3_CHECK_TMP="$Q3_TEST_WORK/tmp"
Q3_OUT="$Q3_TEST_WORK/output.log"
Q3_STATUS=0
Q3_FAILED=0
mkdir -p "$Q3_CHECK_TMP"

# Scratch directories the check cannot use.
Q3_TMP_MISSING="$Q3_TEST_WORK/no-such-dir"
Q3_TMP_FILE="$Q3_TEST_WORK/tmp-is-a-file"
Q3_TMP_READONLY="$Q3_TEST_WORK/tmp-read-only"
: > "$Q3_TMP_FILE"
mkdir -p "$Q3_TMP_READONLY"
chmod a-w "$Q3_TMP_READONLY"
# root writes to read-only directories anyway.
Q3_READONLY_WORKS=0
touch "$Q3_TMP_READONLY/probe" 2> /dev/null || Q3_READONLY_WORKS=1
rm -f "$Q3_TMP_READONLY/probe" 2> /dev/null
# A dd that fails like a write to a full disk.
Q3_FULL_STUBS="$Q3_TEST_WORK/full-disk-stubs"
mkdir -p "$Q3_FULL_STUBS"
printf '#!/bin/sh\necho "dd: error writing: No space left on device" >&2\nexit 1\n' > "$Q3_FULL_STUBS/dd"
chmod +x "$Q3_FULL_STUBS/dd"
# A real full tmpfs needs an unprivileged user and mount namespace.
Q3_HAVE_UNSHARE=0
if unshare -r -m true > /dev/null 2>&1; then
    Q3_HAVE_UNSHARE=1
fi

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

# run_check [ARG...]: check_retro68.sh with $Q3_CHECK_ENV (VAR=value words).
Q3_CHECK_ENV=()
run_check() {
    Q3_STATUS=0
    env TMPDIR="$Q3_CHECK_TMP" "${Q3_CHECK_ENV[@]}" \
        bash "$Q3_TEST_ROOT/check_retro68.sh" "$@" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
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
Q3_VERSION_LINE='case " $* " in *" --version "*) echo "powerpc-apple-macos-gcc (GCC) 12.2.0"; exit 0;; esac'

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
        write_stub "$dir/bin/powerpc-apple-macos-$driver" "$Q3_VERSION_LINE" \
            "$Q3_OUTPUT_ARG" '[ -n "$out" ] || exit 1' 'echo stub > "$out"'
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
    write_stub "$1/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" \
        "echo \"\${0%/*}/../libexec/gcc/powerpc-apple-macos/12.2.0/cc1: error while loading shared libraries: libisl.so.23: cannot open shared object file: No such file or directory\" >&2" \
        'exit 1'
}

# A driver that compiles but cannot link: the target libraries are missing.
break_link() {
    write_stub "$1/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" \
        "$Q3_OUTPUT_ARG" \
        'case " $* " in *" -c "*) echo stub > "$out"; exit 0;; esac' \
        'echo "ld: cannot find -lretrocrt" >&2' 'exit 1'
}

# A MakePEF that writes a header other than the given 12 bytes.
pef_header() {
    write_stub "$1/bin/MakePEF" \
        '[ "$#" -eq 0 ] && exit 1' "$Q3_OUTPUT_ARG" "printf '%s' '$2' > \"\$out\""
}

# A fingerprint of every file under DIR: path, size, mode and mtime.
snapshot() {
    (cd "$1" && find . -printf '%p %s %m %T@\n' | sort) | cksum
}

TC="$Q3_TEST_WORK/toolchain"
fresh_toolchain() {
    rm -rf "$TC"
    make_toolchain "$TC"
}

# ---- compile flags ----

# Prints "C <flag>" and "CXX <flag>" for every compile flag CMakeLists.txt and
# the toolchain file add. Fails on any other flag setting, so a new form
# cannot slip past the comparison below.
cmake_compile_flags() {
    local file line code languages flags language flag status=0
    local re_set='^[[:space:]]*set\(CMAKE_(C|CXX)_FLAGS "\$\{CMAKE_(C|CXX)_FLAGS\} ([^"$]*)"\)[[:space:]]*$'
    local re_append='^[[:space:]]*string\(APPEND CMAKE_(C|CXX)_FLAGS " ([^"$]*)"\)[[:space:]]*$'
    local re_options='^[[:space:]]*add_compile_options\(([^")$<>]*)\)[[:space:]]*$'
    local re_clear='^[[:space:]]*set\(CMAKE_(C|CXX)_FLAGS_[A-Z]+ ""\)[[:space:]]*$'
    for file in "$Q3_TEST_ROOT/CMakeLists.txt" "$Q3_TEST_ROOT/cmake/Retro68.toolchain.cmake"; do
        while IFS= read -r line || [ -n "$line" ]; do
            code="${line%%#*}"
            case "$code" in
                *CMAKE_C_FLAGS*|*CMAKE_CXX_FLAGS*|*ompile_options*|*OMPILE_OPTIONS*|*COMPILE_FLAGS*) ;;
                *) continue ;;
            esac
            if [[ $code =~ $re_set ]] && [ "${BASH_REMATCH[1]}" = "${BASH_REMATCH[2]}" ]; then
                languages="${BASH_REMATCH[1]}"
                flags="${BASH_REMATCH[3]}"
            elif [[ $code =~ $re_append ]]; then
                languages="${BASH_REMATCH[1]}"
                flags="${BASH_REMATCH[2]}"
            elif [[ $code =~ $re_options ]]; then
                languages="C CXX"
                flags="${BASH_REMATCH[1]}"
            elif [[ $code =~ $re_clear ]]; then
                continue
            else
                echo "  ${file#$Q3_TEST_ROOT/} sets compile flags in a form this test does not parse: $line" >&2
                status=1
                continue
            fi
            for language in $languages; do
                for flag in $flags; do
                    echo "$language $flag"
                done
            done
        done < "$file"
    done
    return "$status"
}

# compiles_with_build_flags LOG: the C and C++ compiles recorded in LOG use
# every flag cmake_compile_flags prints, and the C one the toolchain file's
# definitions.
compiles_with_build_flags() {
    local log="$1" flags c_line cxx_line language flag line
    flags="$(cmake_compile_flags)" || return 1
    [ -n "$flags" ] || { echo "  no compile flags found in CMakeLists.txt"; return 1; }
    c_line="$(grep -m 1 -E '^powerpc-apple-macos-gcc .* -c ' "$log")"
    cxx_line="$(grep -m 1 -E '^powerpc-apple-macos-g\+\+ .* -c ' "$log")"
    while read -r language flag; do
        if [ "$language" = C ]; then line="$c_line"; else line="$cxx_line"; fi
        case " $line " in
            *" $flag "*) ;;
            *) echo "  $language compile lacks $flag: $line"; return 1 ;;
        esac
    done <<< "$flags
C -D__MACOS__
C -D__POWERPC__"
}

# The flag lists of check_retro68.sh and check_retro68.ps1 agree.
flag_lists_agree() {
    local sh_c sh_cxx ps_c ps_cxx
    sh_c="$(sed -n 's/^COMPILE_FLAGS="\(.*\)"$/\1/p' "$Q3_TEST_ROOT/check_retro68.sh")"
    sh_cxx="$(sed -n 's/^CXX_FLAGS="\(.*\)"$/\1/p' "$Q3_TEST_ROOT/check_retro68.sh")"
    ps_c="$(awk '/^\$CompileFlags = @\(/, /\)$/' "$Q3_TEST_ROOT/check_retro68.ps1" |
        grep -o '"[^"]*"' | tr -d '"' | tr '\n' ' ' | sed 's/ $//')"
    ps_cxx="$(awk '/^\$CxxFlags = @\(/, /\)$/' "$Q3_TEST_ROOT/check_retro68.ps1" |
        grep -o '"[^"]*"' | tr -d '"' | tr '\n' ' ' | sed 's/ $//')"
    [ -n "$sh_c" ] && [ "$sh_c" = "$ps_c" ] && [ -n "$sh_cxx" ] && [ "$sh_cxx" = "$ps_cxx" ] || {
        echo "  check_retro68.sh: C [$sh_c] C++ [$sh_cxx]"
        echo "  check_retro68.ps1: C [$ps_c] C++ [$ps_cxx]"
        return 1
    }
}

compile_flags_parse() {
    cmake_compile_flags > /dev/null
}
check "CMakeLists.txt compile flags are all in a form the test parses" compile_flags_parse
check "check_retro68.sh and check_retro68.ps1 use the same flags" flag_lists_agree

# ---- check_retro68.sh ----

fresh_toolchain
run_check "$TC"
expect "working toolchain passes the full check" 0 "Retro68 toolchain check passed: $TC"
check "full check compiles with the CMakeLists.txt flags" compiles_with_build_flags "$TC/calls.log"
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
pef_header "$TC" "not a PEF!!!"
run_check "$TC"
expect "MakePEF output without a PEF header fails" 1 "MakePEF did not write a PowerPC PEF"

fresh_toolchain
pef_header "$TC" "Joy!peffm68k"
run_check "$TC"
expect "MakePEF output for another architecture fails" 1 "MakePEF did not write a PowerPC PEF"

fresh_toolchain
write_stub "$TC/bin/Rez" '[ "$1" = --help ] && exit 0' 'exit 0'
run_check "$TC"
expect "Rez without output fails" 1 "Rez did not write an application."

# Probes run with no input: a tool that reads stdin must not wait on the
# caller's. The FIFO is held open read-write, so reading it blocks forever.
Q3_FIFO="$Q3_TEST_WORK/stdin.fifo"
mkfifo "$Q3_FIFO"
# stdin_reader_probe DIR: a MakePEF whose usage path reads all of stdin.
stdin_reader_probe() {
    write_stub "$1/bin/MakePEF" \
        '[ "$#" -eq 0 ] && { cat > /dev/null; echo "makepef: no input file specified." >&2; exit 1; }' \
        "$Q3_OUTPUT_ARG" 'printf "Joy!peffpwpc\000\000\000\001" > "$out"'
}
fresh_toolchain
stdin_reader_probe "$TC"
Q3_STATUS=0
exec 7<> "$Q3_FIFO"
TMPDIR="$Q3_CHECK_TMP" timeout 20 bash "$Q3_TEST_ROOT/check_retro68.sh" --tools-only "$TC" \
    < /dev/fd/7 > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
exec 7<&-
expect "probes do not read the caller's stdin" 0 "Retro68 tools run"

# A toolchain whose target libraries are missing: a resumed setup rebuilds
# them, so --tools-only must still pass while the full check fails.
fresh_toolchain
break_link "$TC"
run_check "$TC"
expect "missing target libraries fail the full check" 1 \
    "linking an XCOFF image (powerpc-apple-macos-gcc) failed" "cannot find -lretrocrt"
run_check --tools-only "$TC"
expect "missing target libraries pass --tools-only" 0 "Retro68 tools run"

# The check itself cannot run: status 3, never 1, and no tool is judged.
fresh_toolchain
run_check_in_tmp() {
    local tmp="$1"
    shift
    Q3_STATUS=0
    env TMPDIR="$tmp" "${Q3_CHECK_ENV[@]}" \
        bash "$Q3_TEST_ROOT/check_retro68.sh" "$@" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}
run_check_in_tmp "$Q3_TMP_MISSING" "$TC"
expect "a missing TMPDIR is status 3" 3 "could not check the Retro68 toolchain" \
    "could not create a scratch directory under $Q3_TMP_MISSING."
run_check_in_tmp "$Q3_TMP_FILE" --tools-only "$TC"
expect "a TMPDIR that is a file is status 3" 3 "could not create a scratch directory"
if [ "$Q3_READONLY_WORKS" -eq 1 ]; then
    run_check_in_tmp "$Q3_TMP_READONLY" --tools-only "$TC"
    expect "a read-only TMPDIR is status 3" 3 "could not create a scratch directory"
else
    echo "SKIP: read-only TMPDIR (running as root, which writes anyway)"
fi
Q3_CHECK_ENV=(PATH="$Q3_FULL_STUBS:$PATH")
run_check --tools-only "$TC"
expect "a TMPDIR without room for 1 MiB is status 3" 3 "does not take 1 MiB"
Q3_CHECK_ENV=()
check "no tool ran while the check could not run" test ! -s "$TC/calls.log"

fresh_toolchain
write_stub "$TC/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" \
    'echo "cc1: error writing to /x/check.s: No space left on device" >&2' 'exit 1'
run_check --tools-only "$TC"
expect "a compile that runs out of space is status 3" 3 \
    "failed for lack of space in the scratch directory" "No space left on device"

if [ "$Q3_HAVE_UNSHARE" -eq 1 ]; then
    fresh_toolchain
    # A 64 KiB tmpfs: full before the check starts.
    Q3_STATUS=0
    unshare -r -m bash -c 'mount -t tmpfs -o size=64k tmpfs "$1" &&
        TMPDIR="$1" bash "$2/check_retro68.sh" --tools-only "$3"' _ \
        "$Q3_CHECK_TMP" "$Q3_TEST_ROOT" "$TC" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "a full tmpfs TMPDIR is status 3" 3 "does not take 1 MiB"
    # A 1200 KiB tmpfs that a compile fills without saying why it failed.
    write_stub "$TC/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" "$Q3_OUTPUT_ARG" \
        'dd if=/dev/zero of="$out" bs=1024 count=2048 2> /dev/null' 'exit 1'
    Q3_STATUS=0
    unshare -r -m bash -c 'mount -t tmpfs -o size=1200k tmpfs "$1" &&
        TMPDIR="$1" bash "$2/check_retro68.sh" --tools-only "$3"' _ \
        "$Q3_CHECK_TMP" "$Q3_TEST_ROOT" "$TC" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "a tmpfs filled during a compile is status 3" 3 "no longer takes writes"
else
    echo "SKIP: tmpfs TMPDIR cases (unshare -r -m is not available)"
fi

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
        "$Q3_TEST_ROOT/check_retro68.sh" "$Q3_TEST_ROOT/setup_retro68.ps1" \
        "$Q3_TEST_ROOT/check_retro68.ps1" "$root/"
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

# run_build ROOT [TMPDIR]
run_build() {
    local root="$1" tmp="${2:-$Q3_CHECK_TMP}"
    Q3_STATUS=0
    (cd "$root" && TMPDIR="$tmp" PATH="$root/stubs:$PATH" \
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
run_build "$ROOT" "$Q3_TMP_MISSING"
expect "build_mac.sh stops when the check cannot run" 1 \
    "could not check the Retro68 toolchain" \
    "Stopping before CMake: check_retro68.sh could not check the Retro68" "(exit status 3)"
check "build_mac.sh never reaches CMake when the check cannot run" test ! -e "$ROOT/cmake.called"
check "build_mac.sh does not start setup when the check cannot run" test ! -e "$ROOT/setup.called"

make_root "$ROOT"
stub_setup "$ROOT"
chmod a-x "$ROOT/check_retro68.sh"
run_build "$ROOT"
expect "build_mac.sh configures with a working toolchain (check script not executable)" 3 \
    "Retro68 toolchain check passed"
check "build_mac.sh reached CMake" test -e "$ROOT/cmake.called"

# ---- setup_retro68.sh ----

# make_setup_root DIR: make_root plus the Retro68 sources, SDK archives, an
# earlier work tree and the host commands setup_retro68.sh runs before it
# builds. The stub build-toolchain.bash records its arguments and whether the
# install directory still exists, then stops setup with status 42.
make_setup_root() {
    local root="$1" tool
    make_root "$root"
    mkdir -p "$root/tools/Retro68-src" "$root/tools/Retro68-work/gcc-build-ppc"
    echo "object" > "$root/tools/Retro68-work/gcc-build-ppc/cc1.o"
    echo 'find_package(Boost COMPONENTS system filesystem)' > "$root/tools/Retro68-src/CMakeLists.txt"
    echo "archive" > "$root/tools/MPW_fully_updated.sit"
    echo "archive" > "$root/tools/OpenGL_SDK_1.2.sit"
    cat > "$root/tools/Retro68-src/build-toolchain.bash" <<EOF
#!/bin/bash
prefix=""
for arg in "\$@"; do case "\$arg" in --prefix=*) prefix="\${arg#--prefix=}";; esac; done
{ echo "args: \$*"; if [ -e "\$prefix" ]; then echo "prefix kept"; else echo "prefix removed"; fi; } > "$root/build-toolchain.called"
exit 42
EOF
    chmod +x "$root/tools/Retro68-src/build-toolchain.bash"
    for tool in git bison flex makeinfo ruby sleep; do
        printf '#!/bin/sh\nexit 0\n' > "$root/stubs/$tool"
        chmod +x "$root/stubs/$tool"
    done
    # PowerShell passes Windows-style relative paths such as tools\temp_mpw.
    cat > "$root/stubs/unar" <<'EOF'
#!/bin/sh
out=""; prev=""; for arg in "$@"; do [ "$prev" = "-o" ] && out="$arg"; prev="$arg"; done
out=$(printf '%s' "$out" | tr '\\' /)
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

# run_setup ROOT [TMPDIR]
run_setup() {
    local root="$1" tmp="${2:-$Q3_CHECK_TMP}"
    Q3_STATUS=0
    (cd "$root" && TMPDIR="$tmp" PATH="$root/stubs:$PATH" \
        bash ./setup_retro68.sh) > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

# The toolchain and work tree were moved to *.SUFFIX-<UTC time>, complete.
moved_aside() {
    local root="$1" suffix="$2" install work
    install="$(find "$root/tools" -maxdepth 1 -name "Retro68-build.$suffix-*" | head -n 1)"
    work="$(find "$root/tools" -maxdepth 1 -name "Retro68-work.$suffix-*" | head -n 1)"
    [ -n "$install" ] && [ -x "$install/bin/powerpc-apple-macos-gcc" ] &&
        [ -e "$install/bin/ConvertDiskImage" ] &&
        [ -n "$work" ] && [ -f "$work/gcc-build-ppc/cc1.o" ] &&
        [[ ${install##*.$suffix-} =~ ^[0-9]{8}T[0-9]{6}Z ]]
}

# Setup stopped before changing anything: no build, nothing moved, and the
# toolchain and work tree are byte for byte as before.
setup_changed_nothing() {
    local root="$1" before="$2"
    [ ! -e "$root/build-toolchain.called" ] &&
        [ -z "$(find "$root/tools" -maxdepth 1 -name 'Retro68-*.*-*')" ] &&
        [ "$(snapshot "$root/tools/Retro68-build"; snapshot "$root/tools/Retro68-work")" = "$before" ]
}

make_setup_root "$ROOT"
break_cc1 "$ROOT/tools/Retro68-build"
loader_error "$ROOT/tools/Retro68-build/bin/powerpc-apple-macos-ar" libfl.so.2
run_setup "$ROOT"
expect "setup_retro68.sh rebuilds a toolchain that cannot run" 42 \
    "libisl.so.23" "rebuilding it all" "Moving previous builds aside" "To restore it: rm -rf"
check "setup_retro68.sh does a full build (no --skip-thirdparty)" \
    bash -c 'grep -q "^args: --prefix=" "$1" && ! grep -q -e "--skip-thirdparty" "$1"' _ "$ROOT/build-toolchain.called"
check "setup_retro68.sh builds into an empty prefix" \
    grep -q -x "prefix removed" "$ROOT/build-toolchain.called"
check "setup_retro68.sh moved the toolchain and work tree aside as *.broken-<UTC time>" \
    moved_aside "$ROOT" broken

# An incomplete toolchain (no ConvertDiskImage) is also moved, never deleted.
make_setup_root "$ROOT"
rm "$ROOT/tools/Retro68-build/bin/ConvertDiskImage"
: > "$ROOT/tools/Retro68-build/bin/ConvertDiskImage"
run_setup "$ROOT"
expect "setup_retro68.sh rebuilds an incomplete toolchain" 42 "Moving previous builds aside"
check "setup_retro68.sh moved the incomplete toolchain aside as *.previous-<UTC time>" \
    moved_aside "$ROOT" previous

make_setup_root "$ROOT"
break_link "$ROOT/tools/Retro68-build"
run_setup "$ROOT"
expect "setup_retro68.sh resumes when the host tools run" 42 \
    "resuming with --skip-thirdparty"
check "setup_retro68.sh passes --skip-thirdparty and keeps the toolchain" \
    bash -c 'grep -q -e "--skip-thirdparty" "$1" && grep -q -x "prefix kept" "$1"' _ "$ROOT/build-toolchain.called"

make_setup_root "$ROOT"
chmod a-x "$ROOT/check_retro68.sh"
run_setup "$ROOT"
expect "setup_retro68.sh resumes with a check script that is not executable" 42 \
    "resuming with --skip-thirdparty"
check "setup_retro68.sh kept the toolchain (check script not executable)" \
    grep -q -x "prefix kept" "$ROOT/build-toolchain.called"

# setup_unchanged NAME [TMPDIR]: setup with an unusable check stops with the
# right message and leaves the working toolchain alone.
setup_unchanged() {
    local name="$1" tmp="${2:-$Q3_CHECK_TMP}" before
    before="$(snapshot "$ROOT/tools/Retro68-build"; snapshot "$ROOT/tools/Retro68-work")"
    run_setup "$ROOT" "$tmp"
    expect "setup_retro68.sh stops when $name" 1 \
        "could not check the existing toolchain" "Nothing was changed"
    check "setup_retro68.sh changed nothing when $name" setup_changed_nothing "$ROOT" "$before"
}

make_setup_root "$ROOT"
setup_unchanged "TMPDIR is missing" "$Q3_TMP_MISSING"
make_setup_root "$ROOT"
setup_unchanged "TMPDIR is a file" "$Q3_TMP_FILE"
if [ "$Q3_READONLY_WORKS" -eq 1 ]; then
    make_setup_root "$ROOT"
    setup_unchanged "TMPDIR is read-only" "$Q3_TMP_READONLY"
fi
make_setup_root "$ROOT"
cp "$Q3_FULL_STUBS/dd" "$ROOT/stubs/dd"
setup_unchanged "TMPDIR has no room"
make_setup_root "$ROOT"
rm "$ROOT/check_retro68.sh"
setup_unchanged "check_retro68.sh is missing"

if [ "$Q3_HAVE_UNSHARE" -eq 1 ]; then
    make_setup_root "$ROOT"
    Q3_BEFORE="$(snapshot "$ROOT/tools/Retro68-build"; snapshot "$ROOT/tools/Retro68-work")"
    Q3_STATUS=0
    unshare -r -m bash -c 'mount -t tmpfs -o size=64k tmpfs "$1" &&
        cd "$2" && TMPDIR="$1" PATH="$2/stubs:$PATH" bash ./setup_retro68.sh' _ \
        "$Q3_CHECK_TMP" "$ROOT" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "setup_retro68.sh stops when TMPDIR is a full tmpfs" 1 \
        "could not check the existing toolchain" "Nothing was changed"
    check "setup_retro68.sh changed nothing when TMPDIR is a full tmpfs" \
        setup_changed_nothing "$ROOT" "$Q3_BEFORE"
fi

check "no scratch files are left behind" \
    bash -c '[ -z "$(ls -A "$1")" ]' _ "$Q3_CHECK_TMP"

# ---- PowerShell ----

run_ps_check() {
    local tmp="$Q3_CHECK_TMP"
    if [ "$1" = --tmp ]; then
        tmp="$2"
        shift 2
    fi
    Q3_STATUS=0
    TMPDIR="$tmp" pwsh -NoProfile -NonInteractive -File "$Q3_TEST_ROOT/check_retro68.ps1" \
        "$@" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

# setup_retro68.ps1 looks for Windows tool names.
make_ps_setup_root() {
    local tool
    make_setup_root "$1"
    for tool in "$1"/tools/Retro68-build/bin/*; do
        cp -p "$tool" "$tool.exe"
    done
    printf '#!/bin/sh\nexit 0\n' > "$1/stubs/cmake"
}

run_ps_setup() {
    local root="$1" tmp="${2:-$Q3_CHECK_TMP}"
    Q3_STATUS=0
    (cd "$root" && TMPDIR="$tmp" PATH="$root/stubs:$PATH" \
        pwsh -NoProfile -NonInteractive -File ./setup_retro68.ps1) > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
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
    check "check_retro68.ps1 compiles with the CMakeLists.txt flags" compiles_with_build_flags "$TC/calls.log"
    check "check_retro68.ps1 links, converts and runs Rez" \
        bash -c 'grep -q -e "-lInterfaceLib -o .*check\.xcoff$" "$1" && grep -q "^Rez .*--data .*check\.pef" "$1"' _ "$TC/calls.log"

    fresh_toolchain
    stdin_reader_probe "$TC"
    Q3_STATUS=0
    exec 7<> "$Q3_FIFO"
    TMPDIR="$Q3_CHECK_TMP" timeout 30 pwsh -NoProfile -NonInteractive \
        -File "$Q3_TEST_ROOT/check_retro68.ps1" -ToolsOnly -InstallDir "$TC" \
        < /dev/fd/7 > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    exec 7<&-
    expect "check_retro68.ps1 probes do not read the caller's stdin" 0 "Retro68 tools run"

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
    pef_header "$TC" "joy!peffpwpc"
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 fails output without a PEF header" 1 "MakePEF did not write a PowerPC PEF"
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1 -ToolsOnly does not convert" 0 "Retro68 tools run"

    fresh_toolchain
    pef_header "$TC" "Joy!peffm68k"
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 fails a PEF for another architecture" 1 "MakePEF did not write a PowerPC PEF"

    fresh_toolchain
    write_stub "$TC/bin/Rez" '[ "$1" = --help ] && exit 0' 'exit 0'
    run_ps_check -InstallDir "$TC"
    expect "check_retro68.ps1 fails a Rez without output" 1 "Rez did not write an application."

    fresh_toolchain
    run_ps_check --tmp "$Q3_TMP_MISSING" -InstallDir "$TC"
    expect "check_retro68.ps1: a missing TMPDIR is status 3" 3 \
        "could not check the Retro68 toolchain" "could not create a scratch directory"
    if [ "$Q3_READONLY_WORKS" -eq 1 ]; then
        run_ps_check --tmp "$Q3_TMP_READONLY" -ToolsOnly -InstallDir "$TC"
        expect "check_retro68.ps1: a read-only TMPDIR is status 3" 3 "could not create a scratch directory"
    fi
    check "check_retro68.ps1 ran no tool while the check could not run" test ! -s "$TC/calls.log"
    write_stub "$TC/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" \
        'echo "cc1: error writing to /x/check.s: No space left on device" >&2' 'exit 1'
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1: a compile that runs out of space is status 3" 3 \
        "failed for lack of space in the scratch directory"
    if [ "$Q3_HAVE_UNSHARE" -eq 1 ]; then
        fresh_toolchain
        Q3_STATUS=0
        unshare -r -m bash -c 'mount -t tmpfs -o size=64k tmpfs "$1" &&
            TMPDIR="$1" pwsh -NoProfile -NonInteractive -File "$2/check_retro68.ps1" -ToolsOnly -InstallDir "$3"' _ \
            "$Q3_CHECK_TMP" "$Q3_TEST_ROOT" "$TC" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
        expect "check_retro68.ps1: a full tmpfs TMPDIR is status 3" 3 "does not take 1 MiB"
    fi

    check "check_retro68.ps1 leaves no scratch files" \
        bash -c '[ -z "$(ls -A "$1")" ]' _ "$Q3_CHECK_TMP"

    make_ps_setup_root "$ROOT"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 accepts a working toolchain" 0 \
        "Retro68 toolchain check passed" "Retro68 appears to be installed"
    check "setup_retro68.ps1 left the working toolchain in place" \
        test -x "$ROOT/tools/Retro68-build/bin/powerpc-apple-macos-gcc.exe"

    make_ps_setup_root "$ROOT"
    Q3_BEFORE="$(snapshot "$ROOT/tools/Retro68-build"; snapshot "$ROOT/tools/Retro68-work")"
    run_ps_setup "$ROOT" "$Q3_TMP_MISSING"
    expect "setup_retro68.ps1 stops when the check cannot run" 1 \
        "could not check the installed Retro68 toolchain (exit status 3)" "Nothing was changed"
    check "setup_retro68.ps1 changed nothing when the check cannot run" \
        setup_changed_nothing "$ROOT" "$Q3_BEFORE"

    make_ps_setup_root "$ROOT"
    break_cc1 "$ROOT/tools/Retro68-build"
    cp -p "$ROOT/tools/Retro68-build/bin/powerpc-apple-macos-gcc" \
        "$ROOT/tools/Retro68-build/bin/powerpc-apple-macos-gcc.exe"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 rebuilds a toolchain that cannot run" 1 \
        "libisl.so.23" "cannot run (see above); rebuilding it" \
        "Moved the toolchain that cannot run to" "Build failed."
    check "setup_retro68.ps1 builds into an empty prefix" \
        grep -q -x "prefix removed" "$ROOT/build-toolchain.called"
    check "setup_retro68.ps1 renamed the toolchain aside as *.broken-<UTC time>" \
        bash -c 'set -- "$1"/tools/Retro68-build.broken-*; [ -x "$1/bin/powerpc-apple-macos-gcc.exe" ]' _ "$ROOT"
else
    echo "SKIP: pwsh is not installed; check_retro68.ps1 and setup_retro68.ps1 not exercised"
fi

# ---- a real toolchain ----

Q3_REAL="${Q3_RETRO68_DIR:-$Q3_TEST_ROOT/tools/Retro68-build}"
if [ -x "$Q3_REAL/bin/powerpc-apple-macos-gcc" ]; then
    run_check "$Q3_REAL"
    if [ "$Q3_STATUS" -eq 0 ] || [ "${Q3_RETRO68_REQUIRE:-0}" = 1 ]; then
        expect "the Retro68 toolchain at $Q3_REAL passes the full check" 0 "Retro68 toolchain check passed"
    else
        echo "SKIP: the local Retro68 toolchain at $Q3_REAL does not pass check_retro68.sh"
        echo "      (exit status $Q3_STATUS). That is a problem with this host's toolchain, not"
        echo "      with this test; set Q3_RETRO68_REQUIRE=1 to make it fail. Its first lines:"
        sed -n '1,4p' "$Q3_OUT" | sed 's/^/        /'
    fi
elif [ "${Q3_RETRO68_REQUIRE:-0}" = 1 ]; then
    echo "FAIL: Q3_RETRO68_REQUIRE=1, but there is no Retro68 toolchain at $Q3_REAL"
    Q3_FAILED=1
else
    echo "SKIP: no Retro68 toolchain at $Q3_REAL"
fi

if [ "$Q3_FAILED" -ne 0 ]; then
    echo "Retro68 readiness tests FAILED"
    exit 1
fi
echo "Retro68 readiness tests passed"
