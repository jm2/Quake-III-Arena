#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-syscalls.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #258: the File Manager permission code/mac/mac_syscalls.c's _open_r
# asks for.  Issue #259: its _rename_r and _unlink_r, which libretro left as
# stubs returning -1, and the review of #492 (same-entry renames, long
# leaves, issue #327).  The regression includes that file whole; its
# <reent.h>, <Files.h>, <Errors.h> and <StringCompare.h> resolve to
# tests/mac_files_fake.h.
for Q3_TEST_HEADER in reent.h Files.h Errors.h StringCompare.h; do
    printf '#include "%s/tests/mac_files_fake.h"\n' "$Q3_TEST_ROOT" > "$Q3_TEST_DIR/$Q3_TEST_HEADER"
done

"${CC:-cc}" \
    -std=gnu99 -Wall -Wno-multichar -Wno-unused-function -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_syscalls_regression.c" \
    -o "$Q3_TEST_DIR/mac_syscalls"

ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/mac_syscalls" read read-locked-file read-locked-volume read-cd \
        read-twice read-missing read-truncate write write-new write-locked \
        append update exclusive hopen-fallback long-name long-leaf \
        rename rename-cross-dir rename-exists rename-exists-no-exchange \
        rename-exists-exchange-fails rename-same-entry \
        rename-exists-busy rename-locked rename-missing rename-other-volume \
        rename-long-name unlink unlink-locked unlink-long-name

# The link keeps these definitions only because XCOFF ld ignores a later
# archive member's redefinition; cmake/static_modules.py fails the Mac build
# unless the map places each one in mac_syscalls.c.obj.  Check that check on
# maps in the Retro68 ld format.
Q3_TEST_LIBRETRO='/opt/Retro68/powerpc-apple-macos/lib/libretrocrt.a(syscalls.c.obj)'
Q3_TEST_OURS='CMakeFiles/Quake3.dir/code/mac/mac_syscalls.c.obj'
q3_test_map() {
    printf 'Linker script and memory map\n\n.text           0x0000000000000000   0x400000\n'
    while [ $# -gt 0 ]; do
        printf ' .pr            0x%016x       0x40 %s\n' "$3" "$2"
        printf '                0x%016x    %s%s\n' "$3" "${4:-               }" "$1"
        shift 4
    done
}
q3_test_overrides() {
    python3 "$Q3_TEST_ROOT/cmake/static_modules.py" overrides --map "$Q3_TEST_DIR/link.map" \
        --override ._open_r=mac_syscalls.c.obj --override ._rename_r=mac_syscalls.c.obj \
        --override ._unlink_r=mac_syscalls.c.obj > /dev/null 2>&1
}
q3_test_map ._open_r "$Q3_TEST_OURS" 4096 '' ._rename_r "$Q3_TEST_OURS" 4160 '' \
    ._unlink_r "$Q3_TEST_OURS" 4224 '' ._rename_r "$Q3_TEST_LIBRETRO" 8192 '-->gc             ' \
    > "$Q3_TEST_DIR/link.map"
q3_test_overrides || { echo "override check refused mac_syscalls.c.obj" >&2; exit 1; }
for Q3_TEST_BAD in libretro both missing; do
    case "$Q3_TEST_BAD" in
        libretro) q3_test_map ._open_r "$Q3_TEST_OURS" 4096 '' ._rename_r "$Q3_TEST_LIBRETRO" 8192 '' \
                      ._unlink_r "$Q3_TEST_OURS" 4224 '' ;;
        both) q3_test_map ._open_r "$Q3_TEST_OURS" 4096 '' ._rename_r "$Q3_TEST_OURS" 4160 '' \
                  ._unlink_r "$Q3_TEST_OURS" 4224 '' ._unlink_r "$Q3_TEST_LIBRETRO" 8192 '' ;;
        missing) q3_test_map ._open_r "$Q3_TEST_OURS" 4096 '' ._rename_r "$Q3_TEST_OURS" 4160 '' ;;
    esac > "$Q3_TEST_DIR/link.map"
    if q3_test_overrides; then
        echo "override check accepted a map with libretro's syscall ($Q3_TEST_BAD)" >&2
        exit 1
    fi
done
echo "libretro override check passed: 1 good and 3 bad maps"
