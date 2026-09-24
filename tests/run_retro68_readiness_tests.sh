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
#     tree aside (never delete a toolchain; keep one moved-aside work tree)
#     and rebuild everything;
#   - when the check itself cannot run (missing, read-only, full or
#     inode-starved TMPDIR, a tool killed from outside, a missing check
#     script), both stop and setup changes nothing;
#   - a BSD/macOS wc, which pads its count, does not break the check;
#   - a tool's error output decides the status even when the scratch
#     directory cannot keep it.
# Issue #227: the toolchain inputs are pinned in retro68-versions.txt. A stub
# git plays Retro68's history, whose upstream HEAD is not the pinned commit:
#   - a fresh setup (sh and ps1) checks out the pinned commit and submodules;
#     an existing checkout is never pulled, and one at another commit or with
#     other submodules stops setup before anything changes;
#   - a tampered or truncated SDK archive, local or downloaded, is rejected
#     before anything is extracted;
#   - cmake/Quake3BuildManifest.cmake writes the PEF's SHA-256, the compiler's
#     version and the Retro68 commit and submodules, deterministically, and the
#     build runs it after MakePEF.
# When pwsh is installed the same cases go through check_retro68.ps1 and
# setup_retro68.ps1. A real toolchain ($Q3_RETRO68_DIR, default
# tools/Retro68-build) must pass the full check, and the Retro68 source and
# SDK archives next to it must match retro68-versions.txt; one that does not
# is reported as SKIP (a local toolchain problem) unless Q3_RETRO68_REQUIRE=1.
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

# The pins in retro68-versions.txt (issue #227), and what upstream HEAD and its
# submodules are instead: a checkout that follows upstream ends up there.
Q3_VERSIONS="$Q3_TEST_ROOT/retro68-versions.txt"
q3_version() {
    sed -n "s/^$1=//p" "$Q3_VERSIONS"
}
Q3_PIN="$(q3_version RETRO68_COMMIT)"
Q3_PIN_URL="$(q3_version RETRO68_URL)"
Q3_PIN_SUBMODULES="$(q3_version RETRO68_SUBMODULE)"
Q3_MPW_FILE="$(q3_version MPW_FILE)"
Q3_OPENGL_FILE="$(q3_version OPENGL_FILE)"
Q3_UPSTREAM=1111111111111111111111111111111111111111
Q3_UPSTREAM_SUB=2222222222222222222222222222222222222222

q3_sha256() {
    if command -v sha256sum > /dev/null 2>&1; then
        sha256sum < "$1" | cut -d ' ' -f 1
    else
        shasum -a 256 < "$1" | cut -d ' ' -f 1
    fi
}

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
    "failed because of its surroundings" "No space left on device"

# More surroundings that are not the toolchain. None needs tmpfs, so CI runs
# them all.
# env_gcc LINE...: a gcc whose --version works and whose other calls run LINE...
env_gcc() {
    write_stub "$TC/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" "$Q3_OUTPUT_ARG" "$@"
}
# env_ar LINE...: an ar whose --version works and whose other calls run LINE...
env_ar() {
    write_stub "$TC/bin/powerpc-apple-macos-ar" '[ "$1" = --version ] && exit 0' "$@"
}
# A padding wc, as on BSD and macOS ("%8s").
Q3_PAD_STUBS="$Q3_TEST_WORK/padding-wc"
mkdir -p "$Q3_PAD_STUBS"
printf '#!/bin/sh\nprintf "%%8s\\n" "$(%s "$@" | tr -d " ")"\n' "$(command -v wc)" > "$Q3_PAD_STUBS/wc"
chmod +x "$Q3_PAD_STUBS/wc"

fresh_toolchain
env_gcc 'kill -KILL $$'
run_check --tools-only "$TC"
expect "a compile killed by SIGKILL (the OOM killer) is status 3" 3 \
    "was stopped by signal 9 from outside (exit status 137)"

fresh_toolchain
env_gcc 'echo "powerpc-apple-macos-gcc: fatal error: Killed signal terminated program cc1" >&2' 'exit 1'
run_check --tools-only "$TC"
expect "cc1 killed from outside is status 3" 3 \
    "failed because of its surroundings" "Killed signal terminated program cc1"

fresh_toolchain
env_gcc 'echo "powerpc-apple-macos-gcc: internal compiler error: Segmentation fault signal terminated program cc1" >&2' 'exit 4'
run_check --tools-only "$TC"
expect "cc1 that crashes is status 1" 1 "Segmentation fault signal terminated program cc1"

fresh_toolchain
write_stub "$TC/bin/MakePEF" 'kill -TERM $$'
run_check --tools-only "$TC"
expect "a probe stopped by SIGTERM is status 3" 3 "(exit status 143)"

fresh_toolchain
env_gcc '[ -n "$out" ] && chmod a-w "$(dirname "$out")"' 'exit 1'
run_check --tools-only "$TC"
expect "a compile that leaves the scratch directory read-only is status 3" 3 \
    "no longer takes writes"

fresh_toolchain
env_ar 'echo "powerpc-apple-macos-ar: could not create temporary file whilst writing archive: no more archived files" >&2' 'exit 1'
run_check --tools-only "$TC"
expect "an ar out of temporary files is status 3" 3 \
    "could not create temporary file whilst writing archive"

# A tool's error output must decide the status even when a file in the
# scratch directory could not keep it, as when that directory is full: this
# compile reports ENOSPC, then empties whatever file its stderr went to.
fresh_toolchain
env_gcc 'echo "cc1: error writing to /x/check.s: No space left on device" >&2' \
    '[ -f /proc/self/fd/2 ] && : > /proc/self/fd/2' 'exit 1'
