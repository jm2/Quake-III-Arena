# MD4 layout validation — 2026-09-17

This follow-up for issue #44 passes FS length into the MD4 loader and validates
its complete layout before payload allocation, copying, shader registration
or native conversion. Invalid input leaves the direct model state unchanged;
actual failed registration frees all staged files and retains only the empty
MOD_BAD cache entry. The requested base file alone may select MD4.

Frames use checked `offsetof(md4Frame_t, bones) + numBones * sizeof(md4Bone_t)`
strides. Bones fit the renderer's 128-entry interpolation array. Frame,
LOD and surface counts fit their complete file spans rather than borrowing
unrelated MD3 limits. LOD and surface chains make forward progress within
their enclosing ranges. Writable sections are aligned and disjoint; each
surface's negative header back reference points to the actual model header.
Consumed surface/shader names terminate; triangle and bone indexes fit their
arrays. Every variable vertex header and weight span is checked, including
late surfaces and LODs. Frame bounds/radii, bone matrices, normals, texture
coordinates, weights and offsets are finite. Nonnegative radii and unordered
bounds retain the earlier MD3 metadata checks.

An absent optional bone-name table and zero-weight vertices remain accepted.
Present bone-name spans are bounded, but unused names are not interpreted as
strings. Bone reference tables are validated and converted. Skinning preserves
the original renderer's direct indexing into the complete frame bone array;
it does not introduce a remapping based on the format header's older comment.
This compatibility choice follows [id Software's renderer](https://github.com/id-Software/Quake-III-Arena/blob/master/code/renderer/tr_animation.c)
and [format definitions](https://github.com/id-Software/Quake-III-Arena/blob/master/code/qcommon/qfiles.h).

Conversion now swaps previously omitted LOD fields, surface header references
and bone-reference fields/entries. Variable-size vertices advance bytewise;
frame strides no longer derive from a null pointer or narrow a host pointer.
Actual skinning clamps negative frame indexes to zero and high indexes to
the final frame before pointer arithmetic. Appended triangle indexes use
the current vertex count as their base, preserving existing batch indexes.

## Validation

The ASan/UBSan fixture executes actual registration, MD4 conversion, animation
submission and skinning with isolated renderer imports and no GL dependency.
Exact input allocations at all four alignments exercise every prefix of a
small two-frame/two-LOD/two-surface model. Goldens check header/LOD/surface
fields, negative back references, names/shaders, complete bone matrices,
triangles, weights, reversed bone-reference lists and native skinning output.
Distinct tess index and vertex counts expose the old append bug. Runtime
INT_MIN/INT_MAX frames exercise both interpolated and final-frame output.

Malformed inputs cover signed/huge counts and offsets, zero progress in early
and late LODs/surfaces, overlap/alignment, invalid indexes and back references,
unterminated names, nonfinite data, invalid bounds/radii, weight spans,
aggregate allocation overflow and failed registration/cache cleanup. Maxima
include 128 bones, 128 weights, 999 vertices and 1999 triangles. Valid cases
also cover 2048 frames, four LODs and 40 surfaces, absent unused names,
zero bones/weights and trailing metadata without imposing MD3-only caps.

Both model fixtures and nine Python tests pass. The previous MD4 host
pointer/stride warning is removed. Both Retro68 products build without
compiler diagnostics and validate as PPC PEFs, using temporary libraries from
[loading evidence](qvm-loading-validation.md). CI invokes the new fixture.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,695,199 | `db5cf524dea019730e4bfab41533d547a57fe5fc5fc40000fe11e9f8091509d4` |
| Quake3_TeamArena | 3,843,773 | `23c3e6165b7a21717b1d8c9fe25cd1b904bf551e46768642c19b1f4808879dbf` |

## Remaining acceptance

Keep #44 open through gated MD3/MD4 merges and deferred commercial 1.32c
model/mod animation and strict-alignment PPC runtime acceptance. Host
regressions and cross-builds do not establish those target results.

Merged master `4fd62bd`, including all reviewed bot syscall steps, is
integrated. The affected sanitizer fixtures, nine Python checks and both
product builds pass; the table records the current integrated artifacts.
