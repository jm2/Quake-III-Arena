# Bot allocation adapters and failure ownership — 2026-09-18

The #47 graph-validation prerequisite also addresses a #48 allocation root.
Native clearing wrappers call memset after a nullable allocation, unchecked
unsigned payload plus prefix can wrap/truncate into signed int engine imports,
and native FreeMemory(NULL) forms and dereferences an invalid header pointer.

Check payload/prefix lengths before calling the signed allocator import,
propagate nullable clearing and accept null cleanup. Optional tracked variants
also reject ownership-counter overflow and null imports before header/list
writes. Keep the exact native ownership prefix, payload initialization and
heap/hunk release distinction, with no hard map cap or public ABI change.

## Validation

Four original actual-body sanitizer proofs fail: heap and hunk clearing after
nullable import, ULONG_MAX prefix wrap with a header write beyond the returned
allocation, and null cleanup. New actual-body checks run shipped, debug, tracked
and tracked-debug variants. All four allocation entry points retain raw/cleared
zero-to-65-byte behavior and exact import length plus prefix. Each nullable
import and unsigned/header/signed-limit failure leaves ownership unchanged;
tracked variants also reject each accounting limit before invoking imports.
Heap frees physically, while hunk frees remain logical and fixture arena bytes
are released only at reset. Null cleanup has no allocator effects.

Local CC=clang ASan/UBSan and separate optimized GCC checks pass in all four
configurations. CI runs its default cc and explicit CC=clang, each under
normal and optimized ASan/UBSan configurations.
All four native AAS regression runners, nine Python checks, Bash syntax and diff
checks pass. Both Retro68 products build with zero compiler diagnostics and
validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,749,997 | `08d05ecfad29bad05f61c85779e6ac9327784ddbd3acb80382c1711c81f1a013` |
| Quake3_TeamArena | 3,898,571 | `47132aa98ab69c39b17f3fe19ff97bb9378665b439747e331268d9d02cc45cc9` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

After CodeRabbit review, the fixture asserts the exact ownership-prefix request
size and rejects fatal diagnostics; tracked FreeMemory(NULL) returns before
BlockFromPointer. Its original tracked-debug proof emits a fatal diagnostic.
Both CI compilers now run normal/optimized variants explicitly. The follow-up
Clang sanitizer runs pass in all eight configurations, and both PPC products
rebuild without compiler diagnostics; their artifact hashes above are unchanged.

## Remaining acceptance

Keep #47/#48 open for full graph/runtime budgets and bot-parser/token/path and
per-caller allocation handling. This makes adapters safe; a parser that assumes
non-null storage still needs its own failure path. Native allocator imports can
also raise engine errors instead of returning NULL. No physical hunk rollback,
complete parser transaction or live retail/Mac OS 9 execution is claimed.
