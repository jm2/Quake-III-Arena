#!/usr/bin/env python3
"""Check that game data is id's retail Quake III paks before packaging it.

Usage: pak_identity.py verify GAME DIR
       pak_identity.py checksum PAK...

The install image ships every pak*.pk3 the package scripts find beside a
retail pak0.pk3 (issue #22). "verify" refuses the directory unless it holds
exactly id's paks for GAME: baseq3 pak0.pk3 to pak8.pk3 (the CD and the 1.32
point release), or missionpack pak0.pk3 to pak3.pk3, each with the header
checksum id's engine knows for that file. A demo pak, a pak from another
release, a renamed or edited pak, a missing point-release pak and any extra
pak*.pk3 are all refused.

The checksum is the one the engine computes when it opens a pk3 (files.c,
FS_LoadZipFile): MD4 over the little-endian CRC-32 of every non-empty entry,
in central directory order, folded to 32 bits by XORing the digest's four
little-endian words (md4.c, Com_BlockChecksum). The table is files.c's
FS_IdPakGame, in pak order; tests/test_packaging_tools.py checks they match.
"""
import os
import struct
import sys
import zipfile

STOCK_PAKS = {
    "baseq3": (1566731103, 298122907, 412165236, 2991495316, 1197932710,
               4087071573, 3709064859, 908855077, 977125798),
    "missionpack": (2430342401, 511014160, 2662638993, 1438664554),
}


def _md4(data):
    """MD4 (RFC 1320): hashlib's depends on OpenSSL's legacy provider."""
    def rotl(x, n):
        x &= 0xFFFFFFFF
        return ((x << n) | (x >> (32 - n))) & 0xFFFFFFFF

    length = len(data)
    data = bytes(data) + b"\x80" + b"\0" * ((55 - length) % 64) + struct.pack("<Q", length * 8)
    a, b, c, d = 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476
    for offset in range(0, len(data), 64):
        x = struct.unpack("<16I", data[offset:offset + 64])
        aa, bb, cc, dd = a, b, c, d
        for i in range(16):
            k, s = i, (3, 7, 11, 19)[i % 4]
            f = (b & c) | (~b & d)
            a, b, c, d = d, rotl(a + f + x[k], s), b, c
        for i in range(16):
            k, s = (i % 4) * 4 + i // 4, (3, 5, 9, 13)[i % 4]
            g = (b & c) | (b & d) | (c & d)
            a, b, c, d = d, rotl(a + g + x[k] + 0x5A827999, s), b, c
        for i in range(16):
            k = (0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15)[i]
            s = (3, 9, 11, 15)[i % 4]
            h = b ^ c ^ d
            a, b, c, d = d, rotl(a + h + x[k] + 0x6ED9EBA1, s), b, c
        a = (a + aa) & 0xFFFFFFFF
        b = (b + bb) & 0xFFFFFFFF
        c = (c + cc) & 0xFFFFFFFF
        d = (d + dd) & 0xFFFFFFFF
    return struct.pack("<4I", a, b, c, d)


def block_checksum(data):
    """md4.c's Com_BlockChecksum, as the engine stores it (LittleLong)."""
    words = struct.unpack("<4I", _md4(data))
    return words[0] ^ words[1] ^ words[2] ^ words[3]


def pak_checksum(path):
    """The header checksum FS_LoadZipFile gives the pk3 at path."""
    with zipfile.ZipFile(path) as archive:
        crcs = [info.CRC for info in archive.infolist() if info.file_size > 0]
    return block_checksum(struct.pack("<%dI" % len(crcs), *crcs))


def verify(game, directory):
    """Return a list of problems with directory as GAME's retail paks."""
    if game not in STOCK_PAKS:
        raise ValueError(f"unknown game {game!r}: expected one of {sorted(STOCK_PAKS)}")
    expected = {f"pak{i}.pk3": checksum for i, checksum in enumerate(STOCK_PAKS[game])}
    problems = []
    found = {}
    for name in sorted(os.listdir(directory)):
        lower = name.lower()
        if lower.startswith("pak") and lower.endswith(".pk3"):
            found[lower] = os.path.join(directory, name)
    for name in sorted(found):
        if name not in expected:
            problems.append(f"{found[name]}: not one of id's {game} paks")
            continue
        try:
            checksum = pak_checksum(found[name])
        except (OSError, zipfile.BadZipFile) as exc:
            problems.append(f"{found[name]}: not a readable pk3 ({exc})")
            continue
        if checksum != expected[name]:
            problems.append(f"{found[name]}: checksum {checksum}, but id's {game}/{name} "
                            f"is {expected[name]}")
    for name in sorted(set(expected) - set(found)):
        problems.append(f"{os.path.join(directory, name)}: missing")
    return problems


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) == 3 and argv[0] == "verify":
        try:
            problems = verify(argv[1], argv[2])
        except (OSError, ValueError) as exc:
            print(f"pak_identity.py: {exc}", file=sys.stderr)
            return 1
        for problem in problems:
            print(f"pak_identity.py: {problem}", file=sys.stderr)
        if problems:
            return 1
        print(f"{argv[2]}: id's retail {argv[1]} paks, every checksum verified")
        return 0
    if len(argv) >= 2 and argv[0] == "checksum":
        for path in argv[1:]:
            print(f"{pak_checksum(path)} {path}")
        return 0
    print(__doc__.split("\n\n")[0], file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
