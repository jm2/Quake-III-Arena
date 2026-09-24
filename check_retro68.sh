#!/bin/bash
# Check that the Retro68 toolchain runs on this host (issue #269).
#
#   ./check_retro68.sh [--tools-only] [INSTALL_DIR]
#
# INSTALL_DIR defaults to tools/Retro68-build next to this script. Finding the
# programs is not enough: after a host OS upgrade `powerpc-apple-macos-gcc
# --version` still works while cc1, ar, Rez and MakeImport can no longer load
# their shared libraries (libisl, libfl, Boost), and CMake then reports only
# that the compiler identification is unknown. So this script runs the tools
# the build runs, in a scratch directory under ${TMPDIR:-/var/tmp}:
#   - powerpc-apple-macos-gcc --version, then gcc -c and g++ -c with the
#     CMakeLists.txt compile flags (cc1, cc1plus, as);
#   - powerpc-apple-macos-ar and -ranlib on the object, as CMake archives it;
#   - powerpc-apple-macos-ld --version, and MakePEF, MakeImport and Rez with
#     no input, which only has to get past the host loader;
# and, unless --tools-only is given:
#   - a C file that includes <MacTypes.h> and <math.h> is compiled, archived
#     and linked with --whole-archive, -lm and -lInterfaceLib as CMakeLists.txt
#     links Quake3 (ld, libretrocrt, the import libraries);
#   - MakePEF converts the XCOFF image, which must start with Joy!peff/pwpc;
#   - Rez combines that PEF with a resource into an APPL/IDQ3 MacBinary.
# --tools-only leaves out the steps that need the Toolbox headers and target
# libraries, which a --skip-thirdparty rebuild recreates. setup_retro68.sh
# uses it to decide whether resuming a build is safe.
#
# Exits 0 when the toolchain runs, 1 with a message naming the failed step and
# the tool's output (a missing library is named there, and on hosts with ldd
# every missing library of the toolchain is listed), and 2 on bad usage.
set -u

usage() {
    echo "Usage: $(basename "$0") [--tools-only] [INSTALL_DIR]"
}

TOOLS_ONLY=0
INSTALL_DIR=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --tools-only) TOOLS_ONLY=1 ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *)
            if [ -n "$INSTALL_DIR" ]; then
                usage >&2
                exit 2
            fi
            INSTALL_DIR="$1"
            ;;
    esac
    shift
done
if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="$(cd "$(dirname "$0")" && pwd)/tools/Retro68-build"
fi
BIN="$INSTALL_DIR/bin"
TARGET=powerpc-apple-macos

# Keep in step with CMakeLists.txt and cmake/Retro68.toolchain.cmake.
COMPILE_FLAGS="-std=gnu99 -fgnu89-inline -O0 -g -fno-strict-aliasing -fsigned-char -D__MACOS__ -D__POWERPC__"

WORK=""
cleanup() {
    if [ -n "$WORK" ]; then
        rm -rf -- "$WORK"
    fi
}
trap cleanup EXIT

# Lists the host libraries that the toolchain's executables cannot find.
report_missing_libraries() {
    local file missing

    command -v ldd > /dev/null 2>&1 || return 0
    missing=$(
        for file in "$BIN"/* "$INSTALL_DIR/libexec/gcc/$TARGET"/*/*; do
            case "${file##*/}" in m68k-*) continue ;; esac
            [ -f "$file" ] && [ -x "$file" ] || continue
            ldd "$file" 2> /dev/null |
                awk -v tool="${file##*/}" '/=> not found/ { print $1, tool }'
        done | sort -u |
            awk '$1 != library { if (line != "") print line; library = $1; line = "  " $1 " (needed by " $2; next }
                 { line = line ", " $2 }
                 END { if (line != "") print line }' |
            sed 's/$/)/'
    )
    if [ -n "$missing" ]; then
        echo "Host shared libraries the toolchain cannot find (ldd):"
        echo "$missing"
    fi
}

fail() {
    local step="$1" log="${2:-}"

    {
        echo "Error: Retro68 toolchain check failed in $INSTALL_DIR:"
        echo "  $step"
        if [ -n "$log" ] && [ -s "$log" ]; then
            sed -n '1,20p' "$log" | sed 's/^/  | /'
        fi
        report_missing_libraries
        echo "If a host shared library is missing (for example after an OS"
        echo "upgrade), install it or rebuild the toolchain: setup_retro68.sh"
        echo "rebuilds everything when the installed tools cannot run. See"
        echo "\"Checking and rebuilding the toolchain\" in docs/building-mac-os9.md."
    } >&2
    exit 1
}

