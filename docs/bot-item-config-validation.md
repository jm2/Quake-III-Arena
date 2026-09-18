# Complete native item configuration — 2026-09-18

LoadItemConfig casts unchecked native counts, copies unterminated filenames,
initializes unchecked hunk storage and publishes valid prefixes after parser errors.
Its missing-classname branch frees only the source header. Malformed input also
consumes permanent engine hunk storage before parsing can reject it.

Check complete count initialization, finite/representable conversion and signed
metadata/payload cost. Preserve native zero/fractional counts and negative cached
count fallback, but reject a failed fallback update. Check full filenames before
copying. Parse into checked heap staging, release every complete source on failure,
reject sticky source errors and publish one complete copied/rebased hunk owner.
Use offsetof for native item field descriptors, retaining their actual offsets and
allowing actual-body 64-bit-host checks. Native item fields/defaults, commercial
1.32c imports/syscalls/layouts and legacy protocol remain compatible.

## Validation

Eight original actual-factory proofs reproduce malformed cleanup/hunk consumption,
source/lexical prefix publication, NaN conversion, oversized cost, missing filename
and unterminated maximum path. Original successful fields/default/fractional/zero
and manually seeded negative-cache fallback goldens pass separately. The original
proof changes only the pointer-to-int field-offset spelling to equivalent offsetof
for host compilation; factory/parser bodies remain original.

Fixed checks cover all seventeen nullable imports, malformed prefixes/fields,
source/lexical errors, six nonfinite/oversized counts and three invalid filename
paths. Every nullable path cleans complete parser/staging owners, retains only
complete shared count variables, consumes no failed hunk and retries. Failed
distinct loads retain complete prior configuration/item bytes without extra physical
hunk consumption. Failed negative fallback update retains its prior string/value
and stops before VFS/arena imports; retry succeeds. Native structure fields,
numbering, model/name/classname strings, vector/float values and zero-capacity empty
configuration warnings remain. Final inline pointers rebase into complete storage.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass six normal/fast/debug/
tracked allocator modes without diagnostics. The host import callback offsets
physical storage using the actual measured ownership prefix to satisfy host
long-double alignment. A temporary PPC compiler probe confirms native packed
script alignment and ownership prefix are both four bytes; no production packing
or allocator prefix changes are made. Heap/parser owners physically release;
hunk release remains logical until the owning engine resets its physical arena.
Five ledger/manifest checks, Bash syntax and diff checks pass; GCC/Clang CI runs
the fixture. Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,605 | `8e5b1188f09b179e0723ceab84765f26f7473ef741610c6aafb990833911211f` |
| Quake3_TeamArena | 3,928,179 | `d195f532cae2ebb9104b941b52203623878e69f258b5f765fcdaff6822fa49d6` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for shared structure numeric conversion, goal/setup and other
publication/allocation consumers, aggregate memory/work/recursion budgets and full
world/library transactions. This establishes factory ownership, not all item
runtime semantics. Retail/Mac OS 9 execution and target bot acceptance remain deferred.
