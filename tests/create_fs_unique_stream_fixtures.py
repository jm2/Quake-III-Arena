"""Crafted pk3s for unique pk3 streaming (#260), plus CRC32/size manifests.

crafted.pk3 holds entries larger than the test's small zone: a 20 MiB
incompressible "RoQ", a 6 MiB compressible "music" track, a stored entry and
many mid-size entries for the concurrent-handle pass.

corrupt.pk3 is written by hand so its deflate stream is valid for exactly the
first half of corrupt.bin and then hits a reserved block type, which zlib
rejects as a data error at a known output offset.
"""
from pathlib import Path
import random
import struct
import sys
import zipfile
import zlib


def manifest(pk3, out):
    with zipfile.ZipFile(pk3) as z, open(out, "w") as m:
        for info in z.infolist():
            if info.is_dir():
                continue
            m.write("%08x %d %s\n" % (zlib.crc32(z.read(info)) & 0xFFFFFFFF,
                                      info.file_size, info.filename))


def pattern(rng, length):
    # Compressible but not trivial: short random runs repeated with drift.
    out = bytearray()
    while len(out) < length:
        run = rng.randbytes(rng.randint(8, 64))
        out += run * rng.randint(4, 40)
    return bytes(out[:length])


def crafted(directory):
    rng = random.Random(260)
    path = directory / "crafted.pk3"
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("video/big.roq", rng.randbytes(20 * 1024 * 1024))
        z.writestr("music/track.wav", pattern(rng, 6 * 1024 * 1024 + 123))
        z.writestr(zipfile.ZipInfo("video/stored.roq"), rng.randbytes(3 * 1024 * 1024 + 7),
                   compress_type=zipfile.ZIP_STORED)
        for i in range(40):
            z.writestr("maps/m%02d.bin" % i, pattern(rng, rng.randint(64 * 1024, 1536 * 1024)))
        for i in range(8):
            z.writestr("scripts/s%d.txt" % i, b"// script %d\n" % i * rng.randint(1, 200))
        z.writestr("empty.dat", b"")
    manifest(path, directory / "crafted.manifest")


def write_zip(path, entries):
    # entries: (name, uncompressed bytes, stored payload, method)
    blob = bytearray()
    central = bytearray()
    for name, plain, payload, method in entries:
        crc = zlib.crc32(plain) & 0xFFFFFFFF
        raw = name.encode()
        offset = len(blob)
        blob += struct.pack("<IHHHHHIIIHH", 0x04034B50, 20, 0, method, 0, 0,
                            crc, len(payload), len(plain), len(raw), 0) + raw + payload
        central += struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, 20, 20, 0, method, 0, 0,
                               crc, len(payload), len(plain), len(raw), 0, 0, 0, 0, 0,
                               offset) + raw
    eocd = struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, len(entries), len(entries),
                       len(central), len(blob), 0)
    path.write_bytes(bytes(blob + central + eocd))


def corrupt(directory):
    rng = random.Random(2600)
    half = 1024 * 1024 + 333
    first, second = pattern(rng, half), pattern(rng, half)
    good = pattern(rng, 512 * 1024)
    comp = zlib.compressobj(9, zlib.DEFLATED, -15)
    head = comp.compress(first) + comp.flush(zlib.Z_FULL_FLUSH)
    tail = comp.compress(second) + comp.flush()
    # BFINAL=1, BTYPE=11 (reserved): an invalid block right after the flush.
    tail = b"\x07" + tail[1:]
    goodComp = zlib.compressobj(9, zlib.DEFLATED, -15)
    goodPayload = goodComp.compress(good) + goodComp.flush()
    write_zip(directory / "corrupt.pk3", [
        ("corrupt.bin", first + second, head + tail, 8),
        ("good.bin", good, goodPayload, 8),
    ])
    with open(directory / "corrupt.manifest", "w") as m:
        m.write("%08x %d corrupt.bin\n" % (zlib.crc32(first + second) & 0xFFFFFFFF, 2 * half))
        m.write("%08x %d good.bin\n" % (zlib.crc32(good) & 0xFFFFFFFF, len(good)))
    # The test checks the decoded prefix against this.
    (directory / "corrupt.prefix").write_bytes(first)


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--manifest":
        manifest(sys.argv[2], sys.argv[3])
    else:
        target = Path(sys.argv[1])
        target.mkdir(parents=True, exist_ok=True)
        crafted(target)
        corrupt(target)
