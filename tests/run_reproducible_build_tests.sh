#!/usr/bin/env bash
# Issue #405: the same commit built in two checkout directories, with the same
# SOURCE_DATE_EPOCH, must give byte-identical PEF and application containers.
#
#  1. cmake/Quake3PrefixMap.cmake, which CMakeLists.txt includes, must keep the
#     checkout directory out of compiler output. A small project includes it
#     from two source trees of different depth, with the build tree inside the
#     source tree (as build_mac/ is) and outside it. $CC compiles the same
#     source with the options it sets: the objects must be identical and must
#     not name either directory. Without them they must differ.
#  2. mac_app.py must write SOURCE_DATE_EPOCH over every date Rez stores
#     (finalize) and over every date genisoimage stamps on package mode's
#     install image (set-dates). tests/test_packaging_tools.py checks that on
#     synthetic containers. When Retro68's Rez is installed (tools/Retro68-build
#     or PATH), two Rez and finalize runs of code/mac/mac_resources.r, in
#     different directories and time zones a second apart, must give identical
#     .bin and AppleDouble files. When genisoimage (or mkisofs) is installed
#     too, two install images made from them the way build_mac.sh package
#     makes them (set-dates on the tree, genisoimage in UTC, set-dates on the
#     image, ndif_image.py) must be identical.
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-reproducible.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
CC="${CC:-cc}"

fail() {
    echo "run_reproducible_build_tests: $*" >&2
    exit 1
}

command -v cmake > /dev/null 2>&1 || fail "cmake is required"
grep -qF 'include("${CMAKE_SOURCE_DIR}/cmake/Quake3PrefixMap.cmake")' \
    "$Q3_TEST_ROOT/CMakeLists.txt" ||
    fail "CMakeLists.txt does not include cmake/Quake3PrefixMap.cmake"

# --- 1. Compiler output names no checkout directory -------------------------

# write_tree <source dir>: a project that records the prefix map options.
write_tree() {
    mkdir -p "$1/code" "$1/include"
    cat > "$1/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.12)
project(prefix_map_probe NONE)
include("$Q3_TEST_ROOT/cmake/Quake3PrefixMap.cmake")
get_property(options DIRECTORY PROPERTY COMPILE_OPTIONS)
string(REPLACE ";" "\\n" options "\${options}")
file(WRITE "\${CMAKE_BINARY_DIR}/options.txt" "\${options}\\n")
EOF
    cat > "$1/include/probe.h" <<'EOF'
static const char probe_header[] = __FILE__;
EOF
    cat > "$1/code/probe.c" <<'EOF'
#include <assert.h>
#include "probe.h"

const char *probe_file(void) { return __FILE__; }
const char *probe_base(void) { return __BASE_FILE__; }
const char *probe_include(void) { return probe_header; }
int probe_assert(int value) { assert(value > 0); return value; }
EOF
}

# compile <source dir> <build dir> <object> [options...]: compile probe.c from
# the build directory by absolute paths, as CMake's Makefiles do.
compile() {
    local source="$1" build="$2" object="$3"
    shift 3
    (cd "$build" && "$CC" "$@" -std=gnu99 -O0 -g -I"$source/include" \
        -c "$source/code/probe.c" -o "$object")
}

# probe <layout> <source dir> <build dir>: configure and compile, with and
# without the options; leaves <build dir>/with.o and <build dir>/without.o.
probe() {
    local layout="$1" source="$2" build="$3" options
    write_tree "$source"
    cmake -S "$source" -B "$build" > "$source.cmake.log" 2>&1 ||
        { cat "$source.cmake.log" >&2; fail "$layout: cmake failed"; }
    mapfile -t options < <(grep -v '^$' "$build/options.txt")
    [ "${#options[@]}" -gt 0 ] || fail "$layout: Quake3PrefixMap.cmake set no options"
    compile "$source" "$build" with.o "${options[@]}"
    compile "$source" "$build" without.o
}

