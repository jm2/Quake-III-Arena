# Derived AAS travel costs and cache sums — 2026-09-18

Finite serialized reachability coordinates can overflow subtraction or squared
vector length. The native area-distance conversion also casts NaN/infinity or a
positive distance at 2^31 to int. Check the derived float representation before
conversion, including release fast-math builds, and saturate at the maximum
native uint16 time. Check float cache-start times before their uint16 assignment.

Area/portal updates, cross-cluster route selection and hide-area distance
penalties accumulate costs in uint16 fields. Saturate additions before that
capacity overflows: a maximum-cost crossing must never wrap into a cheap route.
Preserve representable native walk/crouch/swim factors, classification order,
minimum time, cache costs and route results. Oversized costs now saturate instead
of wrapping. Retail v4/v5 payloads, commercial 1.32c interfaces, cache field widths
and serialized times remain unchanged.

## Validation

Four original actual-body proofs fail: three UBSan cases cover NaN, a finite
2^31 distance and infinity derived from finite coordinates; a native multi-area
cache proof exposes a maximum area penalty wrapping into a cheap route.

The distance runner extracts actual native classification/factors/conversion
through strict source seams and uses real vector length. Independent literal
speed/minimum/multi-axis/signed-zero goldens retain normal results. Uint16/signed
boundary goldens saturate. Every coordinate position/direction/class handles
nonfinite inputs and finite overflowing lengths before integer casts.

The cache runner exercises actual area/portal updates, cross-cluster route
selection and hide-area routing, replacing only cache providers and including
the real projection body. Literal normal/maximal costs, nonfinite/oversized
float cache starts and enemy-distance penalties cannot wrap. Clang normal/
release fast-math sanitizers and optimized GCC checks pass. Integrated portal
sanitizers, review-ledger checks, Bash syntax and diff checks pass.
Both Retro68 products rebuild with zero compiler diagnostics and valid PPC PEFs:

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,758,211 | `7bcfef5a3bb080450320b0a127b45e978db9b5580153b5752576310c5f88bd4a` |
| Quake3_TeamArena | 3,906,785 | `85d7ed88be88ace84a9e2bfc8c6b670ed7b591e214c71476b7513150b964eec3` |

The artifacts include the final portal/cluster empty-dummy ownership checks
and native isolated-goal routing guards. Integrated distance/cache/isolated-routing
sanitizers pass after merging the final source.
Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for derived routing allocations/work budgets, runtime query
indices, full geometric consistency and transactional late replacement/physical
arena recovery, plus complete mover and deferred retail/Mac OS 9 execution.
Saturating costs does not make the entire routing engine safe; bots remain
disabled through remaining roots. Historical evidence remains dated.
