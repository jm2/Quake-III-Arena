# Native weapon weight pair publication — 2026-09-18

BotLoadWeaponWeights previously released the working pair before parsing the
replacement and before allocating its index. Stage a complete weight candidate
and checked index, then replace both roots together. Failure retains the working
pair. An identical cached weight owner remains valid while its new index stages.
Check the weapon table before any candidate import. BotFreeWeaponWeights clears
both roots after release so repeat cleanup is safe. A NULL filename has a safe
diagnostic. Private index inputs/count/cost are checked before allocation/write.

## Validation

Actual weapon/fuzzy/parser/libvar/allocator bodies cover twenty nullable imports
across native cached and reload ownership modes, each with a populated working
pair. Prior pair/header/tree/name/index bytes remain unchanged, private owners
release, sources/tokens close and each case retries. If a complete cached
candidate exists before its index fails, native cache ownership retains that
candidate until BotShutdownWeights; uncached candidates release immediately.
A cached alias plus failed index retains both old roots. Successful cached-alias
replacement performs one index import and no VFS imports. Malformed and suffix
source errors preserve the working pair under both ownership policies. Missing
tables and NULL filenames reject before imports. Repeat cleanup clears both
roots, selects no weapon and permits reload. Private invalid count/cost/input
checks precede allocation, and native zero capacity keeps a valid empty owner.

Native golden checks use real names, all index slots and FuzzyWeight evaluation
through BotChooseBestFightWeapon. Actual BotShutdownWeaponAI/BotShutdownWeights
release heap/cache/state owners; the owning engine resets physical hunk storage.
Tracked modes also prove zero remaining logical allocation records.

Eight original actual-weapon proofs independently reproduce four parser/source-
error root losses, cached-index failure, stale cleanup roots, missing-table old-
pair loss and invalid private input. Each fails in six modes under both compilers.
Separate original cached/reload pair/evaluation/public-shutdown goldens pass all
twelve builds. The original weapon body and its real fuzzy dependencies are
unchanged in these proof builds. The parent memory adapter already bounds native
requests; private index checks do not imply a new allocator guarantee.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all normal/fast/debug/
tracked modes without diagnostics. CI runs both compilers. Bash syntax, five
ledger/manifest checks and diff checks pass. Physical host alignment follows
[item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,784,035 | `785d4bf682f58cd9fb681333aed6300e5f5cfadc7a9f26c81a43aaa4eac2ea1f` |
| Quake3_TeamArena | 3,936,705 | `b1ac67f8e52c8bd7089b7c48a3b85b29f8c7a7008eccd6b25a9f10be31fb4ef5` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public layouts/imports/syscalls, protocol defaults, weight-cache ownership
and valid native field/evaluation behavior remain unchanged.

## Remaining acceptance

Keep #48 open for active index rebuilding across weapon-table replacement, public
weapon allocation/range consumers, further nullable factories/chat, complete
transactions and aggregate expression/parse/recursion/memory budgets. Retail/
Mac OS 9 execution remains deferred.
