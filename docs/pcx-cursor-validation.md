# PCX cursor validation — 2026-09-17

The second step for issue #42 replaces the two-stage PCX loader with a
portable renderer module and one RGBA allocation. It checks the complete
128-byte version-5 header, single 8-bit plane, ordered origin/extents,
1024-pixel axis cap, scanline byte count, and complete 256-color palette.
The palette marker requirement follows [ioquake3's PCX loader](https://github.com/ioquake/ioq3/blob/main/code/renderercommon/tr_image_pcx.c).
Markerless files are rejected; the retail-format marker separates all
compressed reads from the palette. No retail asset corpus was available.

Both validation and decoding use checked byte spans. The first pass validates
every literal/run and complete scanline, including padding, before allocating.
Zero-length runs, missing values and runs crossing scanline boundaries are
rejected. The second pass writes only visible pixels with native opaque RGBA
palette colors. Nonzero origins and padded scanlines use their declared image
extents. The original 1024×1024 maximum bounds RGBA allocation to 4 MiB.

Malformed PCX remains a nonfatal warning. All file ownership is released before
the warning or publication of dimensions/pixels, and missing files remain quiet.
CMake, Unix Make/Cons, Visual Studio, all four Xcode renderer source phases and
the lint list include the new module. The build-manifest regression covers it.

## Validation

The ASan/UBSan fixture invokes the actual loader with an exact file end at all
four input alignments and an exact output allocation. Every prefix of small
valid files is rejected. Golden images cover literals, repeated and escaped
high-byte colors, all 256 palette entries, nonzero origins, padding within a
run, a maximum 65535-byte scanline, optional dimension outputs and the exact
1024×1024 limit.

Malformed cases cover zero or oversized runs, runs crossing a row, missing
literal/run values or padding, unsupported formats/planes, zero stride,
reversed and extreme extents, a missing palette marker, negative reported
length, allocation failure and missing files. Rejection asserts no published
image/dimensions, no output allocation before complete validation, and input
release before its warning. Both image fixtures, nine Python tests and Bash
syntax pass.

Both Retro68 products build without compiler diagnostics and pass PEF
validation using temporary libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,903 | `f8cd4c993a64558ca437b253166a520f5fb4b2ef26a9c7b465da5b88e9f9ca3a` |
| Quake3_TeamArena | 3,822,477 | `2612390ca6fe338d91af44ebdfb46ccd9209a948d22189e32f5702a448af2681` |

## Remaining acceptance

Keep #42 open. TGA still needs bounded decoding and its malformed corpus.
Retail 1.32c textures/UI and strict-alignment PPC live acceptance remain
explicitly deferred. This host corpus checks the supported PCX format and
bounds; it does not establish acceptance of every retail or mod asset.

Reviewed QVM work through #64, single-run CI (#68) and the RoQ console
shutdown fix are integrated. The affected sanitizer fixtures, nine Python
checks and both Mac products pass; CI retains every runner. The table
records these integrated artifacts.
