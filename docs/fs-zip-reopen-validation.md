# Native ZIP clone decoder ownership — 2026-09-18

unzReOpen copied an active shared decoder pointer into a new unique stream.
The filesystem copied that pointer again while updating selected-entry metadata.
Opening or closing the clone could release storage still owned by the shared
reader. Clear the clone's borrowed decoder pointer. When a shared reader is
active, query and open a unique target on its own archive; keep the shared
metadata, decoder and physical FILE cursor intact. Remove the filesystem's
redundant structure copy. Idle shared archives retain the native buffering fast
path. Reopen failure clears the candidate handle and returns native missing-file
status; any private clone also closes on metadata/size/open/read failure.

Keep selected-entry metadata, native shared/unique handles, independent read
cursors, PK3 format and commercial 1.32c interfaces. No layout/import/syscall/
protocol change or CRC-policy change is required.

## Validation

The fixture uses the actual filesystem and embedded decoder with a valid complete
32 MiB compressed zero payload, a complete 32 MiB stored byte-pattern payload
and a small compressed text entry. The archive is generated incrementally with
ordinary ZIP32 metadata. At the native exclusive buffering cap, large entries
use streams without allocating the full payload.

After a shared partial read, a unique open frees none of its physical owners and
preserves every byte of the complete shared read/decoder record. Both cursors
advance independently. Closing either reader first keeps the other usable.
Direct unzReOpen also retains entry metadata, gets its own FILE and copies no
decoder owner; closing the unused clone preserves the complete shared record and
subsequent reads. Teardown releases every physical zone/decoder/mount owner and
native handle. The fixture observes real engine allocation/free imports and does
not replace clone/open/read/close bodies.

Active stored readers retain their complete archive and decoder bytes and exact
physical FILE position through both large streamed and small buffered unique
targets. Reads cross more than two native input buffers and verify every byte of
the deterministic stored pattern. Small text buffering remains intact. This
covers the later Codex physical-cursor finding and the active-small-target path.

The unchanged preceding filesystem and unzip sources fail three direct/public
cases in all four normal/fast Clang/GCC combinations, for twelve demonstrated
failures. Four separate original cold unique-stream goldens pass, including two
simultaneous unique readers, independent payload/cursor and survivor-after-close.
The reviewed `40912889eb` head additionally fails eight physical-cursor/active-
small-buffer cases across both compilers/modes. Four separate reviewed-head native
cold unique goldens pass too, for eight original/reviewed goldens in the renewed
proof. Proofs stop at actual ownership/cursor assertions before freed-storage
access. Original sources in each proof remain unchanged.

Fixed checks pass Clang ASan/UBSan and optimized GCC in normal/optimized-fast modes
without diagnostics. Parent decoder/buffer/mount fixtures pass both compilers.
Five ledger/manifest unit checks, Bash syntax and diff checks pass. CI runs both
compilers in the existing host job.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `b88528e5300bb799c71f02dfb4e33e27c3b19262be7ca7160dfe0a26b22b8d6d` |
| Quake3_TeamArena | 3,932,501 | `44b1670a063b443fce7ff05dcc537bdd0a231ba07f5f3dd6b6df12ee7fa01e9f` |

Source is the `a036478` baseline plus the preceding decoder/buffer/mount steps
and the complete decoder/physical-cursor fix. Toolchain libraries follow
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for hostile entry sizes, truncated compressed payloads and remaining
failure paths. This valid 32 MiB stream case does not replace size-boundary tests.
Retail PK3/Mac OS 9 execution remains explicitly deferred by the user.
