# RoQ chunk and audio validation — 2026-09-17

The first step for issue #41 separates chunk headers from payload reads. The
reader tracks actual bytes consumed, checks the remaining file length, and
limits payloads to the 65,536-byte file buffer minus eight header bytes.
Every header uses all four little-endian size bytes. Reads use one-byte items
so both synchronous byte-return and threaded item-return stream implementations
report the exact byte count. A final payload no longer requires a following
header or gets discarded before decoding.

Embedded packets validate all child header/payload ranges and the declared
child count before dispatch. Nesting is limited to 16 packets. Mono expansion
must fit 32,768 output shorts (at most 16,384 input bytes); stereo must contain
complete pairs and fit the same output (at most 32,768 bytes). These checks
also apply to silent movies. Valid native sample decoding and submission
formats remain unchanged. Codebooks and quad-info chunks check their minimum
input sizes; RGBA channel packing uses defined unsigned shifts.

Short reads, invalid sizes, malformed packets, and excessive audio stop the
movie and disable looping. Cleanup closes the file even before any frame
buffer exists. Initial open and loop reset validate exact header reads and
reopen results. Public run returns EOF after cleanup and uses the saved handle
while shutdown clears the active handle. Stopped handles cannot reopen the
movie or repeat cleanup.

## Validation

The ASan/UBSan fixture executes the actual open/run/stop, reader, packet, audio,
and codebook paths with exact-sized input allocations. It covers:

- all initial header truncations and short reads after a successful stat;
- 16,385, 32,769, 65,529, and 65,536-byte payloads, both audio capacity limits,
  the exact disk-buffer limit, stereo odd/excessive sizes, and silent rejection;
- high size bytes, `UINT_MAX`, invalid magic and embedded stream magic;
- all small payload truncations and trailing header truncations;
- the final audio payload, a second disk chunk, complete native samples,
  and a rounded neutral codebook pixel;
- valid/invalid packet child counts, ranges, trailing bytes and nesting;
- short codebooks and quad info, failure without a reset loop, hold/stop before
  the first frame, valid looping, and failed/short-read loop reopen.

The new runner and eight Python checks pass. Five existing host compiler
warnings remain in frame-offset pointer casts and unsigned shader timing;
frame offset cleanup belongs to the next step. Both Retro68 products build
without compiler diagnostics and pass PEF validation using the temporary
libraries documented in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,686,293 | `c13fb70ebe374686e19291bc856f661cf63a6a522974ceddbcb1c1a2fd6d20bb` |
| Quake3_TeamArena | 3,834,867 | `7a1940aff74204b5326c7528172a317e66079e31a112fc26f29c18694531ed93` |

## Remaining acceptance

Keep #41 open. Frame dimensions/products, both buffer halves, quad counts,
VQ cursors, and motion-compensated source blocks still need bounded validation
and regressions. This step does not establish that the entire RoQ decoder is
safe for hostile movies. Retail 1.32c and Mac OS 9 live playback remain
explicitly deferred; the intro/idlogo bypass remains tracked by #14.

## Integration with reviewed QVM work

After integrating master through PR #64 and the single-run CI step (#68),
all eleven current host C runners, eight Python tests and both Retro68 products
pass. The workflow and CI documentation retain every QVM and RoQ check.
The table above records these integrated product artifacts. Frame/VQ work
still belongs to the following RoQ PRs; live acceptance remains deferred.

The actual cinematic console command also runs malformed pre-frame movies in
normal, hold and loop modes. Its first-frame wait condition uses the bounded
active CL handle and stops after shutdown clears it, without indexing -1.
