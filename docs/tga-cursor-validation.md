# TGA cursor validation — 2026-09-17

The third step for issue #42 replaces unchecked TGA header, image ID, raw and
RLE reads with a portable renderer module using checked byte spans. Header
fields decode bytewise. Nonzero dimensions and the RGBA allocation product
are validated by division before multiplication; every complete raw/RLE
packet is validated before allocating or writing output.

Supported native types remain 2 (RGB), 3 (uncompressed grayscale), and 10
(RLE RGB), without color maps. Type 3 also retains the native loader's
24/32-bit acceptance; unsupported depths fail before allocation. RLE runs
and raw packets may cross rows, but may not exceed the remaining image.
Image ID bytes must fit the file; optional trailing TGA metadata remains
accepted. The decoder writes exactly four RGBA bytes per declared pixel.

Row order and alpha follow the existing Quake III behavior. A declared
top-down image still uses native bottom-up order and produces the existing
warning, also retained by [ioquake3's TGA loader](https://github.com/ioquake/ioq3/blob/main/code/renderercommon/tr_image_tga.c).
The file is released before warnings/errors or published pixels/dimensions.
Malformed input keeps the existing ERR_DROP behavior, and missing files remain
quiet. CMake, Unix Make/Cons, Visual Studio, every Xcode renderer source phase
and lint lists include the module; the manifest regression checks it.

## Validation

ASan/UBSan executes the actual loader using exact input/output allocations,
with the input end at all four header alignments. Every prefix of small raw,
gray and RLE files is rejected before allocation. Golden images cover 24/32-bit
colors/alpha, grayscale, native row order and declared origins, 255-byte IDs,
mixed run/raw packets crossing rows, one-pixel and 128-pixel packet endpoints,
optional dimensions and trailing metadata. Wide/tall 65535-pixel grayscale
files exercise unsigned header extents without large image allocations.

Malformed cases include either packet kind exceeding the image, a later bad
packet after an earlier valid one, every ID/payload truncation, unsupported
types/depths/color maps, zero and overflowing dimensions, absent raw payload,
negative reported length, allocation failure and missing files. Rejection
asserts no partial output/dimensions, no allocation before complete validation,
and input release before the error longjmp. Post-jump state lives in static
storage. All three image fixtures, nine Python tests and Bash syntax pass.

Both Retro68 products build without compiler diagnostics and pass PEF
validation using temporary libraries from [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,885 | `fc23af815d85b1b0fa6167713bbe5e5c215aaf0c7013d6ac87f1d4fb1cf78ab6` |
| Quake3_TeamArena | 3,822,459 | `c1b37e5757073cee0c94bae557e3942a682a83b98c7cfb7e682191e965af1ecc` |

## Remaining acceptance

Keep #42 open until all three image PRs merge through the review/CI gate and
retail 1.32c textures/UI and strict-alignment PPC live acceptance are completed.
The user has deferred those live checks. The related JPEG source/destination,
fatal-error and duplicate-API work is tracked separately by #43.

Reviewed QVM work through #64, single-run CI (#68) and the RoQ console
shutdown fix are integrated. The affected sanitizer fixtures, nine Python
checks and both Mac products pass; CI retains every runner. The table
records these integrated artifacts.
