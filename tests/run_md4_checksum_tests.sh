#!/usr/bin/env bash
# md4.c computes retail MD4 on every host (its word type was unsigned long,
# 64 bits on LP64 hosts): RFC 1320's test vectors through the real md4.c, and
# the header and pure checksums the real files.c gives generated pk3s whose
# entry CRC-32s make their header checksums retail baseq3/pak0.pk3's and
# missionpack/pak3.pk3's.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-md4-checksum.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 - "$Q3_TEST_DIR" <<'PY_ZIP'
import sys, zipfile, zlib

def forge(crc):
    """Four bytes whose CRC-32 is crc (CRC-32 is affine in a fixed-length input)."""
    base = zlib.crc32(b"\0\0\0\0")
    rows = [(zlib.crc32((1 << bit).to_bytes(4, "little")) ^ base, 1 << bit) for bit in range(32)]
    want, result = crc ^ base, 0
    for i in range(32):
        pivot = next(j for j in range(i, 32) if rows[j][0] >> i & 1)
        rows[i], rows[pivot] = rows[pivot], rows[i]
        for j in range(32):
            if j != i and rows[j][0] >> i & 1:
                rows[j] = (rows[j][0] ^ rows[i][0], rows[j][1] ^ rows[i][1])
    for i in range(32):
        if want >> i & 1:
            result ^= rows[i][1]
    data = result.to_bytes(4, "little")
    assert zlib.crc32(data) == crc
    return data

# The CRC lists of tests/run_vm_static_qvm_tests.sh's forged id paks; an empty
# entry adds no CRC, as in FS_LoadZipFile.
for name, crcs in (("pak0.pk3", (0x620d2ba6, 0x1e6c0e7d, 0x8363ef0b, 1, 0x78aa8e5f)),
                   ("pak3.pk3", (0xcd7b1be4, 0xb11a3e3f, 0x2c15df49, 4, 0xac6d28fb))):
    with zipfile.ZipFile(sys.argv[1] + "/" + name, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("empty.dat", b"")
        for i, crc in enumerate(crcs):
            archive.writestr("entry%d.dat" % i, forge(crc))
PY_ZIP
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/md4_checksum_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/md4-tests"
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/md4-tests" "$Q3_TEST_DIR"
done
