#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-config-mutable.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/fs_config_mutable_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/config-mutable-tests"
    # Fresh tree per mode: base holds the retail-style pak0.pk3, home holds the
    # user's loose configs plus pk3s a server could have auto-downloaded.
    Q3_FIXTURE="$Q3_TEST_DIR/$Q3_TEST_MODE"
    mkdir -p "$Q3_FIXTURE/base/baseq3" "$Q3_FIXTURE/home/baseq3/vm" "$Q3_FIXTURE/home/evilmod/vm"
    printf '// local q3config\n' > "$Q3_FIXTURE/home/baseq3/q3config.cfg"
    printf '// local autoexec\n' > "$Q3_FIXTURE/home/baseq3/autoexec.cfg"
    python3 - "$Q3_FIXTURE" <<'PY_ZIP'
import sys, zipfile
root = sys.argv[1]
def pack(path, entries):
    with zipfile.ZipFile(root + path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            archive.writestr(name, data)
pack("/base/baseq3/pak0.pk3", [("default.cfg", b"// retail default\n")])
pack("/home/baseq3/zz_dl.pk3", [("q3config.cfg", b"// hostile q3config\n"),
                                ("autoexec.cfg", b"// hostile autoexec\n"),
                                ("dl.txt", b"downloaded pk3 data\n")])
pack("/home/evilmod/evil.pk3", [("q3config.cfg", b"// hostile mod q3config\n"),
                                ("autoexec.cfg", b"// hostile mod autoexec\n"),
                                ("evil.txt", b"evil pk3 data\n")])
PY_ZIP
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/config-mutable-tests" "$Q3_FIXTURE"
done
