# Complete native level-item pool — 2026-09-18

InitLevelItemHeap frees prior storage before resolving unchecked count/allocation
imports, then links missing or zero-size storage and writes at count minus one.
The public initializer clears map information before noticing no heap failure.

Validate complete native counts and signed payload cost before integer conversion.
Stage cleared pool storage and its complete free chain before replacing heap/free/
active lists and count. Propagate the private failure before public map-information
reset. Prior pool/list bytes survive failure; complete shared cache variables retain
native retry ownership. Existing 256 default, positive fractional truncation,
allocation/free/list behavior, commercial 1.32c interfaces and protocol remain.

## Validation

Twenty original actual-pool/public-path proofs expose twelve nullable node/value/
pool imports with empty/occupied prior state, zero/fractional-below-one/negative/
oversized counts, infinities/NaN and payload overflow. They fail in normal/fast modes.
Original private/public 256-node ascending free-chain/cleared allocation/list/
exhaustion goldens and native 2.75-to-two-node golden pass separately. The proof
renames the original void helper and uses a return-true adapter for the new private
status signature; original pool body and public caller behavior remain unchanged.

Fixed tests exercise twelve nullable paths and sixteen invalid count/cost paths
through private/public initialization. Every failed setup retains complete pool,
free/active pointers, active count and both node payloads; no map-info iterator runs
and only complete shared variables survive. Nullable cases retry and physically
release prior heap storage. Native default and configured chains, actual Alloc/
Free/Add helpers, clearing and exhausted-pool diagnostic remain. Successful complete
replacement resets active state and publishes only its new free chain. Native tracked
allocator teardown leaves zero logical blocks/bytes; all physical heap owners release.
Public success uses a synthetic empty BSP; runtime navigation queries assert unreachable.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass six normal/fast/debug/
tracked allocator modes without diagnostics. Actual goal/structure/source/allocator
bodies compile; host callback alignment uses measured ownership prefixes as documented
in [item evidence](bot-item-config-validation.md). Five ledger/manifest checks, Bash
syntax and diff checks pass; GCC/Clang CI runs the fixture. Both PPC products build
with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,853 | `c660a219cbef1fe62c72f0a2fd36f74ae062031da8f3cb7f8b16eb0e70b0b961` |
| Quake3_TeamArena | 3,932,427 | `349c46b60bc03332810d8bc7954d0eaafe2b9196d6e4e12c1e810eae5397f823` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for map-info/weapon/chat and further publication/allocation
consumers, aggregate budgets and full metadata/world/library transactions. Pool
failure preserves map info; later metadata/entity-scan failures still require their
own transaction policy. Retail/Mac OS 9 execution and target bot acceptance remain
deferred.
