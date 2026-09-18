# Native public weapon guards — 2026-09-18

BotAllocWeaponState previously returned a positive handle after a NULL state
import. Return native failure handle zero without publishing an incomplete state.
BotValidWeaponNumber checks the table and exclusive array bound before a getter
read; zero remains rejected as in the native API. BotGetWeaponInfo checks its
output pointer before copying. Public selection returns no weapon before reading
missing index/inventory inputs. Valid native getter/reset/evaluation stays intact.

## Validation

Actual public weapon/factory/fuzzy/parser/libvar/allocator bodies exercise failed
state imports under cached and private weight ownership. Failure returns zero,
keeps prior roots/bytes and every physical owner, and retries at the same first
free slot to a complete cleared state. Actual public free releases that owner.
Getter tests reject 0/-1/count/count+1/INT_MAX with every output/canary byte intact,
and reject missing tables/outputs before reads/writes. Native invalid handles
remain safe. Missing selection index/inventory inputs return zero. Native golden
checks copy every getter field/padding byte within physical canaries, retain pair
roots on reset and select the same weapon using the real fuzzy evaluator. Public
shutdown releases heap/cache/state and tracked logical owners; physical hunk
storage remains until its engine-owned arena reset.

Six original actual-public proofs independently reproduce false positive handles,
exclusive-bound output modification, missing table/output and missing selection
inputs in all twelve compiler/mode builds. Separate original valid getter/reset/
evaluation/public-shutdown goldens pass all twelve builds. Original production
bodies and actual dependencies are unchanged in these proof builds. The getter
fixture verifies destination bytes as well as error status because the adjacent projectile array can keep
the wrong read inside the enclosing physical allocation; ASan alone is insufficient.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes without diagnostics. CI runs both compilers. Bash syntax,
five ledger/manifest checks and diff checks pass. Physical host alignment follows
[item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,788,165 | `2683f4331f7d11da20808316311b445da07be440392ccbaf9c298a58d98ae907` |
| Quake3_TeamArena | 3,936,739 | `64955118235687113c6be0d2d150df0fed3a03329d773316dc55cfbd25f7d6a2` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, native failure/valid-handle conventions,
protocol defaults, file format and valid field/evaluation behavior remain unchanged.

## Remaining acceptance

Keep #48 open for further nullable factories/chat, weight-cache ownership across
policy changes, complete library/world transactions and aggregate expression/
parse/recursion/memory budgets. Retail/Mac OS 9 execution remains deferred.
