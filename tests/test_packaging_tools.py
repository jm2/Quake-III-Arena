import binascii
import contextlib
from datetime import datetime, timedelta, timezone
import io
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
import zlib

import create_appledouble
import generate_icon_r
import mac_app
import macbinary_encode

REPO = Path(__file__).resolve().parent.parent


class AppleDoubleTests(unittest.TestCase):
    def test_resource_and_finder_metadata_layout(self):
        resource = bytes(range(32))

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            resource_path = temp_path / "Quake3.rsrc"
            output_path = temp_path / "%Quake3"
            resource_path.write_bytes(resource)

            with contextlib.redirect_stdout(io.StringIO()):
                create_appledouble.create_appledouble(
                    str(resource_path), str(output_path))

            blob = output_path.read_bytes()

        self.assertEqual(struct.unpack_from(">I", blob, 0)[0], 0x00051607)
        self.assertEqual(struct.unpack_from(">I", blob, 4)[0], 0x00020000)
        self.assertEqual(struct.unpack_from(">H", blob, 24)[0], 2)

        finder_id, finder_offset, finder_length = struct.unpack_from(
            ">III", blob, 26)
        resource_id, resource_offset, resource_length = struct.unpack_from(
            ">III", blob, 38)
        self.assertEqual((finder_id, finder_offset, finder_length), (9, 50, 32))
        self.assertEqual(
            (resource_id, resource_offset, resource_length),
            (2, 82, len(resource)),
        )

        finder_info = blob[finder_offset:finder_offset + finder_length]
        self.assertEqual(finder_info[0:4], b"APPL")
        self.assertEqual(finder_info[4:8], b"IDQ3")
        self.assertEqual(struct.unpack_from(">H", finder_info, 8)[0], 0x2000)
        self.assertEqual(blob[resource_offset:], resource)


