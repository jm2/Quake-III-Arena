"""Real ZIP32 buffering boundaries and malformed entry size/open/read inputs."""
from pathlib import Path
import struct
import sys
import zipfile


def create(directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(directory / "caps.pk3", "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("native.txt", b"native data\n")
        for name, length in (("below.bin", 32 * 1024 * 1024 - 1),
                             ("at.bin", 32 * 1024 * 1024),
                             ("above.bin", 32 * 1024 * 1024 + 1),
                             ("above.txt", 32 * 1024 * 1024 + 1)):
            with z.open(name, "w") as entry:
                while length:
                    count = min(length, 1024 * 1024)
                    entry.write(bytes(count))
                    length -= count
    for name, declared, fault in (
        ("maximum.pk3", 0x7FFFFFFE, None),
        ("intmax.pk3", 0x7FFFFFFF, None),
        ("signed.pk3", 0x80000000, None),
        ("almost-uintmax.pk3", 0xFFFFFFFE, None),
        ("uintmax.pk3", 0xFFFFFFFF, None),
        ("open-small.pk3", 12, "open"),
        ("open-large.pk3", 32 * 1024 * 1024, "open"),
        ("read-small.pk3", 12, "read"),
        ("read-large.pk3", 32 * 1024 * 1024, "read"),
    ):
        path = directory / name
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as z:
            z.writestr("native.txt", b"native data\n")
            z.writestr("bad.bin", b"native data\n")
        data = bytearray(path.read_bytes())
        local = data.index(b"PK\x03\x04", 4)
        central = data.index(b"PK\x01\x02")
        central = data.index(b"PK\x01\x02", central + 4)
        struct.pack_into("<I", data, local + 22, declared)
        struct.pack_into("<I", data, central + 24, declared)
        if fault == "open":
            data[local:local + 4] = b"BAD!"
        elif fault == "read":
            compressed = struct.unpack_from("<I", data, local + 18)[0] // 2
            struct.pack_into("<I", data, local + 18, compressed)
            struct.pack_into("<I", data, central + 20, compressed)
        path.write_bytes(data)
    # Stored entries. "stored" keeps 6 of the 12 payload bytes as its data but
    # still declares the full length; "overrun" declares, in matching local and
    # central sizes, more data than the archive holds after the local header.
    for name, declared, fault in (
        ("stored-small.pk3", 12, "stored"),
        ("stored-large.pk3", 32 * 1024 * 1024, "stored"),
        ("overrun-small.pk3", 4096, "overrun"),
        ("overrun-large.pk3", 32 * 1024 * 1024, "overrun"),
    ):
        path = directory / name
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as z:
            z.writestr("native.txt", b"native data\n")
            z.writestr("bad.bin", b"native data\n")
        data = bytearray(path.read_bytes())
        local = data.index(b"PK\x03\x04", 4)
        central = data.index(b"PK\x01\x02")
        central = data.index(b"PK\x01\x02", central + 4)
        compressed = 6 if fault == "stored" else declared
        if fault == "overrun" and declared <= len(data) - local:
            raise ValueError("overrun fixture must declare past the archive end")
        struct.pack_into("<II", data, local + 18, compressed, declared)
        struct.pack_into("<II", data, central + 20, compressed, declared)
        path.write_bytes(data)


if __name__ == "__main__":
    create(sys.argv[1])
