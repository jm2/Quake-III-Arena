# Native buffered file-handle ownership — 2026-09-18

Buffered unique PK3 opens release their ZIP stream while keeping an owned
payload and cursor in the native handle slot. FS_HandleForFile previously
treated that slot as free because the stream pointer was NULL. Another open
overwrote the owner and cursor and leaked its buffer. A free slot must have
neither a stream nor an owned buffer. Keep the 63 usable native slots, stream
behavior, compressed payloads and commercial 1.32c interfaces.

## Validation

The fixture uses actual filesystem and embedded decoder bodies against a real
compressed ZIP, sharing the preceding allocator fixture. Two simultaneous opens
get distinct slots. The complete first handle, payload and partial-read cursor
survive a second open/close, a missing-entry failure and retry. The remaining
first payload and EOF stay correct; closing it leaves the retry readable.

All 63 usable slots hold real independent buffers. Every earlier complete handle
and payload remains intact while filling the table. Releasing the final slot
physically frees one owner; reopening reuses that slot alone. Every payload
remains readable during reverse-order close. Teardown releases every physical
zone/temporary/mount/decoder owner and the load stack. The fixture permits 128
physical zone records for this case; engine capacities remain unchanged.

The unchanged original filesystem body fails the pair and full-table proofs
across normal/fast Clang and GCC, for eight demonstrated failures. Four separate
original single-open/shared-read/cleared-allocation goldens pass. The proof uses
the preceding corrected decoder to isolate the filesystem defect; original
filesystem code is unchanged.

Fixed checks pass Clang ASan/UBSan and optimized GCC in normal and optimized
fast modes without diagnostics. Five ledger/manifest unit checks, Bash syntax
and diff checks pass. CI runs both compilers in the existing host job.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `2bb104610ddd890e6b998e8d660b16131368b43368dfd984ac496cda0ec93804` |
| Quake3_TeamArena | 3,932,501 | `5fd4516adc189078dab5f700cea4dba7754d7a7027637bb74ca0f98fab1acd5f` |

Build source is the `a036478` baseline plus the preceding unzip fix and this
handle condition. The later parent script-only fast-mode correction does not
change product source. Toolchain libraries follow the existing
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open for mount names/metadata, entry cap/signed-size/truncation fixtures,
all failure paths and deferred retail PK3/Mac OS 9 execution. This case does not
change or claim to test the engine's full-table ERR_DROP behavior.
