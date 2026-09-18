# Native console message pool publication — 2026-09-18

InitConsoleMessageHeap previously cast an unchecked count, released its prior
logical owner before checking allocation and formed invalid singleton links.
Replacement also left active queue roots in the old pool. Validate finite native
signed counts and payload cost, complete queue shape/capacity and a checked
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
nullable imports and occupied allocation failure. Every failure retains full
prior heap/free roots, all pool bytes and every client byte, and consumes no extra
physical hunk; each retries successfully. Native defaults/fractions and singleton
free-chain endpoint/clear-field goldens remain separate. NaN/infinity, signed
range, excessive payload and nonpositive/truncated-zero counts stop before
conversion/arena imports. Too-small capacity, negative/excess queue counts,
mismatched counts/endpoints/previous links and cycles reject before allocation
without changing caller-provided state. Restoring valid queues permits retry.

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
missing checks. FIFO timing uses the AAS clock boundary stub.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes without diagnostics. Parent chat lifecycle checks also pass
six modes with both compilers. An optional no-main seam leaves their default
execution intact. CI runs both compilers. Bash syntax, five ledger/manifest checks
and diff checks pass. Physical host alignment follows [item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,255 | `00fada046be6657ebc702906a9f1505b07186fb4d2362d943f0f3060142fca08` |
| Quake3_TeamArena | 3,936,829 | `c33be9d0470cd139367f2581088b6ce5a0f4084e41394c942e07a9fad101bc7c` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, protocol defaults, valid counts and queued
payload behavior remain unchanged. This is synchronous native pool replacement,
not a claim that the full dictionary/library transaction is complete.

## Remaining acceptance

Keep #48 open for other chat loaders/string consumers, cache policy ownership,
complete dictionary/library/world transactions and aggregate expression/parse/
recursion/memory budgets. Retail/Mac OS 9 execution remains explicitly deferred.