run_check --tools-only "$TC"
expect "a space error that no scratch file kept is status 3" 3 \
    "failed because of its surroundings" "No space left on device"

fresh_toolchain
Q3_STATUS=0
(umask 222; exec env TMPDIR="$Q3_CHECK_TMP" bash "$Q3_TEST_ROOT/check_retro68.sh" --tools-only "$TC") \
    > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
expect "a scratch directory created read-only (umask 222) is status 3" 3 "does not take 1 MiB"
check "no tool ran with a read-only scratch directory" test ! -s "$TC/calls.log"

check "the padding wc stub pads like BSD wc" \
    bash -c '[ "$(printf abc | PATH="$1:$PATH" wc -c)" = "       3" ]' _ "$Q3_PAD_STUBS"
fresh_toolchain
Q3_CHECK_ENV=(PATH="$Q3_PAD_STUBS:$PATH")
run_check "$TC"
expect "a padding (BSD/macOS) wc passes the full check" 0 "Retro68 toolchain check passed"
Q3_CHECK_ENV=()

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
    # A compile that fills the tmpfs, reports ENOSPC and removes its temporary
    # file again: the scratch directory takes writes afterwards, and the
    # message only survives if the check did not write it there.
    write_stub "$TC/bin/powerpc-apple-macos-gcc" "$Q3_VERSION_LINE" "$Q3_OUTPUT_ARG" \
        'fill="$(dirname "$out")/fill.tmp"' \
        'dd if=/dev/zero of="$fill" bs=1024 count=2048 2> /dev/null' \
        'echo "cc1: error writing to check.s: No space left on device" >&2' \
        'rm -f "$fill"' 'exit 1'
    Q3_STATUS=0
    unshare -r -m bash -c 'mount -t tmpfs -o size=1200k tmpfs "$1" &&
        TMPDIR="$1" bash "$2/check_retro68.sh" --tools-only "$3"' _ \
        "$Q3_CHECK_TMP" "$Q3_TEST_ROOT" "$TC" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "a compile that briefly fills the tmpfs is status 3" 3 \
        "failed because of its surroundings" "No space left on device"
    # A tmpfs with room for data but only a few inodes.
    Q3_STATUS=0
    unshare -r -m bash -c 'mount -t tmpfs -o size=4m,nr_inodes=9 tmpfs "$1" &&
        TMPDIR="$1" bash "$2/check_retro68.sh" --tools-only "$3"' _ \
        "$Q3_CHECK_TMP" "$Q3_TEST_ROOT" "$TC" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "a tmpfs out of inodes is status 3" 3 "does not take 1 MiB and 16 files"
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
        "$Q3_TEST_ROOT/check_retro68.ps1" "$Q3_VERSIONS" "$root/"
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

# write_git_stub ROOT: a git that logs its arguments to ROOT/git.log and plays
# Retro68's history, whose upstream HEAD ($Q3_UPSTREAM) is not the pinned
# commit. A clone copies ROOT/upstream at upstream HEAD, and pull moves a
# checkout there. The pinned commit records the submodule commits
# retro68-versions.txt lists, any other commit $Q3_UPSTREAM_SUB for each. A
# checkout keeps its commit in .git/stub-head and its submodules' in
# .git/stub-sub.
write_git_stub() {
    local root="$1"
    mkdir -p "$root/stubs"
    printf '%s\n' "$Q3_PIN_SUBMODULES" > "$root/pinned-gitlinks"
    {
        printf '#!/bin/bash\nroot=%q pin=%q upstream=%q upstream_sub=%q\n' \
            "$root" "$Q3_PIN" "$Q3_UPSTREAM" "$Q3_UPSTREAM_SUB"
        cat <<'EOF'
echo "git $*" >> "$root/git.log"
dir=.
while :; do
    case "$1" in
        -C) dir="$2"; shift 2 ;;
        -c) shift 2 ;;
        *) break ;;
    esac
done
# "<path> <commit>" for each submodule the checked-out commit records.
gitlinks() {
    if [ "$(cat "$dir/.git/stub-head")" = "$pin" ]; then
        cat "$root/pinned-gitlinks"
    else
        sed "s/ .*/ $upstream_sub/" "$root/pinned-gitlinks"
    fi
}
case "$1" in
    --version) echo "git version 2.99.0" ;;
    clone)
        for target; do :; done
        mkdir -p "$target/.git" && cp -R "$root/upstream/." "$target/" || exit 1
        echo "$upstream" > "$target/.git/stub-head"
        case " $* " in *" --recursive "*) dir="$target"; gitlinks > "$target/.git/stub-sub" ;; esac
        ;;
    checkout) for commit; do :; done; echo "$commit" > "$dir/.git/stub-head" ;;
    pull) echo "$upstream" > "$dir/.git/stub-head" ;;
    rev-parse) cat "$dir/.git/stub-head" ;;
    submodule)
        case "$2" in
            update) gitlinks > "$dir/.git/stub-sub" ;;
            status)
                gitlinks | while read -r path link; do
                    sub=$(sed -n "s|^$path ||p" "$dir/.git/stub-sub" 2> /dev/null)
                    if [ -z "$sub" ]; then echo "-$link $path"
                    elif [ "$sub" = "$link" ]; then echo " $sub $path (heads/master)"
                    else echo "+$sub $path (heads/master)"; fi
                done
                ;;
        esac
        ;;
esac
EOF
    } > "$root/stubs/git"
    chmod +x "$root/stubs/git"
}