check_layout() {
    local layout="$1" source_a="$2" build_a="$3" source_b="$4" build_b="$5" dir
    probe "$layout" "$source_a" "$build_a"
    probe "$layout" "$source_b" "$build_b"
    if ! cmp -s "$build_a/with.o" "$build_b/with.o"; then
        cmp "$build_a/with.o" "$build_b/with.o" >&2 || true
        fail "$layout: the same source compiled in two directories differs"
    fi
    for dir in "$source_a" "$build_a" "$source_b" "$build_b"; do
        if grep -qaF "$dir" "$build_a/with.o" "$build_b/with.o"; then
            fail "$layout: an object names $dir"
        fi
    done
    grep -qaF "./code/probe.c" "$build_a/with.o" ||
        fail "$layout: __FILE__ is not ./code/probe.c"
    # The control: without the map the directories are in the objects.
    cmp -s "$build_a/without.o" "$build_b/without.o" &&
        fail "$layout: objects built without the prefix map are identical; the check proves nothing"
    grep -qaF "$source_a/code/probe.c" "$build_a/without.o" ||
        fail "$layout: an object built without the prefix map does not name its source"
    echo "prefix map ($layout): identical objects, no checkout directory"
}

T="$Q3_TEST_DIR"
check_layout "build tree inside the source tree" \
    "$T/a/src" "$T/a/src/build_mac" \
    "$T/b/a/much/longer/checkout/src" "$T/b/a/much/longer/checkout/src/build_mac"
check_layout "build tree outside the source tree" \
    "$T/c/src" "$T/c/out" \
    "$T/d/another/src" "$T/d/elsewhere/out"

# --- 2. finalize writes SOURCE_DATE_EPOCH over Rez's dates ------------------

(cd "$Q3_TEST_ROOT" && python3 -m unittest -q tests.test_packaging_tools.ReproducibleDateTests)

REZ="$Q3_TEST_ROOT/tools/Retro68-build/bin/Rez"
[ -x "$REZ" ] || REZ="$(command -v Rez || true)"
if [ -z "$REZ" ]; then
    echo "Rez runs: skipped (Retro68's Rez is not installed)"
    exit 0
fi
REZ_ROOT="$(dirname "$(dirname "$REZ")")"
RINCLUDES=""
for candidate in "$REZ_ROOT/universal/RIncludes" \
        "$Q3_TEST_ROOT/tools/Retro68-build/universal/RIncludes" \
        "$Q3_TEST_ROOT/tools/Retro68-src/InterfacesAndLibraries/Interfaces/RIncludes"; do
    if [ -f "$candidate/Types.r" ] && [ -f "$candidate/CodeFragments.r" ]; then
        RINCLUDES="$candidate"
        break
    fi
done
[ -n "$RINCLUDES" ] || fail "Rez is installed but its RIncludes are not"

# A stand-in PEF: finalize checks only the magic and that every data fork is it.
{ printf 'Joy!peffpwpc'; head -c 200000 /dev/zero | tr '\0' 'q'; } > "$T/Quake3.pef"

# rez_app <dir> <TZ> [SOURCE_DATE_EPOCH]: Rez and finalize as CMakeLists.txt runs them.
rez_app() {
    local dir="$1" zone="$2"
    mkdir -p "$dir"
    cp "$T/Quake3.pef" "$dir/Quake3.pef"
    (
        cd "$dir"
        export TZ="$zone"
        if [ "$#" -gt 2 ]; then export SOURCE_DATE_EPOCH="$3"; else unset SOURCE_DATE_EPOCH; fi
        "$REZ" "$Q3_TEST_ROOT/code/mac/mac_resources.r" -I "$RINCLUDES" \
            -I "$Q3_TEST_ROOT/code/mac" -t APPL -c IDQ3 --data Quake3.pef \
            -o Quake3.bin --cc %Quake3.ad
        python3 "$Q3_TEST_ROOT/mac_app.py" finalize --pef Quake3.pef \
            Quake3.bin %Quake3.ad > /dev/null
    ) || fail "Rez or finalize failed in $dir"
}

