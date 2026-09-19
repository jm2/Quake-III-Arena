# Native console message pool publication — 2026-09-18

InitConsoleMessageHeap previously cast an unchecked count, released its prior
logical owner before checking allocation and formed invalid singleton links.
Replacement also left active queue roots in the old pool. Validate finite native
signed counts and payload cost, complete queue shape/capacity and unique, aligned membership in the current pool
before any node dereference, then a checked
private hunk candidate before publishing any root. Copy queued message payloads
and rebind each client's complete links; build only unused free slots with correct
endpoints. Keep queued counts/current handles and all other state bytes intact.
Success releases the prior logical record; physical arenas remain engine-owned.
Default 1024 and valid positive fractional truncation remain native. Nonpositive
capacity is rejected because a console pool requires a real slot. Singleton links
are now valid. Keep the existing void helper; private checked initialization runs
before chat setup changes dictionary/source roots and propagates failure through
the existing BLERR_LIBRARYNOTSETUP return code. No retail syscall/table changes.

## Validation

Actual chat/pool/message/parser/libvar/allocator bodies cover all three fresh
nullable imports and both occupied ownership-bitmap/hunk allocation failures. Every failure retains full
prior heap/free roots, all pool bytes and every client byte, and consumes no extra
physical hunk; each retries successfully. Native defaults/fractions and singleton
free-chain endpoint/clear-field goldens remain separate. NaN/infinity, signed
range, excessive payload and nonpositive/truncated-zero counts stop before
conversion/arena imports. Too-small capacity, negative/excess queue counts,
mismatched counts/endpoints/previous links and cycles reject before persistent
arena allocation without changing caller-provided state. A temporary checked
heap bitmap rejects foreign, one-past, misaligned, genuinely stale and shared
nodes before dereferencing them; it is freed on every exit. Restoring valid
queues permits retry. Count/capacity errors require no bitmap allocation.

Two active/one empty states migrate queues into capacities 3/4/8, including a
full pool and a removed message whose old storage is no longer contiguous. Real
FIFO handles/time/type/text, current client serials and public output links remain;
every active head/tail points into the complete new pool and unused slots form
one cleared bounded free chain. Actual BotSetupChatAI rejects a failed pool before
any dictionary variable/source import and preserves distinct prior dictionary
owners. Actual shutdown releases all logical/heap/state/cache owners; physical
hunks reset only with the engine. Tracked modes prove zero logical records.

Seven original actual-pool proofs reproduce three unchecked fresh imports,
occupied allocation failure, stale queue roots, invalid singleton endpoints and
unchecked too-small replacement in every compiler/mode build. Separate original
default/fractional free-chain and physical-shutdown goldens pass all twelve builds.
Original allocation/link bodies are unchanged; only a test-local status/capacity
adapter permits the original void helper to enter the status-oriented harness.
The adapter reports completion after the original body and does not add its
missing checks. Five further ownership proofs against the actual initially
reviewed checked helper reproduce all five node failures across twelve builds
(60 failures), without that adapter. Separate native default/fractional goldens
pass the same twelve reviewed-body builds. FIFO timing uses the AAS clock
boundary stub. These checks address CodeRabbit's queue-ownership finding.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes without diagnostics. Parent chat lifecycle checks also pass
six modes with both compilers. An optional no-main seam leaves their default
execution intact. CI runs both compilers. Bash syntax, five ledger/manifest checks
and diff checks pass. Physical host alignment follows [item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,261 | `b2d5e67b7b0348cd3c134078029ec9d5c3f828fe7165cfbb308df019785b05b1` |
| Quake3_TeamArena | 3,936,835 | `3226de60cc32c4c201017913234aa9fcdeeefe2ffd6de971a5c044f24c2977ef` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, protocol defaults, valid counts and queued
payload behavior remain unchanged. This is synchronous native pool replacement,
not a claim that the full dictionary/library transaction is complete.

## Remaining acceptance

Keep #48 open for other chat loaders/string consumers, cache policy ownership,
complete dictionary/library/world transactions and aggregate expression/parse/
recursion/memory budgets. Retail/Mac OS 9 execution remains explicitly deferred.