# set_version ROOT KEY VALUE: KEY=VALUE in ROOT's retro68-versions.txt.
set_version() {
    sed "s|^$2=.*|$2=$3|" "$1/retro68-versions.txt" > "$1/retro68-versions.new" &&
        mv "$1/retro68-versions.new" "$1/retro68-versions.txt"
}

# make_setup_root DIR: make_root plus the Retro68 sources at the pinned
# commit, SDK archives pinned by their digests, an earlier work tree and the
# host commands setup_retro68.sh runs before it builds. The stub
# build-toolchain.bash records its arguments and whether the install
# directory still exists, then stops setup with status 42.
make_setup_root() {
    local root="$1" tool
    make_root "$root"
    mkdir -p "$root/upstream" "$root/tools/Retro68-work/gcc-build-ppc"
    echo "object" > "$root/tools/Retro68-work/gcc-build-ppc/cc1.o"
    echo 'find_package(Boost COMPONENTS system filesystem)' > "$root/upstream/CMakeLists.txt"
    echo "mpw archive" > "$root/tools/$Q3_MPW_FILE"
    echo "opengl archive" > "$root/tools/$Q3_OPENGL_FILE"
    set_version "$root" MPW_SHA256 "$(q3_sha256 "$root/tools/$Q3_MPW_FILE")"
    set_version "$root" OPENGL_SHA256 "$(q3_sha256 "$root/tools/$Q3_OPENGL_FILE")"
    cat > "$root/upstream/build-toolchain.bash" <<EOF
#!/bin/bash
prefix=""
for arg in "\$@"; do case "\$arg" in --prefix=*) prefix="\${arg#--prefix=}";; esac; done
{ echo "args: \$*"; if [ -e "\$prefix" ]; then echo "prefix kept"; else echo "prefix removed"; fi; } > "$root/build-toolchain.called"
exit 42
EOF
    chmod +x "$root/upstream/build-toolchain.bash"
    for tool in bison flex makeinfo ruby sleep; do
        printf '#!/bin/sh\nexit 0\n' > "$root/stubs/$tool"
        chmod +x "$root/stubs/$tool"
    done
    write_git_stub "$root"
    # Retro68 checked out at the pinned commit and submodules.
    mkdir -p "$root/tools/Retro68-src/.git"
    cp -R "$root/upstream/." "$root/tools/Retro68-src/"
    echo "$Q3_PIN" > "$root/tools/Retro68-src/.git/stub-head"
    cp "$root/pinned-gitlinks" "$root/tools/Retro68-src/.git/stub-sub"
    # PowerShell passes Windows-style relative paths such as tools\temp_mpw.
    {
        printf '#!/bin/sh\necho "unar $*" >> %q\n' "$root/unar.called"
        cat <<'EOF'
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
    } > "$root/stubs/unar"
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

# A second failed rebuild (build_mac.sh reruns setup after one) leaves a
# partial prefix and a new work tree. Every moved-aside toolchain is kept, but
# only the newest moved-aside work tree.
mkdir -p "$ROOT/tools/Retro68-build/bin"
echo partial > "$ROOT/tools/Retro68-build/marker"
echo second > "$ROOT/tools/Retro68-work/marker"
run_setup "$ROOT"
expect "setup_retro68.sh replaces an older moved-aside work tree" 42 \
    "Removing the older moved-aside work tree" "one moved-aside work tree is kept"
check "setup_retro68.sh keeps one moved-aside work tree, the newest" \
    bash -c 'set -- "$1"/tools/Retro68-work.*-*; [ "$#" -eq 1 ] && [ "$(cat "$1/marker")" = second ]' _ "$ROOT"
check "setup_retro68.sh keeps both moved-aside toolchains" \
    bash -c 'root="$1"; set -- "$root"/tools/Retro68-build.broken-*
        [ "$#" -eq 1 ] && [ -x "$1/bin/powerpc-apple-macos-gcc" ] || exit 1
        set -- "$root"/tools/Retro68-build.previous-*
        [ "$#" -eq 1 ] && [ "$(cat "$1/marker")" = partial ]' _ "$ROOT"

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

# ---- pinned toolchain inputs (issue #227) ----

versions_well_formed() {
    local line key
    [[ $Q3_PIN =~ ^[0-9a-f]{40}$ ]] && [ -n "$Q3_PIN_URL" ] && [ -n "$Q3_PIN_SUBMODULES" ] || return 1
    while IFS= read -r line; do
        [[ $line =~ ^[^[:space:]]+\ [0-9a-f]{40}$ ]] || return 1
    done <<< "$Q3_PIN_SUBMODULES"
    for key in MPW_SHA256 OPENGL_SHA256; do
        [[ $(q3_version "$key") =~ ^[0-9a-f]{64}$ ]] || return 1
    done
    for key in MPW_FILE MPW_URL OPENGL_FILE OPENGL_URL; do
        [ -n "$(q3_version "$key")" ] || return 1
    done
}
check "retro68-versions.txt pins a full commit, its submodules and both archive digests" \
    versions_well_formed

# The stub checkout at DIR is at the pinned commit and submodules.
at_pinned_commit() {
    [ "$(cat "$1/.git/stub-head")" = "$Q3_PIN" ] &&
        [ "$(cat "$1/.git/stub-sub" 2> /dev/null)" = "$Q3_PIN_SUBMODULES" ]
}

# Setup stopped before extracting an SDK archive: nothing extracted, built or
# moved aside.
rejected_before_extraction() {
    local root="$1"
    [ ! -e "$root/unar.called" ] && [ ! -e "$root/build-toolchain.called" ] &&
        [ ! -e "$root/tools/temp_mpw" ] && [ ! -e "$root/tools/temp_opengl" ] &&
        [ -z "$(find "$root/tools" -maxdepth 1 -name 'Retro68-*.*-*')" ]
}

