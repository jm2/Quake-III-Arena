# RoQ codebook and VQ validation — 2026-09-17

The third step for issue #41 gives codebook and VQ decoders complete checked
payload spans. A codebook checks its entire fixed-size update before touching
tables; byte-oriented RGBA tables preserve all 256 entries, four 2x2 references
per 4x4 block, 2x expansion to 8x8, and native partial updates. The tables use
86,016 bytes instead of 688,128 bytes of overprovisioned ushort storage.
RGBA channel packing and copy operations avoid pointer aliasing/double casts.

The VQ cursor checks both bytes of every control-word refill and every
codebook/motion index. Complete 8x8 groups and 4x4/2x2 subdivisions are checked
against the destination frame. A first pass validates the full frame before
a second pass writes pixels, so a later bad block cannot leave earlier frame
writes behind. Every motion source row/column must fit the opposite frame half.
Signed offsets preserve native linear-stride and 4:1 aspect-ratio motion
behavior. Byte copies also support sources at an odd pixel offset. Legacy
unused trailing VQ bytes remain accepted.

## Validation

The ASan/UBSan fixture covers every truncation of a 2,560-byte full codebook
with exact payload allocations and unchanged table snapshots on rejection.
It checks every RGBA pixel in all 256 2x2/4x4/8x8 entries, default 256 counts,
2x2-only updates, one 4x4/8x8 update, and preservation of untouched entries.

Synthetic VQ frames cover every root/sub-block opcode, all four 2x2 indices,
every payload prefix of a mixed frame, both halves, full golden pixels and
unchanged motion source/unused buffer. Additional cases cover control-word
refill, a maximum-size 512x512 skipped frame, index 255, native trailing
padding, all 256 motion indices at a frame edge, row/bottom/flag failures,
a valid block followed by an invalid one, odd-pixel sources, 4:1 motion,
first-frame initialization, counter preservation on rejection, and texture
cache invalidation after successful dispatch. Rejected frames compare the
entire 2 MiB allocation and all codebooks before/after.

All three RoQ sanitizer fixtures, eight Python checks and Bash syntax pass.
One pre-existing unsigned shader-timing host warning remains. Both Retro68
products build without compiler diagnostics and pass PEF validation using
the temporary libraries in [loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,661,535 | `ede8253ac0bd53eb328b27af7c3b40b9e569e943e8c73633cafa9226d50f5334` |
| Quake3_TeamArena | 3,810,109 | `c69f717fef88bdd760950cbb50a7b19e1473e016693b96e6352fc71907962a21` |

## Remaining acceptance

Keep #41 open until all three PRs merge and deferred retail 1.32c/Mac OS 9
playback passes, including looping, previews, videoMap, audio and malformed
movie termination on target. These synthetic checks do not establish retail
content/image-quality/performance acceptance. #14 still tracks the cinematic
bypass, and #29 tracks broader security provenance/regression assurance.
