# Native goal and movement state factories — 2026-09-18

BotAllocGoalState previously dereferenced a NULL state import. BotAllocMoveState
returned a positive handle backed by NULL. Both now stage a complete cleared
private state, return native failure handle zero on import failure, and publish
only after required goal client initialization. Native successful state values,
handle numbering, exhaustion behavior and public ABI remain unchanged.

## Validation

The actual goal and movement production bodies and native allocator/libvar/parser
bodies cover fresh and occupied failures, every prior state byte/root, no new
physical owner, and same-slot retry with complete cleared values. Actual public
goal stack and movement initialization retain ordinary input fields and flags.
Both factories fill all 64 native slots in order, fail without imports when full,
free/reuse the last slot, release every physical state through shutdown, and
restart at slot one. Tracked modes prove zero logical owners after release.

Four original family/scenario failures (goal/move, fresh/prior) reproduce across
both compilers and six modes: 48 expected failures. Separate original ordinary
first-state/property/free/shutdown goldens pass all 24 family/compiler/mode builds.
Original production bodies are unchanged in those proof builds. The fixture
includes whole production sources rather than mirrored factory implementations.
Native engine imports are boundary callbacks with physical owner accounting.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes for each family without diagnostics. CI runs both compilers.
Bash syntax, five ledger/manifest checks and diff checks pass. Physical host
alignment follows [item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,755 | `46bd50b4318a3e014428593ad5c44119becf32f9ee190d14b4549a320ed06171` |
| Quake3_TeamArena | 3,945,329 | `177eef38c3a3dc6655077f7aa393f35ca77f6a99dfd419db464a9a5666112113` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, handle conventions, native protocol defaults
and successful state/client values remain unchanged.

## Remaining acceptance

Keep #48 open for goal weight/index publication, further public pointer/factory
consumers, cache policy ownership, full library/world transactions and aggregate
expression/parse/recursion/memory budgets. These state-owner tests do not claim
complete shared weight/cache shutdown. Retail/Mac OS 9 execution is deferred.