# A fresh setup, as on a new host: tools/ holds only the SDK archives.
make_setup_root "$ROOT"
rm -rf "$ROOT/tools/Retro68-src" "$ROOT/tools/Retro68-build" "$ROOT/tools/Retro68-work"
run_setup "$ROOT"
expect "a fresh setup_retro68.sh clones Retro68 and builds" 42 "Step 5: Building Retro68"
check "a fresh setup_retro68.sh checks out the pinned commit and submodules, not upstream HEAD" \
    at_pinned_commit "$ROOT/tools/Retro68-src"
check "a fresh setup_retro68.sh clones the pinned URL and checks out the pinned commit" \
    bash -c 'grep -q -F "git clone --no-checkout $2 " "$1" && grep -q -F " checkout --detach $3" "$1"' \
    _ "$ROOT/git.log" "$Q3_PIN_URL" "$Q3_PIN"

make_setup_root "$ROOT"
rm "$ROOT/tools/Retro68-src/.git/stub-sub"
run_setup "$ROOT"
expect "setup_retro68.sh resumes on a checkout of the pinned commit" 42 "resuming with --skip-thirdparty"
check "setup_retro68.sh checks out the pinned submodules of an existing checkout" \
    at_pinned_commit "$ROOT/tools/Retro68-src"
check "setup_retro68.sh never pulls an existing checkout" \
    bash -c '! grep -q -E "^git .*(pull|fetch|clone|checkout)" "$1"' _ "$ROOT/git.log"

# A checkout at another commit (for example one that followed upstream) stops
# setup before anything changes, even when the toolchain needs a rebuild.
make_setup_root "$ROOT"
echo "$Q3_UPSTREAM" > "$ROOT/tools/Retro68-src/.git/stub-head"
break_cc1 "$ROOT/tools/Retro68-build"
Q3_BEFORE="$(snapshot "$ROOT/tools/Retro68-src"; snapshot "$ROOT/tools/Retro68-work")"
run_setup "$ROOT"
expect "setup_retro68.sh stops on a Retro68 checkout at another commit" 1 \
    "is at $Q3_UPSTREAM" "pins Retro68 $Q3_PIN. Nothing was changed."
check "setup_retro68.sh neither moved nor updated a checkout at another commit" \
    bash -c '! grep -q -E "^git .*(pull|fetch|clone|checkout|submodule update)" "$1/git.log"' _ "$ROOT"
check "setup_retro68.sh left the source and work tree as they were" \
    test "$Q3_BEFORE" = "$(snapshot "$ROOT/tools/Retro68-src"; snapshot "$ROOT/tools/Retro68-work")"
check "setup_retro68.sh extracted, built and moved nothing for a checkout at another commit" \
    rejected_before_extraction "$ROOT"

make_setup_root "$ROOT"
rm -rf "$ROOT/tools/Retro68-src/.git"
run_setup "$ROOT"
expect "setup_retro68.sh stops on a Retro68 source that is not a git checkout" 1 \
    "is at not a git checkout" "Nothing was changed."

# A pin whose submodule commit is not the one the pinned commit records, as
# after bumping RETRO68_COMMIT alone.
make_setup_root "$ROOT"
set_version "$ROOT" RETRO68_SUBMODULE "${Q3_PIN_SUBMODULES%% *} 3333333333333333333333333333333333333333"
run_setup "$ROOT"
expect "setup_retro68.sh stops when the submodules are not the pinned ones" 1 \
    "are not the ones retro68-versions.txt pins" "3333333333333333333333333333333333333333"
check "setup_retro68.sh extracted and built nothing for other submodules" \
    rejected_before_extraction "$ROOT"

make_setup_root "$ROOT"
set_version "$ROOT" RETRO68_COMMIT "${Q3_PIN:0:10}"
run_setup "$ROOT"
expect "setup_retro68.sh stops on an abbreviated RETRO68_COMMIT" 1 \
    "RETRO68_COMMIT is not a full commit hash: ${Q3_PIN:0:10}"
check "setup_retro68.sh ran no git and built nothing with a malformed retro68-versions.txt" \
    bash -c '[ ! -e "$1/git.log" ] && [ ! -e "$1/build-toolchain.called" ]' _ "$ROOT"

# SDK archives: a copy that does not match its pinned SHA-256 is never
# extracted, and no archive is extracted until both match.
make_setup_root "$ROOT"
echo "tampered" >> "$ROOT/tools/$Q3_MPW_FILE"
break_cc1 "$ROOT/tools/Retro68-build"
run_setup "$ROOT"
expect "setup_retro68.sh rejects a tampered MPW archive" 1 \
    "tools/$Q3_MPW_FILE does not match the SHA-256 pinned in retro68-versions.txt" \
    "no copy of $Q3_MPW_FILE matches its pinned SHA-256"
check "setup_retro68.sh extracted, built and moved nothing for a tampered archive" \
    rejected_before_extraction "$ROOT"

make_setup_root "$ROOT"
head -c 5 "$ROOT/tools/$Q3_OPENGL_FILE" > "$ROOT/truncated.sit"
mv "$ROOT/truncated.sit" "$ROOT/tools/$Q3_OPENGL_FILE"
run_setup "$ROOT"
expect "setup_retro68.sh rejects a truncated OpenGL SDK archive" 1 \
    "tools/$Q3_OPENGL_FILE does not match the SHA-256" ", 5 bytes"
