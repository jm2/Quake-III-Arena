# Complete native structure numeric fields — 2026-09-18

ReadNumber negates a signed token word, casts unchecked selected field bounds and
narrows wide/nonfinite values directly into float storage. Invalid values can
invoke integer undefined behavior or publish nonfinite fields in item/weapon data.

Stage validated numeric values before writing destination bytes. Retain native
32-bit signed token-word interpretation and use defined unsigned word negation;
reject host tokens wider than the native word. Preserve native signed/unsigned
16-bit integer ranges and selected-bound truncation. Validate finite/ordered field
bounds and representable selected integer limits. Check float range and actual
finite representation after the original double/float narrowing, including release
fast-math. Native ordinary numeric fields, commercial 1.32c public interfaces and
legacy protocol remain compatible.

## Validation

Seven original actual-reader proofs reproduce signed-minimum negation, oversized
float publication, oversized/NaN selected integer bounds and queued positive/
negative infinity/NaN publication in normal/fast modes. The original source-error
case already rejects through the merged source-reader guard and remains a preserved
behavior case. Original 527 ordinary/boundary/historical-error recovery goldens pass
separately. New tests retain destination bytes on every rejected field, check native
unsigned/signed boundaries, fractional selected-bound truncation and bounded float
outcomes. Actual queued token copies and lexer/source reads own real native storage.
Native high-bit integer words and signed-minimum negation retain the target 32-bit
word semantics on the 64-bit host; overly wide host tokens reject.

Clang ASan/UBSan/float-cast-overflow and optimized GCC pass all six normal/fast,
debug and tracked allocator modes without diagnostics. The complete parent item
factory passes both compilers/modes, including seventeen nullable imports. Host
callback alignment uses the actual ownership prefix as documented in
[item evidence](bot-item-config-validation.md). Physical parser/heap owners release;
hunk ownership remains with its engine arena until reset. Five ledger/manifest
checks, Bash syntax and diff checks pass; GCC/Clang CI runs the new fixture. Both
PPC products build with zero diagnostics and valid PEF headers after the last edit:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,677 | `2cb3ca1a6ec628704f9dfbb9bdfede2f525f8f6308bb3107f3bea11823898fc2` |
| Quake3_TeamArena | 3,932,347 | `b1483e793518d21cf4b608f82ea07973e1e487417a493a6e19fcff1271285e15` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for goal/setup and further allocation/publication consumers,
aggregate memory/work/recursion budgets and full world/library transactions. This
checks numeric fields, not all structure descriptors or item/weapon runtime semantics.
Retail/Mac OS 9 execution and target bot acceptance remain deferred.
