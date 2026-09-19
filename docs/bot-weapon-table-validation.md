# Native live weapon-table indices — 2026-09-18

BotSetupWeaponAI previously replaced the table while active states retained index
arrays sized/mapped for the prior table. Growth could reach a heap over-read in
BotChooseBestFightWeapon; changed weapon numbers kept stale mappings. Validate
both native counts and their combined cost, stage every active replacement index
before the loader consumes persistent arena storage, then fill complete indices
from the new table without further allocations. Publish the table and all state
index roots only after success. Failure frees private indices and retains every
prior table/pair byte without spending an additional physical hunk. Successful
replacement releases old logical table/index records. Weight owners stay intact.
The loader and setup share their existing checked payload-cost calculation.

## Validation

Actual table/factory/fuzzy/parser/allocator bodies exercise two active states and
one empty state, cached and private weight ownership, capacity growth 2 to 8 and
shrink 32 to 8, fractional counts and a weapon moved from slot 1 to 4. Fifty-two
nullable stages retain the complete prior header/inline arrays/unused/fixup bytes,
all state roots, weight headers/trees and every prior index entry. Private owners
release, sources/tokens close, physical hunk count remains unchanged and each
case retries to one complete new persistent owner. Malformed and suffix source
errors cover all four combinations. Invalid counts and payload costs reject before
private index imports. Native empty zero-capacity replacement keeps weight owners,
complete zero-size indices, its warning and no selected weapon.

Fixed direct public evaluation checks moved weapon selection after growth.
Four original actual-table replacement cases independently fail for stale mappings
in all twelve compiler/mode builds. An independent original public growth case
reproduces AddressSanitizer heap-buffer-overflow in BotChooseBestFightWeapon in
all six sanitizer modes, before checking any private index contents. Separate
original ordinary cached/private evaluation and public-shutdown goldens pass all
twelve builds. Original production bodies are unchanged in these proof builds.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass six normal/fast/debug/
tracked modes without diagnostics. Parent loader/setup/weight fixtures also pass
all six modes with both compilers after the shared cost and index staging changes.
The setup runner now links the actual fuzzy implementation; an optional no-main
seam reuses the weight fixture without changing its default execution. Actual
public shutdown releases all heap/cache/state owners and tracked logical records;
only the owning engine resets physical hunk storage. CI runs both compilers. Bash
syntax, five ledger/manifest checks and diff checks pass. Physical host alignment
follows [item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,153 | `dc1696560fb746a9d250eeabb611c95b896b93f60105f3533a9669a1ef00c015` |
| Quake3_TeamArena | 3,936,727 | `64f19b2778ba6961f5b63638379a3a3ecea302b7c33a75f39db07bf949166eb6` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, protocol defaults, native format and weight
ownership remain unchanged. This is the synchronous native loader transaction;
physical old arena storage remains until engine reset.

## Remaining acceptance

Keep #48 open for public weapon state/range consumers, further nullable factories/
chat, complete library/world transactions and aggregate expression/parse/recursion/
memory budgets. Retail/Mac OS 9 execution remains explicitly deferred.