check "setup_retro68.sh extracted neither archive when one was truncated" \
    rejected_before_extraction "$ROOT"

make_setup_root "$ROOT"
cp "$ROOT/tools/$Q3_MPW_FILE" "$ROOT/$Q3_MPW_FILE"
echo "tampered" >> "$ROOT/tools/$Q3_MPW_FILE"
run_setup "$ROOT"
expect "setup_retro68.sh uses a matching repo-root archive over a damaged tools/ copy" 42 \
    "tools/$Q3_MPW_FILE does not match" "Using $Q3_MPW_FILE (SHA-256 matches"
check "setup_retro68.sh extracted the repo-root copy and left the damaged one alone" \
    bash -c 'grep -q -F "unar -q -f $2 -o tools/temp_mpw" "$1/unar.called" &&
        grep -q tampered "$1/tools/$2"' _ "$ROOT" "$Q3_MPW_FILE"

# Downloads go to <name>.part and are kept only if they match.
make_setup_root "$ROOT"
mv "$ROOT/tools/$Q3_MPW_FILE" "$ROOT/good.sit"
printf '#!/bin/sh\necho "wget $*" >> %q\n%s\ncat %q > "$out"\n' "$ROOT/wget.called" \
    'out=""; prev=""; for arg in "$@"; do [ "$prev" = "-O" ] && out="$arg"; prev="$arg"; done' \
    "$ROOT/download.sit" > "$ROOT/stubs/wget"
chmod +x "$ROOT/stubs/wget"
echo "another archive" > "$ROOT/download.sit"
run_setup "$ROOT"
expect "setup_retro68.sh rejects a download that does not match" 1 \
    "tools/$Q3_MPW_FILE.part does not match the SHA-256" "was discarded"
check "setup_retro68.sh kept no part of the rejected download and extracted nothing" \
    bash -c '[ ! -e "$1/tools/$2" ] && [ ! -e "$1/tools/$2.part" ] && [ -s "$1/wget.called" ]' \
    _ "$ROOT" "$Q3_MPW_FILE"
check "setup_retro68.sh extracted, built and moved nothing for a rejected download" \
    rejected_before_extraction "$ROOT"
cp "$ROOT/good.sit" "$ROOT/download.sit"
run_setup "$ROOT"
expect "setup_retro68.sh keeps a download that matches" 42 "resuming with --skip-thirdparty"
check "setup_retro68.sh extracted the matching download from tools/" \
    bash -c 'cmp -s "$1/good.sit" "$1/tools/$2" && [ ! -e "$1/tools/$2.part" ] &&
        grep -q -F "unar -q -f tools/$2 " "$1/unar.called"' _ "$ROOT" "$Q3_MPW_FILE"

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
        "failed because of its surroundings"
    fresh_toolchain
    env_gcc 'kill -KILL $$'
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1: a compile killed by SIGKILL is status 3" 3 \
        "was stopped from outside (exit status 137)"
    fresh_toolchain
    env_gcc '[ -n "$out" ] && chmod a-w "$(dirname "$out")"' 'exit 1'
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1: a compile that leaves the scratch directory read-only is status 3" 3 \
        "no longer takes writes"
    fresh_toolchain
    env_ar 'echo "powerpc-apple-macos-ar: could not create temporary file whilst writing archive: no more archived files" >&2' 'exit 1'
    run_ps_check -ToolsOnly -InstallDir "$TC"
    expect "check_retro68.ps1: an ar out of temporary files is status 3" 3 \
        "could not create temporary file whilst writing archive"
    fresh_toolchain
    Q3_STATUS=0
    (umask 222; exec env TMPDIR="$Q3_CHECK_TMP" pwsh -NoProfile -NonInteractive \
        -File "$Q3_TEST_ROOT/check_retro68.ps1" -ToolsOnly -InstallDir "$TC") \
        > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "check_retro68.ps1: a scratch directory created read-only (umask 222) is status 3" 3 \
        "does not take 1 MiB"
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
    # Issue #227: the existing checkout is neither pulled nor moved.
    check "setup_retro68.ps1 never pulls an existing checkout" \
        bash -c '! grep -q -E "^git .*(pull|fetch|clone|checkout)" "$1/git.log"' _ "$ROOT"
    check "setup_retro68.ps1 keeps an existing checkout at the pinned commit and submodules" \
        at_pinned_commit "$ROOT/tools/Retro68-src"

    # ps_broken_toolchain ROOT: setup_retro68.ps1 would rebuild this toolchain.
    ps_broken_toolchain() {
        break_cc1 "$1/tools/Retro68-build"
        cp -p "$1/tools/Retro68-build/bin/powerpc-apple-macos-gcc" \
            "$1/tools/Retro68-build/bin/powerpc-apple-macos-gcc.exe"
    }

    make_ps_setup_root "$ROOT"
    rm -rf "$ROOT/tools/Retro68-src" "$ROOT/tools/Retro68-build" "$ROOT/tools/Retro68-work"
    run_ps_setup "$ROOT"
    expect "a fresh setup_retro68.ps1 clones Retro68 and builds" 1 "Build failed."
    check "a fresh setup_retro68.ps1 checks out the pinned commit and submodules, not upstream HEAD" \
        at_pinned_commit "$ROOT/tools/Retro68-src"
    check "a fresh setup_retro68.ps1 ran the pinned checkout's build-toolchain.bash" \
        grep -q -x "prefix removed" "$ROOT/build-toolchain.called"

    make_ps_setup_root "$ROOT"
    echo "$Q3_UPSTREAM" > "$ROOT/tools/Retro68-src/.git/stub-head"
    ps_broken_toolchain "$ROOT"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 stops on a Retro68 checkout at another commit" 1 \
        "is at $Q3_UPSTREAM" "pins Retro68 $Q3_PIN. Nothing was changed."
    check "setup_retro68.ps1 neither moved nor updated a checkout at another commit" \
        bash -c '[ "$(cat "$1/tools/Retro68-src/.git/stub-head")" = "$2" ] &&
            ! grep -q -E "^git .*(pull|fetch|clone|checkout|submodule update)" "$1/git.log"' \
        _ "$ROOT" "$Q3_UPSTREAM"
    check "setup_retro68.ps1 extracted, built and renamed nothing for a checkout at another commit" \
        rejected_before_extraction "$ROOT"

    make_ps_setup_root "$ROOT"
    set_version "$ROOT" RETRO68_SUBMODULE "${Q3_PIN_SUBMODULES%% *} 3333333333333333333333333333333333333333"
    ps_broken_toolchain "$ROOT"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 stops when the submodules are not the pinned ones" 1 \
        "are not the ones retro68-versions.txt pins" "3333333333333333333333333333333333333333"
    check "setup_retro68.ps1 extracted, built and renamed nothing for other submodules" \
        rejected_before_extraction "$ROOT"

    make_ps_setup_root "$ROOT"
    set_version "$ROOT" MPW_SHA256 "${Q3_PIN:0:10}"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 stops on a malformed MPW_SHA256" 1 "MPW_SHA256 has an invalid value"
    check "setup_retro68.ps1 ran no git with a malformed retro68-versions.txt" test ! -e "$ROOT/git.log"

    make_ps_setup_root "$ROOT"
    echo "tampered" >> "$ROOT/tools/$Q3_MPW_FILE"
    ps_broken_toolchain "$ROOT"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 rejects a tampered MPW archive" 1 \
        "$Q3_MPW_FILE does not match the SHA-256 pinned in retro68-versions.txt" \
        "no copy of $Q3_MPW_FILE matches its pinned SHA-256"
    check "setup_retro68.ps1 extracted, built and renamed nothing for a tampered archive" \
        rejected_before_extraction "$ROOT"

    make_ps_setup_root "$ROOT"
    head -c 5 "$ROOT/tools/$Q3_OPENGL_FILE" > "$ROOT/truncated.sit"
    mv "$ROOT/truncated.sit" "$ROOT/tools/$Q3_OPENGL_FILE"
    ps_broken_toolchain "$ROOT"
    run_ps_setup "$ROOT"
    expect "setup_retro68.ps1 rejects a truncated OpenGL SDK archive" 1 \
        "$Q3_OPENGL_FILE does not match the SHA-256" ", 5 bytes"
    check "setup_retro68.ps1 extracted neither archive when one was truncated" \
        rejected_before_extraction "$ROOT"
