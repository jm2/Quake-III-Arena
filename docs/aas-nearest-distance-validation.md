# Native floating entity proximity — 2026-09-18

AAS_NearestEntity applies integer abs to two float coordinate differences. Wide
coordinates, infinity and NaN can invoke undefined float-to-int conversions or
signed absolute-value overflow. Use fabsf for both native constant-40 comparisons,
as in [ioquake3](https://github.com/ioquake/ioq3/blob/master/code/botlib/be_aas_entity.c).
Finite defined legacy decisions remain: truncating a float magnitude and comparing
it below integer 40 has the same result as comparing its float magnitude below 40.
Public imports/syscalls/layouts and commercial 1.32c protocol remain unchanged.

## Validation

A temporary actual-function proof uses native entity structures and the real
heap/hunk adapters. Original and fixed code pass 7,175 defined coordinate decisions
in normal and release fast-math modes. Six original wide/nonfinite/signed-minimum
cases trigger sanitizer failures; all fixed cases complete in both modes without
integer conversion. Nonfinite navigation decisions are not acceptance claims.

The existing complete AAS/public-setup fixture passes Clang sanitizers and optimized
GCC in six normal/fast/debug/tracked allocator modes. Both former Clang abs-on-float
warnings disappear; GCC and Clang report zero fixture compilation diagnostics.
Five ledger/manifest checks, Bash syntax and diff checks pass. Both PPC products
build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,537 | `8033f5d982ea01d0550fb2837577f5ce27e409620147e593a011bc5702993cc7` |
| Quake3_TeamArena | 3,928,111 | `61f49e68821a9300e50aa6dd9aef5688ee6ce060de046cfa776aeeed4d62e7ed` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47/#48 open for runtime/query and further allocation/publication consumers,
aggregate budgets and full library/world rollback. Retail/Mac OS 9 execution and
target navigation acceptance remain deferred.
