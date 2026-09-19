# Legacy inflate callbacks and allocation costs — 2026-09-18

The embedded decoder defines its private allocators with `voidp` (`Byte *`),
then casts them to the existing `void *` callback types. An ordinary deflated
PK3 entry reproduces an incompatible function call under Clang UBSan. Match
the existing callback signatures directly and remove the function casts.
Reject an `items * size` product above `INT_MAX` before multiplication or the
signed engine allocation import. Keep native cleared and zero-size allocation
behavior. The engine's existing zone allocator retains its own header/alignment
cost checks; this step checks the decoder's payload product.

Add a complete include guard to the existing unzip header. Its definitions,
layouts and function interfaces remain unchanged. Keep the embedded legacy
decoder, PK3 format and commercial 1.32c game/syscall/network interfaces.

## Validation

The fixture compiles the actual filesystem mount/open/read/free bodies and
embedded decoder together. Python creates an ordinary compressed ZIP; it does
not replace ZIP parsing or decompression. Only engine allocation/log/platform
imports are supplied by the fixture. Each allocation owns physical storage.

Clang ASan/UBSan and optimized GCC pass normal/fast modes. Native mount and
buffered unique reads preserve the complete 12-byte payload and EOF; shared
full-file reads preserve the payload and trailing NUL. Every handle, decoder
allocation, mount owner, temporary buffer and load-stack owner releases.
Direct native cleared/zero-size allocations also pass. Three unsigned product
boundaries reject before any engine allocation import.

The unchanged original decoder fails 12 product-boundary proofs across the
four compiler/mode combinations. Clang also reports the incompatible callback
on ordinary compressed reads in both modes, for 14 demonstrated failures.
Four separate original cleared/zero-size goldens and two original GCC ordinary
compressed-read goldens pass. Original Clang compressed reads are demonstrated
failures and are not counted as passing goldens. The original-body proof emits
the pre-existing redundant-parentheses warning; fixed checks emit none.

Five ledger/build-manifest unit checks, Bash syntax and diff checks pass. CI
runs this fixture with GCC and Clang without adding another required job.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `1a3f3a5b01709fd8d08e48b8798d5f5945700bf5ff799f8fd43ff9f6a83b4179` |
| Quake3_TeamArena | 3,932,501 | `7eb4a832ddca201618721336712a6fb1c3829a032104105bef5cc8ab5621c3bf` |

Build source is the `a036478` baseline plus this decoder change, without later
bot PRs. Temporary toolchain libraries follow the existing
[loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #36 open. Entry-size cap/`INT_MAX`/`UINT32_MAX` ZIP fixtures, truncated
streams, mount metadata/name validation and all failure ownership paths need
separate evidence. This step does not change the legacy decoder's CRC policy.
Retail PK3 and Mac OS 9 execution remain deferred by the user.