else
    echo "SKIP: pwsh is not installed; check_retro68.ps1 and setup_retro68.ps1 not exercised"
fi

# ---- build manifest (issue #227) ----

# make_manifest_root DIR: a stub compiler, the Retro68 source next to it at
# the pinned commit and submodules (played by the stub git), and a PEF.
make_manifest_root() {
    local root="$1"
    rm -rf "$root"
    mkdir -p "$root/tools/Retro68-build/bin" "$root/tools/Retro68-src/.git" "$root/build"
    write_git_stub "$root"
    echo "$Q3_PIN" > "$root/tools/Retro68-src/.git/stub-head"
    cp "$root/pinned-gitlinks" "$root/tools/Retro68-src/.git/stub-sub"
    printf '#!/bin/sh\n%s\n' \
        'printf "powerpc-apple-macos-gcc (GCC) 12.2.0\nCopyright (C) 2022 Free Software Foundation, Inc.\n"' \
        > "$root/tools/Retro68-build/bin/powerpc-apple-macos-gcc"
    chmod +x "$root/tools/Retro68-build/bin/powerpc-apple-macos-gcc"
    printf 'Joy!peffpwpc first build' > "$root/Quake3.pef"
}

# run_manifest ROOT [VERSIONS]: the manifest script as the build runs it.
run_manifest() {
    local root="$1" versions="${2:-$Q3_VERSIONS}"
    Q3_STATUS=0
    cmake "-DQ3_MANIFEST_PEF=$root/Quake3.pef" "-DQ3_MANIFEST_OUTPUT=$root/build/Quake3.manifest.txt" \
        "-DQ3_MANIFEST_COMPILER=$root/tools/Retro68-build/bin/powerpc-apple-macos-gcc" \
        "-DQ3_MANIFEST_GIT=$root/stubs/git" "-DQ3_MANIFEST_RETRO68_SOURCE=$root/tools/Retro68-src" \
        "-DQ3_MANIFEST_VERSIONS=$versions" -P "$Q3_TEST_ROOT/cmake/Quake3BuildManifest.cmake" \
        > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

# manifest_is FILE PEF COMMIT PINNED SUBMODULES: FILE's fields are exactly
# these, in this order, for the PEF at PEF. SUBMODULES holds one
# "<path> <commit>[ note]" per line, or nothing.
manifest_is() {
    local file="$1" pef="$2" commit="$3" pinned="$4" submodules="$5" expected
    expected="pef=Quake3.pef
pef_sha256=$(q3_sha256 "$pef")
gcc_version=powerpc-apple-macos-gcc (GCC) 12.2.0
retro68_commit=$commit"
    if [ -n "$submodules" ]; then
        expected="$expected
$(printf '%s\n' "$submodules" | LC_ALL=C sort | sed 's/^/retro68_submodule=/')"
    fi
    expected="$expected
retro68_pinned=$pinned"
    [ "$(grep -v '^#' "$file")" = "$expected" ] || {
        echo "  expected:"; printf '%s\n' "$expected" | sed 's/^/    /'
        echo "  found:"; sed 's/^/    /' "$file"
        return 1
    }
}

if command -v cmake > /dev/null 2>&1; then
    MANIFEST="$Q3_TEST_WORK/manifest"
    make_manifest_root "$MANIFEST"
    run_manifest "$MANIFEST"
    expect "the build manifest script runs" 0
    check "the build manifest records the PEF's SHA-256, gcc --version and the pinned Retro68" \
        manifest_is "$MANIFEST/build/Quake3.manifest.txt" "$MANIFEST/Quake3.pef" "$Q3_PIN" yes \
        "$Q3_PIN_SUBMODULES"
    cp "$MANIFEST/build/Quake3.manifest.txt" "$MANIFEST/first.txt"
    run_manifest "$MANIFEST"
    check "the build manifest is the same when nothing changed" \
        cmp -s "$MANIFEST/first.txt" "$MANIFEST/build/Quake3.manifest.txt"
    printf 'Joy!peffpwpc second build' > "$MANIFEST/Quake3.pef"
    run_manifest "$MANIFEST"
    check "a different PEF changes only pef_sha256 in the build manifest" \
        bash -c '[ "$(diff "$1" "$2" | grep -c "^[<>]")" = 2 ] &&
            [ "$(diff "$1" "$2" | grep "^[<>]" | cut -c 3- | cut -d = -f 1 | sort -u)" = pef_sha256 ]' \
        _ "$MANIFEST/first.txt" "$MANIFEST/build/Quake3.manifest.txt"

    # A checkout of another commit, with that commit's submodules.
    make_manifest_root "$MANIFEST"
    echo "$Q3_UPSTREAM" > "$MANIFEST/tools/Retro68-src/.git/stub-head"
    sed "s/ .*/ $Q3_UPSTREAM_SUB/" "$MANIFEST/pinned-gitlinks" > "$MANIFEST/tools/Retro68-src/.git/stub-sub"
    run_manifest "$MANIFEST"
    check "the build manifest records a Retro68 checkout at another commit as not pinned" \
        manifest_is "$MANIFEST/build/Quake3.manifest.txt" "$MANIFEST/Quake3.pef" "$Q3_UPSTREAM" \
        "no (retro68-versions.txt pins $Q3_PIN)" \
        "$(printf '%s\n' "$Q3_PIN_SUBMODULES" | sed "s/ .*/ $Q3_UPSTREAM_SUB/")"

    make_manifest_root "$MANIFEST"
    sed "s/ .*/ 3333333333333333333333333333333333333333/" "$MANIFEST/pinned-gitlinks" \
        > "$MANIFEST/tools/Retro68-src/.git/stub-sub"
    run_manifest "$MANIFEST"
    check "the build manifest flags a submodule at another commit" \
        manifest_is "$MANIFEST/build/Quake3.manifest.txt" "$MANIFEST/Quake3.pef" "$Q3_PIN" \
        "no (retro68-versions.txt pins $Q3_PIN)" \
        "$(printf '%s\n' "$Q3_PIN_SUBMODULES" |
            sed "s/ .*/ 3333333333333333333333333333333333333333 (git submodule status flag '+')/")"

    make_manifest_root "$MANIFEST"
    rm -rf "$MANIFEST/tools/Retro68-src"
    run_manifest "$MANIFEST"
    check "the build manifest says unknown without a Retro68 checkout" \
        manifest_is "$MANIFEST/build/Quake3.manifest.txt" "$MANIFEST/Quake3.pef" unknown \
        "no (retro68-versions.txt pins $Q3_PIN)" ""

    # The build runs the script after MakePEF and again whenever the PEF
    # changes: a scratch project wires the same function to a stub PEF step.
    make_manifest_root "$MANIFEST"
    mkdir -p "$MANIFEST/project"
    cp "$Q3_VERSIONS" "$MANIFEST/project/"
    cat > "$MANIFEST/project/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.12)
