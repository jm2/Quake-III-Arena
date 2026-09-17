# RoQ frame geometry validation — 2026-09-17

The second step for issue #41 validates dimensions before cache updates or
quad construction. Dimensions must contain complete 8x8 groups with the
standard 8/4 quad sizes. Division checks the pixel capacity before products;
4-byte pixels in both frame halves must fit the fixed 2 MiB buffer. Complete
8x8/4x4 groups and 64 termination entries must fit each quad table. Each block
checks its last row/column against its frame half. Signed offsets between
halves replace pointer-to-32-bit casts. VQ data before geometry is rejected.

Preview drawing and videoMap uploads use the same texture dimensions and
pixel buffer. Each axis selects a power of two within the source and hardware
limits, retaining the Rage Pro 256 limit. Generic resampling reads source
rows using the validated stride into an exact-sized output. The standard
512x512 and 512x256 to 256x256 averaging paths retain their native behavior.
Smaller/rectangular frames no longer use hardcoded 256/512 row offsets or
submit 256x256 pixels regardless of their source size. Stopped frames are
never submitted again.

## Validation

The new ASan/UBSan fixture executes actual open/run/stop, quad construction,
preview drawing and upload callbacks. It verifies every block in both frame
halves and the termination reservation, and every submitted texture pixel.
Standalone source frames and output textures have exact-sized allocations.

Cases include minimum 8x8, 16x16, non-power-of-two 24x24, retail 256/512,
maximum-area 8x32768 and 32768x8, zero/small/non-aligned dimensions,
`65535` dimensions, excessive pixel products, unsupported quad sizes, VQ
without geometry, narrow/tall resampling, non-power-of-two source frames,
128/256 hardware limits, Rage Pro limits, both native averaging cases, and
stopped movie handles. The earlier chunk/audio fixture and eight Python tests
also pass. The four frame-offset host warnings are removed; one existing
unsigned shader-timing warning remains outside this step.

Both Retro68 products build without compiler diagnostics and pass PEF
validation with the temporary libraries in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,851 | `2aca1f59160b2221b6ebbc2ece4e6329d4c67ab1d15dc92a277de0b2a3c0c0e4` |
| Quake3_TeamArena | 3,822,425 | `8c73c16f0a8ee6367e85461f8e9e51474f000cdcd4c91466ace0aec082229087` |

## Remaining acceptance

Keep #41 open. VQ code/data cursors and motion-compensated source blocks still
need validation and malformed fixtures; the chunk and geometry steps do not
establish complete decoder safety. Actual retail 1.32c and Mac OS 9 playback,
including texture/image quality and performance, remain deferred. #14 still
tracks the intro/idlogo bypass.
