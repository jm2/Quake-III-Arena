"""Generated RoQ movies in a pk3 for the startup cinematic regression (#14).

video/idlogo.RoQ and video/intro.RoQ stand in for the retail startup movies:
a quad-info chunk, a codebook, then one audio chunk and one VQ frame per
1/30 s, as id's encoder lays them out.  idlogo carries mono audio, intro
stereo audio inside ROQ_PACKET chunks.  video/myintro.RoQ is a mod movie whose
name merely contains "intro".  No game data is written.
"""
from pathlib import Path
import random
import struct
import sys
import zipfile

ROQ_QUAD_INFO = 0x1001
ROQ_CODEBOOK = 0x1002
ROQ_QUAD_VQ = 0x1011
ROQ_PACKET = 0x1030
ZA_SOUND_MONO = 0x1020
ZA_SOUND_STEREO = 0x1021


def chunk(cid, payload, flags=0):
    return struct.pack("<HIH", cid, len(payload), flags) + payload


def vq_frame(rng, width, height, frame):
    # One code word per eight 8x8 blocks: code 2 (vq8 entry) or, on odd
    # frames, alternating with code 0 (skip) so the other buffer half is used.
    out = bytearray()
    blocks = (width // 8) * (height // 8)
    for group in range(0, blocks, 8):
        codes = [2 if (frame % 2 == 0 or i % 2 == 0) else 0 for i in range(8)]
        word = 0
        for c in codes:
            word = (word << 2) | c
        out += struct.pack("<H", word)
        for c in codes:
            if c == 2:
                out.append(rng.randrange(4))
    return bytes(out)


def movie(seed, width, height, frames, stereo):
    rng = random.Random(seed)
    data = bytearray(struct.pack("<HIH", 0x1084, 0xFFFFFFFF, 30))
    data += chunk(ROQ_QUAD_INFO, struct.pack("<HHHH", width, height, 8, 4))
    # Four 2x2 entries and four 4x4 entries built from them.
    book = bytes(rng.randrange(256) for _ in range(4 * 6))
    book += bytes(rng.randrange(4) for _ in range(4 * 4))
    data += chunk(ROQ_CODEBOOK, book, 0x0404)
    for frame in range(frames):
        if stereo:
            audio = chunk(ZA_SOUND_STEREO, bytes(rng.randrange(256) for _ in range(1470)), 0x1234)
            vq = chunk(ROQ_QUAD_VQ, vq_frame(rng, width, height, frame))
            data += chunk(ROQ_PACKET, audio + vq, 2)
        else:
            data += chunk(ZA_SOUND_MONO, bytes(rng.randrange(256) for _ in range(735)), 0x80)
            data += chunk(ROQ_QUAD_VQ, vq_frame(rng, width, height, frame))
    return bytes(data)


def main(directory):
    directory = Path(directory)
    with zipfile.ZipFile(directory / "cin.pk3", "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("video/idlogo.RoQ", movie(14, 256, 128, 45, False))
        z.writestr("video/intro.RoQ", movie(1400, 128, 128, 31, True))
        z.writestr("video/myintro.RoQ", movie(14000, 64, 64, 12, False))


if __name__ == "__main__":
    main(sys.argv[1])