project(ManifestWiring NONE)
set(CMAKE_C_COMPILER "$MANIFEST/tools/Retro68-build/bin/powerpc-apple-macos-gcc")
set(RETRO68_INSTALL_ROOT "$MANIFEST/tools/Retro68-build")
include("$Q3_TEST_ROOT/cmake/Quake3BuildManifest.cmake")
add_custom_command(OUTPUT Quake3.pef
    COMMAND "\${CMAKE_COMMAND}" -E copy "$MANIFEST/Quake3.pef" Quake3.pef
    DEPENDS "$MANIFEST/Quake3.pef" VERBATIM)
quake3_add_build_manifest(Quake3)
add_custom_target(Quake3_APPL ALL DEPENDS Quake3.manifest.txt)
EOF
    Q3_STATUS=0
    { cmake -S "$MANIFEST/project" -B "$MANIFEST/project/build" "-DGIT_EXECUTABLE=$MANIFEST/stubs/git" &&
        cmake --build "$MANIFEST/project/build"; } > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "a build that uses quake3_add_build_manifest succeeds" 0 "Recording the build manifest of Quake3"
    check "the build writes the manifest next to the PEF" \
        manifest_is "$MANIFEST/project/build/Quake3.manifest.txt" "$MANIFEST/project/build/Quake3.pef" \
        "$Q3_PIN" yes "$Q3_PIN_SUBMODULES"
    sleep 1
    printf 'Joy!peffpwpc rebuilt' > "$MANIFEST/Quake3.pef"
    Q3_STATUS=0
    cmake --build "$MANIFEST/project/build" > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    expect "a rebuild after the PEF changes succeeds" 0 "Recording the build manifest of Quake3"
    check "the rebuild records the new PEF's SHA-256" \
        grep -q -x "pef_sha256=$(q3_sha256 "$MANIFEST/Quake3.pef")" \
        "$MANIFEST/project/build/Quake3.manifest.txt"
