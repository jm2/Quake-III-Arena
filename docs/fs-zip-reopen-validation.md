# Native ZIP clone decoder ownership — 2026-09-18

unzReOpen copied an active shared decoder pointer into a new unique stream.
The filesystem copied that pointer again while updating selected-entry metadata.
Opening or closing the clone could release storage still owned by the shared
reader. Clear the borrowed decoder pointer at both copy sites. Copy filesystem
metadata only for unique streams; retain their independently opened FILE.

Keep selected-entry metadata, native shared/unique handles, independent read
cursors, PK3 format and commercial 1.32c interfaces. No layout/import/syscall/
protocol change or CRC-policy change is required.

## Validation

The fixture uses the actual filesystem and embedded decoder with a valid complete
32 MiB compressed zero payload. The archive is generated incrementally and uses
ordinary ZIP32 metadata. At the native exclusive buffering cap, both paths use
streams without allocating the full payload.

After a shared partial read, a unique open frees none of its physical owners and
preserves every byte of the complete shared read/decoder record. Both cursors
advance independently. Closing either reader first keeps the other usable.
Direct unzReOpen also retains entry metadata, gets its own FILE and copies no
decoder owner; closing the unused clone preserves the complete shared record and
subsequent reads. Teardown releases every physical zone/decoder/mount owner and
native handle. The fixture observes real engine allocation/free imports and does
not replace clone/open/read/close bodies.

The unchanged preceding filesystem and unzip sources fail three direct/public
cases in all four normal/fast Clang/GCC combinations, for twelve demonstrated
failures. Four separate original cold unique-stream goldens pass, including two
simultaneous unique readers, independent payload/cursor and survivor-after-close.
The proof stops at actual ownership assertions before dereferencing freed storage.

Fixed checks pass Clang ASan/UBSan and optimized GCC in normal/optimized-fast modes
without diagnostics. Parent decoder/buffer/mount fixtures pass both compilers.
Five ledger/manifest unit checks, Bash syntax and diff checks pass. CI runs both
compilers in the existing host job.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `5f26946e4c4ae186e5dcef3518d2872dd0aaef4234a0ad8d24ad93d75d800e37` |
| Quake3_TeamArena | 3,932,501 | `c92da1ee62d284f8e110661518a3c8abbc12bace8cba5070a906fce5aceb03b1` |

Source is the `a036478` baseline plus the preceding decoder/buffer/mount steps
and both clone-copy fixes. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for hostile entry sizes, truncated compressed payloads and remaining
failure paths. This valid 32 MiB stream case does not replace size-boundary tests.
Retail PK3/Mac OS 9 execution remains explicitly deferred by the user.
