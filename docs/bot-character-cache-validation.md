# Native character cache hits at full capacity — 2026-09-18

BotLoadCachedCharacter previously required a free slot before checking for an
existing cached character. A complete 64-slot cache therefore returned zero for
valid cache hits. Look up existing non-reload characters before reserving a free
slot. Native exact/any-skill/tolerance matching remains unchanged; reload and
new-file loads still require a free slot.

## Validation

Actual whole character/cache/parser/libvar/allocator bodies load every native
slot with complete integer/float/string data. With the cache full, first/final
exact hits, any-skill lookup and the existing skill tolerance return their native
handle with no file or allocator imports. Every cached root and complete header/
field byte, string value and physical owner count survives lookup. Reload and
missing keys still return zero without imports when full. Public free/reload
reuses the final slot; public shutdown frees every cached character/string owner
before engine reset. Tracked modes prove zero logical records. Ordinary cold/
cache/reload and characteristic 0/1/79 goldens retain native values.

Four original full-capacity lookup scenarios reproduce 48 failures across both
compilers and six allocator modes. Separate original ordinary cold/cache/reload/
field/shutdown goldens pass all twelve builds. Original production bodies are
unchanged in the proof builds; actual sources are included rather than mirrored.
The fixture uses engine import callbacks and physical owner accounting, with
sufficient storage for all 64 native character/string pairs.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast/
debug/tracked modes without diagnostics. Affected parent skill/default checks
pass their Clang normal/fast modes. CI runs both compilers. Bash syntax, five
ledger/manifest checks and diff checks pass. Physical host alignment follows
[item evidence](bot-item-config-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,796,783 | `9b62bd787bac30394e5028c8b6a7374f85cf4f562f4f216158991dd4ccde39e6` |
| Quake3_TeamArena | 3,945,357 | `d0b69470ae6fcae6adcd2b5e3a3753cc5731701562b4b0c64b174f596796ca73` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Commercial
1.32c public structs/imports/syscalls, native cache capacity, protocol defaults,
matching policy and successful field/handle values remain unchanged.

## Remaining acceptance

Keep #48 open for further character/publication consumers, complete library/
world transactions and aggregate expression/parse/recursion/memory budgets.
These cache-hit checks do not claim full fallback/character graph transactions.
Retail/Mac OS 9 execution is deferred.
