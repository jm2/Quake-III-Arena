"""Create ordinary and malformed ZIP32 inputs for actual mount-body tests."""
from pathlib import Path
import io
import struct
import sys
import zipfile


def create(directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)

    def archive(name, entries):
        with zipfile.ZipFile(directory / name, "w") as z:
            for path, data, compression in entries:
                z.writestr(path, data, compress_type=compression)

    compressed = zipfile.ZIP_DEFLATED
    stored = zipfile.ZIP_STORED
    native = [("native.txt", b"native data\n", compressed)]
    archive("native.pk3", native)
    archive("maximum.pk3", [("A" * 255, b"native data\n", compressed),
                            ("native.txt", b"native data\n", stored),
                            ("empty.bin", b"", stored), ("textures/", b"", stored)])
    archive("long.pk3", [("a" * 256, b"native data\n", compressed)])
    archive("prefix-long.pk3", native + [("a" * 256, b"native data\n", compressed)])
    archive("nul.pk3", [("nulXname.txt", b"native data\n", compressed)])
    p = directory / "nul.pk3"
    p.write_bytes(p.read_bytes().replace(b"nulXname.txt", b"nul\0name.txt"))
    for name in ("first.pk3", "count.pk3", "short-name.pk3", "mutable1.pk3",
                 "mutable2.pk3", "mutable3.pk3"):
        (directory / name).write_bytes((directory / "native.pk3").read_bytes())
    p = directory / "first.pk3"
    p.write_bytes(p.read_bytes().replace(b"PK\x01\x02", b"BAD!", 1))
    archive("second.pk3", native + [("second.txt", b"native data\n", compressed)])
    p = directory / "second.pk3"
    data = bytearray(p.read_bytes())
    first = data.index(b"PK\x01\x02")
    second = data.index(b"PK\x01\x02", first + 4)
    data[second:second + 4] = b"BAD!"
    p.write_bytes(data)
    p = directory / "count.pk3"
    data = bytearray(p.read_bytes())
    end = data.rindex(b"PK\x05\x06")
    struct.pack_into("<HH", data, end + 8, 2, 2)
    p.write_bytes(data)
    p = directory / "short-name.pk3"
    data = bytearray(p.read_bytes())
    central = data.index(b"PK\x01\x02")
    struct.pack_into("<H", data, central + 28, 200)
    p.write_bytes(data)
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w"):
        pass
    (directory / "empty.pk3").write_bytes(b"stub" + stream.getvalue())


if __name__ == "__main__":
    create(sys.argv[1])