else
    echo "SKIP: cmake is not installed; the build manifest script was not run"
fi

# CMakeLists.txt uses the function for every Classic application, and each
# application's target depends on its manifest.
classic_applications_record_manifests() {
    local body
    body="$(sed -n '/^function(quake3_add_classic_application /,/^endfunction()/p' "$Q3_TEST_ROOT/CMakeLists.txt")"
    grep -q -x -F 'include("${CMAKE_SOURCE_DIR}/cmake/Quake3BuildManifest.cmake")' "$Q3_TEST_ROOT/CMakeLists.txt" &&
        printf '%s\n' "$body" | grep -q -x -F '    quake3_add_build_manifest(${target})' &&
        printf '%s\n' "$body" | grep -q -F '"${target}.manifest.txt")'
}
check "CMakeLists.txt writes a build manifest for every Classic application" \
    classic_applications_record_manifests

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

# real_pin NAME PROBLEM: PASS NAME when PROBLEM is empty; otherwise SKIP (a
# local toolchain that is not the pinned one), or FAIL with
# Q3_RETRO68_REQUIRE=1.
real_pin() {
    if [ -z "$2" ]; then
        echo "PASS: $1"
    elif [ "${Q3_RETRO68_REQUIRE:-0}" = 1 ]; then
        echo "FAIL: $1: $2"
        Q3_FAILED=1
    else
        echo "SKIP: $1: $2"
    fi
}

# The Retro68 source next to the real toolchain, read without taking locks.
Q3_REAL_SRC="$(dirname "$Q3_REAL")/Retro68-src"
if [ -e "$Q3_REAL_SRC/.git" ] && command -v git > /dev/null 2>&1; then
    Q3_PROBLEM=""
    Q3_FOUND="$(GIT_OPTIONAL_LOCKS=0 git -C "$Q3_REAL_SRC" rev-parse HEAD 2> /dev/null)"
    [ "$Q3_FOUND" = "$Q3_PIN" ] || Q3_PROBLEM="HEAD is ${Q3_FOUND:-unknown}"
    while read -r Q3_PATH Q3_COMMIT; do
        Q3_LINK="$(GIT_OPTIONAL_LOCKS=0 git -C "$Q3_REAL_SRC" rev-parse "HEAD:$Q3_PATH" 2> /dev/null)"
        Q3_FOUND="$(GIT_OPTIONAL_LOCKS=0 git -C "$Q3_REAL_SRC/$Q3_PATH" rev-parse HEAD 2> /dev/null)"
        if [ "$Q3_LINK" != "$Q3_COMMIT" ] || [ "$Q3_FOUND" != "$Q3_COMMIT" ]; then
            Q3_PROBLEM="$Q3_PROBLEM${Q3_PROBLEM:+; }$Q3_PATH: recorded ${Q3_LINK:-nothing}, checked out ${Q3_FOUND:-nothing}"
        fi
    done <<< "$Q3_PIN_SUBMODULES"
    Q3_FOUND="$(GIT_OPTIONAL_LOCKS=0 git -C "$Q3_REAL_SRC" config -f .gitmodules \
        --get-regexp '^submodule\..*\.path$' | awk '{ print $2 }' | LC_ALL=C sort | tr '\n' ' ')"
    [ "$Q3_FOUND" = "$(printf '%s\n' "$Q3_PIN_SUBMODULES" | cut -d ' ' -f 1 | LC_ALL=C sort | tr '\n' ' ')" ] ||
        Q3_PROBLEM="$Q3_PROBLEM${Q3_PROBLEM:+; }its submodules are $Q3_FOUND"
    real_pin "the Retro68 source at $Q3_REAL_SRC is at the pinned commit and submodules" "$Q3_PROBLEM"
else
    echo "SKIP: no Retro68 git checkout at $Q3_REAL_SRC"
fi

# The SDK archives next to the real toolchain or at the repository root.
for Q3_KEY in MPW OPENGL; do
    Q3_FILE="$(q3_version "${Q3_KEY}_FILE")"
    Q3_FOUND=""
    for Q3_PATH in "$(dirname "$Q3_REAL")/$Q3_FILE" "$(dirname "$(dirname "$Q3_REAL")")/$Q3_FILE"; do
        [ -f "$Q3_PATH" ] || continue
        Q3_FOUND="$Q3_PATH"
        Q3_LINK="$(q3_sha256 "$Q3_PATH")"
        Q3_PROBLEM=""
        [ "$Q3_LINK" = "$(q3_version "${Q3_KEY}_SHA256")" ] || Q3_PROBLEM="its SHA-256 is $Q3_LINK"
        real_pin "$Q3_PATH matches ${Q3_KEY}_SHA256" "$Q3_PROBLEM"
    done
    [ -n "$Q3_FOUND" ] || echo "SKIP: no $Q3_FILE next to $Q3_REAL or at the repository root"
done

if [ "$Q3_FAILED" -ne 0 ]; then
    echo "Retro68 readiness tests FAILED"
    exit 1
fi
echo "Retro68 readiness tests passed"