rez_app "$T/rez/one" UTC 1767225600
sleep 1
rez_app "$T/rez/a/second/checkout" Pacific/Auckland 1767225600
for output in Quake3.bin Quake3.ad %Quake3.ad; do
    if ! cmp -s "$T/rez/one/$output" "$T/rez/a/second/checkout/$output"; then
        cmp -l "$T/rez/one/$output" "$T/rez/a/second/checkout/$output" | head -n 8 >&2 || true
        fail "two Rez and finalize runs with the same SOURCE_DATE_EPOCH give different $output"
    fi
done

# The control: without SOURCE_DATE_EPOCH the dates are Rez's.
rez_app "$T/rez/unset" Pacific/Auckland
cmp -s "$T/rez/one/Quake3.bin" "$T/rez/unset/Quake3.bin" &&
    fail "Rez's MacBinary dates were already reproducible; the check proves nothing"
echo "Rez runs: identical .bin and AppleDouble files with SOURCE_DATE_EPOCH"

# --- 3. Package mode's install image ----------------------------------------

MKISOFS="$(command -v genisoimage || command -v mkisofs || true)"
if [ -z "$MKISOFS" ]; then
    echo "Install images: skipped (genisoimage and mkisofs are not installed)"
    exit 0
fi
head -c 300000 /dev/urandom > "$T/pak0.pk3"
cat > "$T/hfs_mapping.txt" <<'EOF'
.pk3   Raw   'IDQ3' 'Stak' "Quake 3 Data"
Quake3 Raw   'IDQ3' 'APPL' "Quake 3 App"
EOF

# install_image <dir> <caller TZ> <app dir> [SOURCE_DATE_EPOCH]: what
# build_mac.sh package does with the applications and the game data.
install_image() {
    local dir="$1" zone="$2" app="$3"
    local folder="$dir/content/Quake 3 Arena"
    mkdir -p "$folder/baseq3"
    cp "$T/pak0.pk3" "$folder/baseq3/pak0.pk3"
    cp "$app/Quake3.ad" "$folder/Quake3"
    cp "$app/%Quake3.ad" "$folder/%Quake3"
    (
        export TZ="$zone"
        if [ "$#" -gt 3 ]; then export SOURCE_DATE_EPOCH="$4"; else unset SOURCE_DATE_EPOCH; fi
        cd "$Q3_TEST_ROOT"
        python3 mac_app.py set-dates "$dir/content"
        TZ=UTC "$MKISOFS" -quiet -hfs -double -map "$T/hfs_mapping.txt" \
            -o "$dir/Quake3_Install.img" -V "Quake 3 Arena" "$dir/content"
        python3 mac_app.py set-dates "$dir/Quake3_Install.img"
        python3 mac_app.py verify --hfs-name Quake3 --pef "$app/Quake3.pef" \
            "$dir/Quake3_Install.img" > /dev/null
        python3 ndif_image.py "$dir/Quake3_Install.img" \
            "$dir/Quake3_Install.img.bin" "Quake 3 Arena" > /dev/null
    ) || fail "the install image in $dir could not be made"
}

install_image "$T/image/one" UTC "$T/rez/one" 1767225600
sleep 1
install_image "$T/image/a/second/checkout" Pacific/Auckland \
    "$T/rez/a/second/checkout" 1767225600
for output in Quake3_Install.img Quake3_Install.img.bin; do
    if ! cmp -s "$T/image/one/$output" "$T/image/a/second/checkout/$output"; then
        cmp -l "$T/image/one/$output" "$T/image/a/second/checkout/$output" | head -n 8 >&2 || true
        fail "two install images with the same SOURCE_DATE_EPOCH give different $output"
    fi
done

# The control: without SOURCE_DATE_EPOCH the image carries the time it was made.
install_image "$T/image/unset" UTC "$T/rez/one"
cmp -s "$T/image/one/Quake3_Install.img" "$T/image/unset/Quake3_Install.img" &&
    fail "genisoimage's images were already reproducible; the check proves nothing"
echo "Install images: identical .img and NDIF .img.bin with SOURCE_DATE_EPOCH"