class MacBinaryTests(unittest.TestCase):
    def test_deterministic_header_forks_and_crc(self):
        data = b"data fork"
        resource = b"resource fork"
        source_date_epoch = 946684800
        expected_mac_time = (
            source_date_epoch + macbinary_encode.MAC_EPOCH_OFFSET)

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            input_path = temp_path / "Quake3.img"
            resource_path = temp_path / "Quake3.rsrc"
            output_path = temp_path / "Quake3.img.bin"
            input_path.write_bytes(data)
            resource_path.write_bytes(resource)

            with mock.patch.dict(
                    os.environ,
                    {"SOURCE_DATE_EPOCH": str(source_date_epoch)},
                    clear=False):
                with contextlib.redirect_stdout(io.StringIO()):
                    macbinary_encode.encode_macbinary(
                        str(input_path),
                        str(output_path),
                        "iso ",
                        "dCpy",
                        str(resource_path),
                    )

            blob = output_path.read_bytes()

        header = blob[:128]
        name_length = header[1]
        self.assertEqual(header[2:2 + name_length], b"Quake3.img")
        self.assertEqual(header[65:69], b"iso ")
        self.assertEqual(header[69:73], b"dCpy")
        self.assertEqual(struct.unpack_from(">I", header, 83)[0], len(data))
        self.assertEqual(
            struct.unpack_from(">I", header, 87)[0], len(resource))
        self.assertEqual(
            struct.unpack_from(">I", header, 91)[0], expected_mac_time)
        self.assertEqual(
            struct.unpack_from(">I", header, 95)[0], expected_mac_time)
        self.assertEqual(header[122:124], b"\x81\x81")
        self.assertEqual(
            struct.unpack_from(">H", header, 124)[0],
            binascii.crc_hqx(header[:124], 0),
        )

        data_offset = 128
        resource_offset = data_offset + ((len(data) + 127) // 128) * 128
        self.assertEqual(blob[data_offset:data_offset + len(data)], data)
        self.assertEqual(
            blob[resource_offset:resource_offset + len(resource)], resource)
        self.assertEqual(
            len(blob),
            128
            + ((len(data) + 127) // 128) * 128
            + ((len(resource) + 127) // 128) * 128,
        )

    def test_filename_length_counts_encoded_bytes_and_caps_at_63(self):
        long_name = "\N{LATIN SMALL LETTER E WITH ACUTE}" * 64

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            input_path = temp_path / long_name
            output_path = temp_path / "output.bin"
            input_path.write_bytes(b"x")

            with contextlib.redirect_stdout(io.StringIO()):
                macbinary_encode.encode_macbinary(
                    str(input_path), str(output_path), "TEXT", "IDQ3")

            header = output_path.read_bytes()[:128]

        expected_name = long_name.encode("mac_roman")[:63]
        self.assertEqual(header[1], 63)
        self.assertEqual(header[2:65], expected_name)

    def test_unrepresentable_filename_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            input_path = temp_path / "quake-\N{GRINNING FACE}.img"
            output_path = temp_path / "output.bin"
            input_path.write_bytes(b"x")

            with self.assertRaisesRegex(ValueError, "not representable"):
                macbinary_encode.encode_macbinary(
                    str(input_path), str(output_path), "iso ", "dCpy")

            self.assertFalse(output_path.exists())

    def encode(self, name, resource=None, source_date_epoch=946684800):
        """Encode a one-byte data fork named name; return the .bin path."""
        temp_path = Path(self.enterContext(tempfile.TemporaryDirectory()))
        input_path = temp_path / name
        input_path.write_bytes(b"x")
        resource_path = None
        if resource is not None:
            resource_path = temp_path / "fork.rsrc"
            resource_path.write_bytes(resource)
        output_path = temp_path / "output.bin"
        with mock.patch.dict(
                os.environ, {"SOURCE_DATE_EPOCH": str(source_date_epoch)}):
            with contextlib.redirect_stdout(io.StringIO()):
                macbinary_encode.encode_macbinary(
                    str(input_path), str(output_path), "APPL", "IDQ3",
                    resource_path and str(resource_path))
        return output_path

    def test_issue_52_name_length_is_encoded_bytes(self):
        """Issue #52: header[1] counts MacRoman bytes, capped at 63."""
        cases = {
            "q" * 63: b"q" * 63,
            "q" * 64: b"q" * 63,
            "Quake III \N{LATIN SMALL LETTER E WITH ACUTE}t\N{TRADE MARK SIGN}":
                b"Quake III \x8et\xaa",
            "\N{BULLET}" * 62 + "ab": b"\xa5" * 62 + b"a",
        }
        for name, expected in cases.items():
            with self.subTest(name=name):
                header = self.encode(name).read_bytes()[:128]
                self.assertEqual(header[1], len(expected))
                self.assertEqual(header[2:2 + header[1]], expected)
                self.assertEqual(header[2 + header[1]:65],
                                 bytes(63 - len(expected)))
                self.assertEqual(mac_app.read_macbinary(
                    str(self.encode(name))).name,
                    expected.decode("mac_roman"))

    def test_issue_32_dates_are_valid_mac_epoch_times(self):
        """Issue #32: both dates are nonzero seconds since 1904-01-01."""
        epoch = datetime(1904, 1, 1, tzinfo=timezone.utc)
        for unix_time in (0, 946684800, 2082844799):
            with self.subTest(unix_time=unix_time):
                header = self.encode("Quake3", source_date_epoch=unix_time
                                     ).read_bytes()[:128]
                created, modified = struct.unpack_from(">II", header, 91)
                self.assertEqual(created, modified)
                self.assertEqual(
                    epoch + timedelta(seconds=created),
                    datetime.fromtimestamp(unix_time, timezone.utc))
        with self.assertRaisesRegex(ValueError, "date range"):
            self.encode("Quake3", source_date_epoch=2082844800 + 2 ** 31 * 2)

    @unittest.skipUnless(shutil.which("lsar"), "The Unarchiver's lsar is not installed")
    def test_independent_reader_accepts_header(self):
        """Issues #32/#52: lsar decodes name, codes, forks and dates."""
        name = "Quake III Arena \N{LATIN SMALL LETTER E WITH ACUTE}" + "x" * 50
        output = self.encode(name, resource=b"resource fork")
        listing = json.loads(subprocess.run(
            ["lsar", "-j", str(output)], check=True, capture_output=True,
            text=True).stdout)
        self.assertEqual(listing["lsarFormatName"], "MacBinary")
        entries = listing["lsarContents"]
        self.assertEqual([entry.get("XADIsResourceFork", 0) for entry in entries],
                         [0, 1])
        expected_time = datetime(2000, 1, 1, tzinfo=timezone.utc)
        for entry, size in zip(entries, (1, len(b"resource fork"))):
            self.assertEqual(entry["XADFileName"],
                             name.encode("mac_roman")[:63].decode("mac_roman"))
            self.assertEqual(entry["XADFileSize"], size)
            self.assertEqual(struct.pack(">II", entry["XADFileType"],
                                         entry["XADFileCreator"]), b"APPLIDQ3")
            for field in ("XADCreationDate", "XADLastModificationDate"):
                # Classic dates are local wall-clock time; allow any zone.
                date = datetime.strptime(entry[field], "%Y-%m-%d %H:%M:%S %z")
                self.assertLess(abs(date - expected_time), timedelta(hours=26))


def decode_png_rgba(path):
    """8-bit RGBA rows of a non-interlaced RGBA PNG, without Pillow.

    16-bit samples keep their high byte, as Pillow's RGBA conversion does.
    """
    blob = Path(path).read_bytes()
    assert blob[:8] == b"\x89PNG\r\n\x1a\n"
    offset, idat = 8, b""
    while offset < len(blob):
        length, kind = struct.unpack_from(">I4s", blob, offset)
        body = blob[offset + 8:offset + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(
                ">IIBBBBB", body)
            assert depth in (8, 16) and (color, interlace) == (6, 0)
        elif kind == b"IDAT":
            idat += body
        offset += 12 + length
    pixel_size = depth // 2
    raw, stride, rows, previous = zlib.decompress(idat), width * pixel_size, [], None
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        previous = previous or bytearray(stride)
        for x in range(stride):
            a = line[x - pixel_size] if x >= pixel_size else 0
            b = previous[x]
            c = previous[x - pixel_size] if x >= pixel_size else 0
            if kind == 1:
                line[x] = (line[x] + a) & 0xFF
            elif kind == 2:
                line[x] = (line[x] + b) & 0xFF
            elif kind == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                predictor = a if pa <= pb and pa <= pc else b if pb <= pc else c
                line[x] = (line[x] + predictor) & 0xFF
        rows.append([tuple(line[x:x + pixel_size:depth // 8])
                     for x in range(0, stride, pixel_size)])
        previous = line
    return rows


def rez_data(source, resource_type):
    """Bytes of a data 'TYPE' (...) { $"..." }; resource in a .r file."""
    body = re.search(r"data '%s' \(128, \"Quake3\"\) \{(.*?)\};"
                     % re.escape(resource_type), source, re.S).group(1)
    return bytes.fromhex("".join(re.findall(r'\$"([0-9A-F]*)"', body)))


class ClassicIconTests(unittest.TestCase):
    """Issue #31: icl8/ics8 index the standard 8-bit system palette."""

    def test_system_palette_matches_clut_8(self):
        clut = generate_icon_r.SYSTEM_CLUT8
        self.assertEqual(len(set(clut)), 256)
        anchors = {0: (0xFF, 0xFF, 0xFF), 5: (0xFF, 0xFF, 0x00),
                   35: (0xFF, 0x00, 0x00), 38: (0xCC, 0xFF, 0x99),
                   210: (0x00, 0x00, 0xFF), 214: (0x00, 0x00, 0x33),
                   215: (0xEE, 0x00, 0x00), 224: (0x11, 0x00, 0x00),
                   225: (0x00, 0xEE, 0x00), 235: (0x00, 0x00, 0xEE),
                   245: (0xEE, 0xEE, 0xEE), 254: (0x11, 0x11, 0x11),
                   255: (0x00, 0x00, 0x00)}
        for index, rgb in anchors.items():
            self.assertEqual(clut[index], rgb, index)
        # Exact colors map to themselves; transparency blends to white.
        self.assertEqual(
            [generate_icon_r.system_index(pixel) for pixel in (
                (0, 0, 0, 0), (0xCC, 0x33, 0x99, 255), (0xEE, 0, 0, 255),
                (0, 0, 0, 255), (0x80, 0, 0, 0x80))],
            [0, 62, 215, 255, 57])

    def test_committed_icons_use_system_palette(self):
        source = (REPO / "code/mac/quake3_icons.r").read_text()
        for resource_type, png in (("icl8", "quake3_32.png"),
                                   ("ics8", "quake3_16.png")):
            with self.subTest(resource=resource_type):
                rows = decode_png_rgba(REPO / "code/mac" / png)
                pixels = [pixel for row in rows for pixel in row]
                indexes = rez_data(source, resource_type)
                self.assertEqual(len(indexes), len(pixels))
                self.assertEqual(
                    indexes,
                    bytes(generate_icon_r.system_index(p) for p in pixels))
                # Independently of the generator: each opaque pixel is drawn
                # within one color-cube step of the source color.
                for (r, g, b, a), index in zip(pixels, indexes):
                    if a == 255:
                        drawn = generate_icon_r.SYSTEM_CLUT8[index]
                        error = sum((s - d) ** 2 for s, d in zip((r, g, b), drawn))
                        self.assertLessEqual(error, 3 * 26 ** 2,
                                             ((r, g, b), drawn))


def mapping_entries(lines):
    """(name, translation, creator, type) for each genisoimage map line."""
    entries = []
    for line in lines:
        match = re.fullmatch(
            r"(\S+)\s+(Raw|Ascii)\s+'(.{4})'\s+'(.{4})'\s+(\"[^\"]*\"|'[^']*')",
            line)
        if not match:
            raise AssertionError(f"type/creator codes not quoted: {line!r}")
        entries.append(match.groups()[:4])
    return entries


def shell_mapping_lines():
    script = (REPO / "build_mac.sh").read_text(encoding="utf-8")
    return re.search(r'cat > "\$MAPPING_FILE" <<EOF\n(.*?)\nEOF\n',
                     script, re.S).group(1).splitlines()


def powershell_mapping_lines():
    script = (REPO / "build_mac.ps1").read_text()
    block = re.search(r"Set-Content -Path \$MappingFile -Value \((.*?)\n    \)",
                      script, re.S).group(1)
    return re.findall(r'^\s*"(.*)",?$', block, re.M)


class HfsMappingTests(unittest.TestCase):
    """Issue #299: genisoimage map lines quote every type/creator code."""

    EXPECTED = [(".pk3", "Raw", "IDQ3", "Stak"),
                (".cfg", "Ascii", "IDQ3", "TEXT"),
                ("Quake3", "Raw", "IDQ3", "APPL"),
                ("Quake3_TeamArena", "Raw", "IDQ3", "APPL")]

    def test_bash_and_powershell_maps_quote_codes(self):
        for lines in (shell_mapping_lines(), powershell_mapping_lines()):
            self.assertEqual(mapping_entries(lines), self.EXPECTED)

    @unittest.skipUnless(
        shutil.which("genisoimage") and shutil.which("hmount"),
        "genisoimage and hfsutils are not installed")
    def test_genisoimage_applies_mapping(self):
        temp_path = Path(self.enterContext(tempfile.TemporaryDirectory()))
        content = temp_path / "content"
        (content / "baseq3").mkdir(parents=True)
        for path in ("baseq3/pak0.pk3", "q3config.cfg", "Quake3"):
            (content / path).write_bytes(b"x")
        mapping = temp_path / "hfs_mapping.txt"
        image = temp_path / "Quake3_Install.img"
        for name, lines in (("bash", shell_mapping_lines()),
                            ("powershell", powershell_mapping_lines())):
            with self.subTest(script=name):
                mapping.write_text("\n".join(lines) + "\n")
                subprocess.run(
                    ["genisoimage", "-quiet", "-hfs", "-double", "-map",
                     str(mapping), "-o", str(image), "-V", "Quake 3 Arena",
                     str(content)], check=True, capture_output=True)
                # hfsutils keeps its mounted-volume state in $HOME/.hcwd.
                env = dict(os.environ, HOME=str(temp_path))
                listing = ""
                subprocess.run(["hmount", str(image)], env=env, check=True,
                               capture_output=True)
                try:
                    for folder in (":", ":baseq3"):
                        listing += subprocess.run(
                            ["hls", "-l", folder], env=env, check=True,
                            capture_output=True, text=True).stdout
                finally:
                    subprocess.run(["humount"], env=env, capture_output=True)
                codes = {name: code for code, name in re.findall(
                    r"^f  (.{9})\s.*\s(\S+)$", listing, re.M)}
                self.assertEqual(codes, {"pak0.pk3": "Stak/IDQ3",
                                         "q3config.cfg": "TEXT/IDQ3",
                                         "Quake3": "APPL/IDQ3"}, listing)


class CreatorConsistencyTests(unittest.TestCase):
    """Issue #9: one Finder creator (retail IDQ3) signs the BNDL everywhere."""

    def test_bundle_signature_matches_every_creator(self):
        bndl = re.compile(r"resource 'BNDL' \(128\) \{\s*'(.{4})',")
        for path in ("code/mac/quake3_icons.r", "generate_icon_r.py"):
            with self.subTest(path=path):
                self.assertEqual(bndl.findall((REPO / path).read_text()),
                                 ["IDQ3"])
        icons = (REPO / "code/mac/quake3_icons.r").read_text()
        self.assertRegex(icons, r"resource 'FREF' \(128\) \{\s*'APPL',\s*0,")
        self.assertIn("-t APPL -c IDQ3", (REPO / "CMakeLists.txt").read_text())
        self.assertEqual(mac_app.verify.__defaults__[1:3], (b"APPL", b"IDQ3"))
        for entry in mapping_entries(shell_mapping_lines()):
            self.assertEqual(entry[2], "IDQ3")


PEF = b"Joy!peffpwpc" + bytes(range(256)) * 2


def build_cfrg(architecture=b"pwpc", usage=1, location=1):
    name = b"\x06Quake3"
    member_size = (42 + len(name) + 3) & ~3
    member = struct.pack(">4sIIIIHBBIIIIH", architecture, 0, 0, 0, 1 << 20, 0,
                         usage, location, 0, 0, 0, 0, member_size) + name
    return (struct.pack(">IIIIIIII", 0, 0, 1, 0, 0, 0, 0, 1)
            + member.ljust(member_size, b"\0"))


def build_resource_fork(resources):
    """Resource fork bytes for {type: [(id, name or None, data), ...]}."""
    data, names, references, type_entries = bytearray(), bytearray(), bytearray(), []
    type_list_length = 2 + 8 * len(resources)
    for resource_type, items in resources.items():
        type_entries.append(struct.pack(
            ">4sHH", resource_type.encode("mac_roman"), len(items) - 1,
            type_list_length + len(references)))
        for resource_id, name, body in items:
            name_offset = 0xFFFF
            if name is not None:
                name_offset = len(names)
                names += bytes([len(name)]) + name.encode("mac_roman")
            references += struct.pack(">hHII", resource_id, name_offset, len(data), 0)
            data += struct.pack(">I", len(body)) + body
    type_list = (struct.pack(">H", (len(resources) - 1) & 0xFFFF)
                 + b"".join(type_entries) + references)
    data_offset, map_offset = 256, 256 + len(data)
    map_length = 28 + len(type_list) + len(names)
    header = struct.pack(">IIII", data_offset, map_offset, len(data), map_length)
    resource_map = (header + bytes(8) + struct.pack(">HH", 28, 28 + len(type_list))
                    + type_list + names)
    return header + bytes(data_offset - 16) + data + resource_map


def launchable_resources(**overrides):
    resources = {
        "cfrg": [(0, None, build_cfrg())],
        "SIZE": [(-1, None, bytes(10))],
        "BNDL": [(128, None, b"IDQ3" + bytes(8))],
        "FREF": [(128, None, b"APPL" + bytes(3))],
        "ICN#": [(128, "Quake3", bytes(256))],
        "ics#": [(128, "Quake3", bytes(64))],
        "icl8": [(128, "Quake3", bytes(1024))],
        "ics8": [(128, "Quake3", bytes(256))],
    }
    for resource_type, items in overrides.items():
        if items:
            resources[resource_type] = items
        else:
            del resources[resource_type]
    return build_resource_fork(resources)


def rez_macbinary(name, data, resource_fork, flags=0):
    """A MacBinary header laid out the way Retro68's Rez writes it."""
    header = bytearray(128)
    header[1] = len(name)
    header[2:2 + len(name)] = name.encode("mac_roman")
    header[65:73] = b"APPLIDQ3"
    header[73] = flags >> 8
    struct.pack_into(">II", header, 83, len(data), len(resource_fork))
    struct.pack_into(">H", header, 124, binascii.crc_hqx(bytes(header[:124]), 0))
    return (bytes(header) + data.ljust((len(data) + 127) & ~127, b"\0")
            + resource_fork.ljust((len(resource_fork) + 127) & ~127, b"\0"))


def rez_appledouble(resource_fork):
    """A %Name AppleDouble header laid out the way Retro68's Rez writes it."""
    finder_offset = 50 + len(resource_fork)
    return (struct.pack(">II16sH", 0x00051607, 0x00020000, bytes(16), 2)
            + struct.pack(">III", 2, 50, len(resource_fork))
            + struct.pack(">III", 9, finder_offset, 32)
            + resource_fork + b"APPLIDQ3" + bytes(24))


def hfs_image(name, data, resource_fork, flags=0, parent_id=2):
    """A minimal HFS volume: one catalog leaf holding one file record."""
    block, first_sector, catalog_blocks = 512, 4, 2
    data_blocks = max(1, -(-len(data) // block))
    resource_start = catalog_blocks + data_blocks
    total_blocks = resource_start + max(1, -(-len(resource_fork) // block))
    image = bytearray(first_sector * 512 + total_blocks * block)
    struct.pack_into(">2s", image, 1024, b"BD")
    struct.pack_into(">I", image, 1024 + 0x14, block)
    struct.pack_into(">H", image, 1024 + 0x1C, first_sector)
    struct.pack_into(">IHH", image, 1024 + 0x92, catalog_blocks * block, 0,
                     catalog_blocks)

    catalog = first_sector * 512
    struct.pack_into(">IIBBHH", image, catalog, 0, 0, 1, 0, 3, 0)
    struct.pack_into(">HIIIIHHII", image, catalog + 14, 1, 1, 1, 1, 1, block, 37, 2, 0)

    encoded = name.encode("mac_roman")
    key = struct.pack(">BBIB", 6 + len(encoded), 0, parent_id, len(encoded)) + encoded
    key += b"\0" * (len(key) & 1)
    record = bytearray(102)
    record[0] = 2
    record[4:12] = b"APPLIDQ3"
    struct.pack_into(">H", record, 12, flags)
    struct.pack_into(">I", record, 26, len(data))
    struct.pack_into(">I", record, 36, len(resource_fork))
    struct.pack_into(">HH", record, 74, catalog_blocks, data_blocks)
    struct.pack_into(">HH", record, 86, resource_start, total_blocks - resource_start)
    leaf = catalog + block
    struct.pack_into(">IIBBHH", image, leaf, 0, 0, 0xFF, 1, 1, 0)
    image[leaf + 14:leaf + 14 + len(key) + len(record)] = key + record
    struct.pack_into(">HH", image, leaf + block - 4, 14 + len(key) + len(record), 14)

    data_offset = catalog + catalog_blocks * block
    image[data_offset:data_offset + len(data)] = data
    resource_offset = catalog + resource_start * block
    image[resource_offset:resource_offset + len(resource_fork)] = resource_fork
    return bytes(image)


def run_mac_app(*argv):
    stdout, stderr = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        status = mac_app.main([str(argument) for argument in argv])
    return status, stdout.getvalue(), stderr.getvalue()


class ClassicApplicationTests(unittest.TestCase):
    """Issues #225/#226: every build yields a launchable APPL/IDQ3 application."""

    def setUp(self):
        temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(temp_dir.cleanup)
        self.path = Path(temp_dir.name)
        self.pef = self.path / "Quake3.pef"
        self.pef.write_bytes(PEF)

    def write_rez_outputs(self, resource_fork):
        """Write Rez's MacBinary, HFS and AppleDouble outputs (no flags set)."""
        outputs = (self.path / "Quake3.bin", self.path / "Quake3.dsk",
                   self.path / "%Quake3.ad")
        outputs[0].write_bytes(rez_macbinary("Quake3", PEF, resource_fork))
        outputs[1].write_bytes(hfs_image("Quake3", PEF, resource_fork))
        outputs[2].write_bytes(rez_appledouble(resource_fork))
        (self.path / "Quake3.ad").write_bytes(PEF)
        return outputs

    def test_finalize_sets_bundle_flag_in_every_container(self):
        resource_fork = launchable_resources()
        outputs = self.write_rez_outputs(resource_fork)

        status, stdout, stderr = run_mac_app(
            "finalize", "--pef", self.pef, *outputs)

        self.assertEqual(status, 0, stderr)
        self.assertEqual(stdout.count("APPL/IDQ3, flags 0x2000"), 3)
        for output in outputs:
            with self.subTest(container=output.name):
                app = mac_app.load(str(output))
                self.assertEqual((app.file_type, app.creator), (b"APPL", b"IDQ3"))
                self.assertEqual(app.flags, mac_app.K_HAS_BUNDLE)
                self.assertEqual(app.data, PEF)
                self.assertEqual(app.resource_fork, resource_fork)

        header = outputs[0].read_bytes()[:128]
        self.assertEqual(header[73], 0x20)
        self.assertEqual(struct.unpack_from(">H", header, 124)[0],
                         binascii.crc_hqx(header[:124], 0))
        self.assertEqual(run_mac_app("verify", "--pef", self.pef, *outputs)[0], 0)

    def test_finalize_removes_incomplete_outputs(self):
        outputs = self.write_rez_outputs(launchable_resources(BNDL=[]))

        status, _, stderr = run_mac_app("finalize", "--pef", self.pef, *outputs)

        self.assertEqual(status, 1)
        self.assertIn("missing or empty 'BNDL' resource", stderr)
        for output in outputs + (self.path / "Quake3.ad",):
            self.assertFalse(output.exists(), output.name)

    def test_verify_names_each_launch_problem(self):
        def app(resource_fork=None, data=PEF, file_type=b"APPL",
                flags=mac_app.K_HAS_BUNDLE):
            return mac_app.MacFile("Quake3", file_type, b"IDQ3", flags, data,
                                   resource_fork or launchable_resources())

        self.assertEqual(mac_app.verify(app(), PEF), [])
        cases = {
            "type/creator is TEXT/IDQ3": app(file_type=b"TEXT"),
            "differs from the PEF": app(data=PEF + b"x"),
            "not a PowerPC PEF": app(data=b"\0" * 16),
            "missing or empty 'icl8'": app(launchable_resources(
                icl8=[(128, None, b"")])),
            "missing or empty 'FREF'": app(launchable_resources(FREF=[])),
            "does not describe a PowerPC application": app(launchable_resources(
                cfrg=[(0, None, build_cfrg(usage=0))])),
            "no 'SIZE' (-1) or (0)": app(launchable_resources(
                SIZE=[(128, None, bytes(10))])),
            "signature 'Q3A0' does not match": app(launchable_resources(
                BNDL=[(128, None, b"Q3A0" + bytes(8))])),
            "kHasBundle Finder flag is clear": app(flags=0),
        }
        for message, mac_file in cases.items():
            with self.subTest(message=message):
                problems = mac_app.verify(mac_file, PEF)
                self.assertTrue(any(message in problem for problem in problems),
                                problems)

    def test_resource_fork_parser_reads_names_and_rejects_truncation(self):
        fork = launchable_resources()
        resources = mac_app.parse_resource_fork(fork)

        self.assertEqual(sorted(resources), sorted(mac_app.DEFAULT_REQUIRED))
        self.assertEqual(resources["ICN#"][128], ("Quake3", bytes(256)))
        self.assertEqual(resources["SIZE"][-1], (None, bytes(10)))
        self.assertEqual(mac_app.parse_resource_fork(b""), {})
        with self.assertRaises(mac_app.FormatError):
            mac_app.parse_resource_fork(fork[:-40])

    def test_repository_macbinary_encoder_round_trips(self):
        data_path = self.path / "Quake3"
        resource_path = self.path / "Quake3.rsrc"
        output = self.path / "Quake3.bin"
        data_path.write_bytes(PEF)
        resource_path.write_bytes(launchable_resources())
        with contextlib.redirect_stdout(io.StringIO()):
            macbinary_encode.encode_macbinary(
                str(data_path), str(output), "APPL", "IDQ3", str(resource_path))

        app = mac_app.read_macbinary(str(output))
        self.assertEqual((app.name, app.data), ("Quake3", PEF))
        self.assertEqual(app.resource_fork, resource_path.read_bytes())

        corrupted = bytearray(output.read_bytes())
        corrupted[70] ^= 0xFF
        output.write_bytes(corrupted)
        with self.assertRaisesRegex(mac_app.FormatError, "CRC mismatch"):
            mac_app.read_macbinary(str(output))

    def test_hfs_image_finds_application_in_a_folder(self):
        image = self.path / "Quake3_Install.img"
        resource_fork = launchable_resources()
        image.write_bytes(hfs_image("Quake3", PEF, resource_fork, parent_id=16))

        app = mac_app.load(str(image), hfs_name="Quake3")
        self.assertEqual((app.data, app.resource_fork), (PEF, resource_fork))
        with self.assertRaisesRegex(mac_app.FormatError, "not found"):
            mac_app.load(str(image), hfs_name="Quake3_TeamArena")
        with self.assertRaisesRegex(mac_app.FormatError, "not found"):
            mac_app.load(str(image))

        image.write_bytes(bytes(4096))
        with self.assertRaisesRegex(mac_app.FormatError, "not an HFS volume"):
            mac_app.load(str(image), hfs_name="Quake3")

    def test_exports_resource_fork_and_finder_info_for_real_forks(self):
        resource_fork = launchable_resources()
        application = self.path / "Quake3.bin"
        application.write_bytes(rez_macbinary(
            "Quake3", PEF, resource_fork, flags=mac_app.K_HAS_BUNDLE))
        exported = self.path / "Quake3.rsrc"

        self.assertEqual(run_mac_app("resource-fork", application, exported)[0], 0)
        self.assertEqual(exported.read_bytes(), resource_fork)
        status, stdout, _ = run_mac_app("finder-info", application)
        self.assertEqual(status, 0)
        self.assertEqual(stdout.strip(), ("4150504C" "49445133" "2000").ljust(64, "0"))

        status, _, stderr = run_mac_app(
            "resource-fork", self.path / "Quake3.hqx", self.path / "none.rsrc")
        self.assertEqual(status, 1)
        self.assertIn("unknown container", stderr)
        self.assertFalse((self.path / "none.rsrc").exists())

        for argv in (("verify", self.path / "missing.bin"),
                     ("verify", "--pef", self.path / "missing.pef", application)):
            with self.subTest(argv=argv):
                status, _, stderr = run_mac_app(*argv)
                self.assertEqual(status, 1)
                self.assertIn("No such file", stderr)


if __name__ == "__main__":
    unittest.main()
