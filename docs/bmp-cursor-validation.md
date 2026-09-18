# BMP cursor validation — 2026-09-17

The first step for issue #42 moves BMP decoding into a portable renderer
module and replaces unaligned native integer casts with checked byte spans
and little-endian fields. It validates both magic bytes, complete Windows
headers, file length, planes, BI_RGB compression, supported 8/16/24/32-bit
formats, signed dimensions and the RGBA allocation product before allocation.

Palette sizes/indices, pixel offsets and complete padded rows must fit the
file. The 16-bit path consumes input RGB555 words and writes exactly four
RGBA bytes per pixel, expanding five-bit channels to eight. Bottom-up and
top-down orientation, DWORD row padding, RGB555 and palette rules follow
[Microsoft's DIB documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader);
pixel offsets follow the [file-header definition](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader).
Legacy 32-bit alpha bytes remain preserved. Extended Windows headers and
explicit gaps before pixel data are range checked. Outputs publish only on
success; malformed data releases its file before ERR_DROP. CMake's existing
renderer source glob includes the new module in both products. Explicit Unix
Make/Cons, Visual Studio, Xcode and lint lists also include the decoder; the
manifest regression checks every source phase that builds the existing image module.

## Validation

The ASan/UBSan fixture executes the actual file loader with exact input/output
allocations at all four input alignments. Every truncation of small valid
8/16/24/32-bit and extended-header files is rejected with its declared size
updated to the truncated size, so header/palette/payload checks all execute.
Golden images cover row padding/gaps, both orientations, RGB555, alpha,
explicit and default 256-entry palettes, and optional dimension outputs.

Other cases cover either bad magic byte, file-size mismatch, short/extreme
header sizes, unsupported planes/depth/compression, invalid offsets and image
sizes, palette indices/counts, zero/negative/extreme dimensions, allocation
failure and missing files. Rejection asserts zero published dimensions/image,
no unexpected large allocation, and complete ownership before the error
longjmp. Post-error assertions use static storage to avoid indeterminate
nonvolatile automatic variables after longjmp.

The new sanitizer fixture, eight Python tests and Bash syntax pass. Both
Retro68 products build without compiler diagnostics and pass PEF validation
using temporary libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,669,829 | `cd1546507d1f4263782a8e97d9460595ce40899b33c3fa31f41d3a69d319b325` |
| Quake3_TeamArena | 3,822,499 | `bacbcc259b008b375b9855ddae9b1591d0b0e37d1538f96306b297834eb3136f` |

## Remaining acceptance

Keep #42 open. PCX and TGA still need bounded decoding and their malformed
corpora. Retail 1.32c textures/UI and strict-alignment PPC live acceptance
remain explicitly deferred. The JPEG manager/API work is tracked separately
by #43; this BMP step does not establish complete image-loader safety.

After integrating the reviewed QVM work through #64, single-run CI (#68),
and all three RoQ steps including the console shutdown fix, the BMP fixture,
nine Python checks and both Mac products pass. CI retains every runner.
The table records these integrated artifacts.
