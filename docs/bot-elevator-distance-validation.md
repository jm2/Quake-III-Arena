# Floating elevator height comparison — 2026-09-18

BotTravel_Elevator calls integer abs on a floating height difference. The implicit
conversion truncates the distance: with barrier 32.5 and distance 32.625, the old
32 < 32.5 comparison incorrectly selects the exit path. Wide/non-finite differences
and signed minimum invoke undefined integer conversion/absolute behavior.

Use fabsf for the float distance, as in the primary
[ioquake3 movement source](https://github.com/ioquake/ioq3/blob/master/code/botlib/be_ai_move.c).
No control-flow/import/layout/syscall changes are needed. Commercial 1.32c default
integer barriers retain their defined decisions; configured fractional distances
keep full native float precision. Legacy protocols and retail formats remain.

## Validation

A temporary proof extracts the exact actual condition and uses native movement/
reachability/libvar structure types. Eight original-condition proofs reproduce the
fractional decision defect, positive/negative wide values, positive/negative infinity,
NaN, coordinate-difference overflow and signed-minimum absolute overflow. The fixed
condition eliminates these integer casts. Normal and release Clang ASan/UBSan and
optimized release GCC complete safely.

All 114,695 comparisons match the original defined decisions: 16,385 signed
one-eighth distances across seven integer barriers, including native default 32.
Fractional inside/outside cases use the complete float distance. The existing actual
movement initialization fixture passes all 40 nullable-import/retry/prior-state
cases and native configured/default/brush goldens in GCC/Clang normal/fast-math;
the entire movement translation unit now compiles with zero diagnostics. This
one-line correction uses existing CI coverage without adding a duplicate condition
test to the repository. Five ledger/manifest checks and diff checks pass.

Both PPC products build with zero diagnostics and valid PEF headers:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,779,493 | `3208b370d341d61e9a4367c1bf185619449b210680f492dd39d00b7c4a07e85a` |
| Quake3_TeamArena | 3,928,067 | `502372e3324365bd229c3d3f25b483604db55784cd0c378ec8e5a4d52adb421f` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

The temporary proof covers the actual comparison and setup, not complete elevator
navigation. Movement input/query validation and other libvar consumers remain
separate work. Keep #48 aggregate acceptance open; retail/Mac OS 9 navigation stays
deferred for the user follow-up session.
