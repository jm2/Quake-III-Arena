# Derived AAS area travel-time conversion — 2026-09-18

Finite serialized reachability coordinates can overflow subtraction or squared
vector length, and the native area travel-time function casts NaN/infinity or
a positive distance at 2^31 to int. Check the derived float representation and
signed upper boundary before that cast, including release fast-math builds.
Return the maximum native uint16 travel time for those unrepresentable results.

Preserve every defined native conversion: walk/crouch/swim factors and
classification order, minimum integer time, and the existing uint16 conversion,
including its wrap for representable signed distances. Retail v4/v5 payloads,
commercial 1.32c interfaces and serialized times remain unchanged.

## Validation

Three original actual-body UBSan proofs fail on NaN, a finite 2^31 distance and
infinity derived from finite coordinates. The runner extracts actual native
classification/factors/conversion through strict source seams and uses the real
vector-length body. Independent literal speed/minimum/multi-axis/signed-zero and
uint16/signed-boundary goldens pass. Every coordinate position/direction/class
rejects nonfinite inputs and finite overflowing lengths before integer casts.
Clang normal/release fast-math sanitizers and optimized GCC checks pass.

## Remaining acceptance

Keep #47 open for derived routing allocation/work budgets, cache accumulation,
all runtime query indices, full geometric consistency and transactional late
replacement/physical arena recovery, plus complete mover and deferred retail/
Mac OS 9 execution. Returning a bounded time does not make the entire routing
engine safe; bots remain disabled through the remaining roots.
