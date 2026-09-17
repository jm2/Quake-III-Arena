# VM syscall memory trap validation — 2026-09-17

The seventh step for issue #35 routes UI, cgame, and game MEMSET/MEMCPY/STRNCPY
traps through shared range checks. Negative lengths, nonempty null buffers,
and ranges crossing the VM allocation fail before mutation. Legacy address
masking remains. Zero-length operations do not dereference buffers; memory
copies handle overlap with `memmove`.

String copying bounds the destination and scans only the available source
bytes. A terminating source at the last byte is accepted and zero-padded;
an unterminated source may be copied through the requested count but cannot
be read beyond its allocation. STRNCPY returns its VM destination, matching
[ioquake3's trap behavior](https://github.com/ioquake/ioq3/blob/master/code/client/cl_cgame.c),
rather than exposing a native pointer. Trusted native modules retain their
32-bit pointer ABI.

A range fault marks the QVM before `ERR_DROP`; the VM call dispatcher skips
faulted interpreted or compiled modules during shutdown.

## Validation

The common memory-trap ASan/UBSan harness uses an exact-sized allocation and
covers negative/extreme lengths, destination/source boundaries, null buffers,
zero lengths, masked addresses, both overlap directions, source termination
at the last byte, padding, bounded unterminated strings, and VM return values.
Every rejected operation preserves the entire data image. The dispatcher
harness also checks faulted interpreted/compiled re-entry.

All six sanitizer runners and eight Python checks pass. Portable CI includes
the new trap harness. Both Retro68 products build without compiler diagnostics
and pass PEF validation with the temporary libraries described in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,677,987 | `53584f54f8da10cdffac20394fffffda9face2ec9b7fe728355b29c6d53c7acc` |
| Quake3_TeamArena | 3,826,561 | `425dac46a417fb8c4e0dddbd4681f7a0c28c2a35c0bad189135de9d564b605bf` |

## Remaining acceptance

Keep #35 open. File, cvar, command, renderer, collision, botlib, and other
syscall families still need pointer/structure/array checks. The harness tests
the real shared handlers; full dispatcher compilation is covered by the Mac
builds. Native Windows and retail/Mac OS 9 live acceptance remain deferred.
No complete syscall sandbox claim is made.
