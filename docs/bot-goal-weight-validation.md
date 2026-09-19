# Native goal weight pair publication — 2026-09-18

BotLoadItemWeights previously overwrote the working weight root before parsing or
index allocation finished, leaked old pairs on successful replacement and left
stale pointers after cleanup. Stage complete weight and checked item-index owners
before replacing both roots. Check the item table before candidate imports and
retain a cached alias while its new index stages. BotFreeItemWeights now clears
both roots. Private index inputs, item array and signed allocation costs are
checked before allocation or writes. Successful classname mapping stays native.

Codex identified cached data being freed after a reload-policy change during
replacement. The shared cleanup now follows actual cache ownership for both goal
and weapon states; [owner evidence](bot-weight-cache-validation.md) includes
120 original failures and separate native goldens. The current PPC artifacts
below include both the pair publication and shared cleanup changes.

## Validation

Actual goal/fuzzy/parser/libvar/allocator bodies cover twenty nullable imports
across cached and reload policies, with populated prior states. Failure preserves
all prior state/header/tree/name/index bytes, closes sources/tokens and releases
private candidates. A complete cached candidate retains its native cache owner
through index failure until BotShutdownWeights. Failed cached-alias index import
retains both prior roots; successful alias replacement imports only a new index.
Malformed and suffix errors preserve prior pairs under both policies. Absent
tables and NULL filenames reject before imports. Repeated cleanup is safe and
permits reload. Invalid count/cost/missing array/input checks precede imports;
zero capacity retains a valid complete empty index owner.

Native goldens use actual item classnames, all index entries, duplicate/missing
classname mappings and FuzzyWeight evaluation. Actual public goal/weight shutdown
releases physical heap/cache/state owners, while the owning engine resets hunk
storage. Tracked modes prove zero logical records after release. This fixture
checks evaluation through the actual fuzzy function; full navigation query
acceptance remains open.

Eight original actual-goal scenarios reproduce 96 failures across both compilers
and six modes, independently covering parser/source root loss, nullable cached
index, stale cleanup roots, missing table and invalid index inputs. Separate
original cached/reload mapping/tree/evaluation/public-shutdown goldens pass all
twelve builds. Original production bodies and dependencies are unchanged in the
proof builds. The parent allocator already checks native signed request costs.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes without diagnostics. Affected parent goal setup and both
state factories pass all Clang modes. CI runs both compilers. Bash syntax, five
ledger/manifest checks and diff checks pass. Physical host alignment follows
[item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,783 | `c4c386dd0434e27838e7b97819984b03d8f7fcb72e504c62fbce2994097e5b3d` |
| Quake3_TeamArena | 3,945,357 | `f69820cc7c5dc24e21a092272ba1543efea7be4b89b18311973ac502c534da63` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public layouts/imports/syscalls, native protocol defaults, loading policy and
valid classname mapping/evaluation remain unchanged.

## Remaining acceptance

Keep #48 open for active index rebuilding across item-table replacement, further public
consumers, complete world/
library transactions and aggregate expression/parse/recursion/memory budgets.
Retail/Mac OS 9 execution is deferred.
