# Native projectile model offset — 2026-09-18

The projectile model descriptor uses WEAPON_OFS(model), eight bytes after the
actual projectile model field. Nonempty model text is absent from its expected
slot, and long text can overwrite flags/gravity when it follows numeric fields.
Use PROJECTILE_OFS(model), matching [ioquake3](https://github.com/ioquake/ioq3/blob/master/code/botlib/be_ai_weap.c)
and [Quake3e](https://github.com/ec-/Quake3e/blob/master/code/botlib/be_ai_weap.c).

Use standard offsetof for both private descriptor macros, retaining native field
values while allowing actual-body host compilation without truncating pointers.
Public projectile/weapon layouts, 80-byte strings, parser ordering, commercial
1.32c imports/syscalls and protocol remain unchanged.

## Validation

Two original actual-descriptor proofs reproduce misplaced short model text and
long-model scalar corruption in six normal/fast/debug/tracked modes. The original
no-model name/all-scalar golden passes separately in every mode. The proof changes
only the offset macro spelling to standard offsetof for host compilation; native
offset values and the incorrect original model descriptor remain unchanged.

Fixed tests exercise lengths 0/1/13/71/72/79 before and after scalar fields, plus
the no-model golden. Actual ReadStructure and projectile descriptors check exact
model/name text, every scalar value and both physical structure canaries. Actual
source/parser/structure/allocator bodies release every source/token heap owner;
no physical hunk is consumed. Clang ASan/UBSan/float-cast-overflow and optimized
GCC pass all six modes without diagnostics. The parent item fixture also passes
all six modes with both compilers after introducing an optional owner-only reuse
seam. GCC/Clang CI runs the new fixture. Five ledger/manifest checks, Bash syntax
and diff checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,927 | `3c5fb69554727288cc9378252d2ff6d9d172324bb09505ec45f7c32418ea3ab1` |
| Quake3_TeamArena | 3,932,501 | `665fc81f97eaa28b5402b5c8841149285b9b19d3c7673c55ca9a9b3cc45cf658` |

Both PPC products build with zero diagnostics and valid PEF headers. Temporary
toolchain libraries: [loading evidence](qvm-loading-validation.md). Host allocator
alignment uses measured ownership prefixes as documented in [item evidence](bot-item-config-validation.md).

## Remaining acceptance

Keep #48 open for weapon count/path/allocation/source-error and configuration/
weight publication consumers, chat consumers, complete transactions and aggregate
budgets. This is actual descriptor/parser evidence, not full weapon-factory failure
coverage. Retail/Mac OS 9 execution remains explicitly deferred.
