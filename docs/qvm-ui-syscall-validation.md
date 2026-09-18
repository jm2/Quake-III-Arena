# QVM UI syscall validation — 2026-09-17

The eighth step for issue #35 checks the UI dispatcher's strings, fixed-size
structures and vectors, file buffers, output buffers, and vertex arrays before
passing them to native code. Common VM helpers validate complete ranges,
structure alignment, terminated strings, and array-size multiplication while
preserving legacy address masking and the trusted native module's 32-bit ABI.
A fault marks the QVM before `ERR_DROP` and prevents shutdown re-entry.

Optional NULL arguments remain supported where the existing API permits
queries or resets: cvar registration/reset, file-length queries, renderer color
reset, background tracks, server-status reset, real-time queries, shader time
offsets, and CD-key checksums. Writable file opens require a handle output;
invalid open modes drop the QVM. Non-NULL string outputs need at least one
byte. Empty polygon submissions return without calling the renderer.

Two output routines also needed repairs. `CLUI_GetCDKey` now honors the
requested capacity, including truncation and termination of buffers shorter
than 17 bytes. `PC_SourceFileAndLine` now truncates and terminates its filename
within the syscall ABI's `MAX_QPATH` bytes instead of using an unbounded copy.

## Validation

The shared memory-trap ASan/UBSan fixture now covers aligned exact-end
structures, unaligned byte buffers, required and optional NULL pointers,
masked pointers, negative/overflowing array sizes, unterminated input strings,
termination at the image boundary, and empty/oversized output buffers. Every
rejected argument preserves the complete VM image.

The UI runner calls the actual private CD-key routine for both base and unique
mod keys using exact-sized buffers from 1 through 20 bytes. A second fixture
calls the actual botlib filename output with short, exact-limit, and long paths,
with and without a script stack, and verifies invalid handles preserve output.
The linked parser dependencies are not used as a general parser fuzz harness.

All seven sanitizer runners and eight Python checks pass. Both Retro68 products
build without compiler diagnostics and pass PEF validation using the temporary
libraries described in the [loading evidence](qvm-loading-validation.md).
Portable CI runs the new UI fixtures and the extended common-helper fixture.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,678,039 | `7be114c748f6f1387b3fd8320a06e39406401d581354a0a31780f14cacf3f95e` |
| Quake3_TeamArena | 3,826,613 | `d4eff0a248143e0881f959afb61108c3e8760f0b2963b8aa38e2b41afacd6baa` |

## Remaining acceptance

Keep #35 open. Cgame and server syscall pointer/structure/array handling and
indirect native-side accesses still need review. Pointer bounds do not impose
cvar or command permissions; #39 remains separate. Retail 1.32c QVM behavior
and Mac OS 9 live acceptance remain deferred to the follow-up session.
