"""Complete ZIP32 entries for active-stream decoder and physical cursor checks."""
import sys
import zipfile


def create(path):
    with zipfile.ZipFile(path, "w") as archive:
        for name, compression, block in (
            ("large.bin", zipfile.ZIP_DEFLATED, bytes(1024 * 1024)),
            ("stored.bin", zipfile.ZIP_STORED, bytes(range(256)) * 4096),
        ):
            info = zipfile.ZipInfo(name)
            info.compress_type = compression
            with archive.open(info, "w") as entry:
                for _ in range(32):
                    entry.write(block)
        archive.writestr("native.txt", b"native data\n", compress_type=zipfile.ZIP_DEFLATED)


if __name__ == "__main__":
    create(sys.argv[1])
