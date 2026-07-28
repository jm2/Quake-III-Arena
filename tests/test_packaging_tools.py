import binascii
import contextlib
import io
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

import create_appledouble
import macbinary_encode


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


if __name__ == "__main__":
    unittest.main()