require_tool() {
    [ -f "$BIN/$1" ] && [ -x "$BIN/$1" ] ||
        fail "$1 is missing or not executable in $BIN."
}

# Runs a tool that must succeed.
run_step() {
    local step="$1" log
    shift
    log="$WORK/step.log"
    if ! "$@" > "$log" 2>&1; then
        fail "$step failed:" "$log"
    fi
}

# Runs a tool without input: its own usage error is fine, but the host loader
# must not stop it (status 126/127, a signal, or a loader message).
run_probe() {
    local tool="$1" log status
    shift
    log="$WORK/probe.log"
    "$BIN/$tool" "$@" > "$log" 2>&1
    status=$?
    if [ "$status" -ge 126 ] ||
       grep -q -E "error while loading shared libraries|symbol lookup error|Library not loaded|not found \(required by" "$log"; then
        fail "$tool could not start (exit status $status):" "$log"
    fi
}

for tool in "$TARGET-gcc" "$TARGET-g++" "$TARGET-ar" "$TARGET-ranlib" \
            "$TARGET-ld" MakePEF MakeImport Rez; do
    require_tool "$tool"
done

WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-retro68-check.XXXXXX")" ||
    fail "could not create a scratch directory under ${TMPDIR:-/var/tmp}."
export PATH="$BIN:$PATH"

if [ "$TOOLS_ONLY" -eq 1 ]; then
    printf 'int retro68_check(void)\n{\n\treturn 0;\n}\n' > "$WORK/check.c"
else
    printf '%s\n' '#include <MacTypes.h>' '#include <math.h>' '' \
        'int main(int argc, char **argv)' '{' \
        '	Boolean ok = argc > 0 && argv != NULL;' \
        '	return ok && floor(1.5) == 1.0 ? 0 : 1;' '}' > "$WORK/check.c"
fi
printf 'int retro68_check_cxx(int value)\n{\n\treturn value + 1;\n}\n' > "$WORK/check_cxx.cc"

run_step "$TARGET-gcc --version" "$BIN/$TARGET-gcc" --version
# shellcheck disable=SC2086
run_step "compiling a C file ($TARGET-gcc -c)" \
    "$BIN/$TARGET-gcc" $COMPILE_FLAGS -c "$WORK/check.c" -o "$WORK/check.o"
run_step "compiling a C++ file ($TARGET-g++ -c)" \
    "$BIN/$TARGET-g++" -fsigned-char -c "$WORK/check_cxx.cc" -o "$WORK/check_cxx.o"
run_step "archiving the object ($TARGET-ar qc)" \
    "$BIN/$TARGET-ar" qc "$WORK/libcheck.a" "$WORK/check.o"
run_step "indexing the archive ($TARGET-ranlib)" \
    "$BIN/$TARGET-ranlib" "$WORK/libcheck.a"
run_step "$TARGET-ld --version" "$BIN/$TARGET-ld" --version
run_probe MakePEF
run_probe MakeImport
run_probe Rez --help

if [ "$TOOLS_ONLY" -eq 0 ]; then
    # shellcheck disable=SC2086
    run_step "linking an XCOFF image ($TARGET-gcc)" \
        "$BIN/$TARGET-gcc" $COMPILE_FLAGS \
        -Wl,--whole-archive "$WORK/libcheck.a" -Wl,--no-whole-archive \
        -lm -lInterfaceLib -o "$WORK/check.xcoff"
    run_step "converting the XCOFF image to PEF (MakePEF)" \
        "$BIN/MakePEF" "$WORK/check.xcoff" -o "$WORK/check.pef"
    if [ "$(head -c 12 "$WORK/check.pef" 2> /dev/null)" != "Joy!peffpwpc" ]; then
        fail "MakePEF did not write a PowerPC PEF (no Joy!peff/pwpc header)."
    fi
    printf "data 'Q3ck' (128) {\n\t\$\"00\"\n};\n" > "$WORK/check.r"
    run_step "building an application with Rez" \
        "$BIN/Rez" "$WORK/check.r" -t APPL -c IDQ3 --data "$WORK/check.pef" \
        -o "$WORK/check.bin"
    if [ ! -s "$WORK/check.bin" ]; then
        fail "Rez did not write an application."
    fi
fi

if [ "$TOOLS_ONLY" -eq 1 ]; then
    echo "Retro68 tools run: $INSTALL_DIR"
else
    echo "Retro68 toolchain check passed: $INSTALL_DIR"
fi
exit 0
