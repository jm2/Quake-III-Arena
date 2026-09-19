# Real ZIP entry size/open/truncation evidence — 2026-09-18

The July size/cast/open/exact-read candidate is already committed in `204fe36`.
Add the missing actual-file host evidence before treating it as accepted. This
step changes fixtures, CI and documentation; product source remains exactly the
preceding decoder/physical-cursor step. Preserve commercial 1.32c formats and
the native exclusive generic buffering cap and large-text override policy.

## Validation

Actual filesystem and embedded decoder bodies read real ZIP32 files with complete
zero payloads just below, at and above 32 MiB. Generic unique reads buffer only
below the cap; above-cap text remains buffered. The complete buffered payload
reaches its last byte. Both streamed cap fixtures consume their entire declared
payload through repeated decoder refills, verify every byte including the final
chunk and reach stable EOF. Scalar length queries remain correct. These are
valid complete archives, generated incrementally. This closes the Codex finding
that checking only the first 64 streamed bytes could miss later corruption.

Malformed ZIP metadata declares entry lengths `INT_MAX`, `INT_MAX + 1`,
`UINT32_MAX - 1` and `UINT32_MAX`. Both unique/shared opens, full-file reads and
length queries reject before a signed allocation or temporary-buffer import.
The `INT_MAX - 1` boundary is checked only as open/query scalar metadata without
a huge allocation; its short declared payload is not a valid full-size golden.

Bad local headers and truncated compressed inputs cover small buffered and large
streamed paths. Buffered open/short read failures clear their handle. Streamed
truncation returns its signed short/error result. Failed full-file reads return
NULL, release the temporary owner and restore the load stack. A complete prior
independent buffered handle, payload and partial-read cursor survive every case.
Native valid-entry retries pass. Physical zone/temporary/mount/decoder owners and
actual OS file descriptor counts return to baseline; teardown clears all handles.

The unchanged original July filesystem body at `abe5028` fails eight cold
size/open/read cases across normal/fast Clang and optimized GCC, for 32 original
failures. No occupied prior is used in those proofs, isolating these defects from
the separately fixed old buffered-slot bug. Current decoder/engine allocation
imports isolate the historical filesystem body. Failures are expected rejection/
exact-read assertions or an observed invalid signed zone request. Four separate
original valid full-payload cap/text/stream/query goldens pass.

Fixed checks pass normal/optimized-fast Clang ASan/UBSan and optimized GCC without
diagnostics, including the renewed parent physical-cursor fix. Parent native ZIP
fixtures, five ledger/manifest checks, Bash/Python syntax and diff checks pass.
CI runs both compilers inside the existing host job.

Product source is unchanged from the preceding `adac2c8` step; both zero-diagnostic
PPC products, valid PEF headers and their hashes are recorded in
[stream evidence](fs-zip-reopen-validation.md). Host fixture changes do not require
another product rebuild.

## Remaining acceptance

Keep #36 open pending merged implementations and all current-head CI/review gates,
remaining metadata/open/read/handle paths, and retail PK3/Mac OS 9 execution.
The latter remains explicitly deferred. This evidence does not change or attest
the legacy decoder's disabled CRC policy or impose a new aggregate archive budget.
