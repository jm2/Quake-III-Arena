# AAS bbox float endian conversion — 2026-09-18

Issue #47 bbox coordinates are native floats in the unchanged retail AAS
layout. Using LittleLong converts their values through an integer on PowerPC,
corrupting fractional coordinates and overflowing for some encoded bit patterns.
Use LittleFloat for bbox mins/maxs, matching the other coordinate fields.
No format, syscall, QVM or commercial 1.32c interface changes are introduced.
Existing unsigned 16-bit reachability travel-time swapping is already correct
and remains unchanged.

## Validation

The original actual swap body fails an independent encoded 1.25 bbox fixture.
The new ASan/UBSan/float-cast-overflow runner executes that body under independent
identity and byte-reversal helpers. Ten finite float samples cover fractions,
signed zero, positive/negative FLT_MIN and FLT_MAX; every axis, bbox integer
field and complete round-trip byte array must match the independent encoding.
Eight unsigned 16-bit travel-time boundaries through 65535 also decode and
round-trip under both models. The native layout loader sanitizer suite remains
passing. Optimized GCC normal and release fast-math configurations pass.
These are host endian models, not PowerPC execution or live retail bot tests.

Both Retro68 products build with zero compiler diagnostics and pass PPC PEF
header validation.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,745,889 | `04240fa6381fdf53fafc0647ac8ea2356bee5600fd007a796e74f759666ffa0b` |
| Quake3_TeamArena | 3,894,463 | `646d05d0804013ef88961171197bbc70bd93660b6b589dcd4983b199e65bd43c` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for graph/reference/numeric validation before loaded publication,
aggregate arena/runtime budgets, transactional late-load replacement, writer
ownership and mover/entity-model checks. Preserve the existing retail format;
Mac OS 9 and retail acceptance remain deferred. Earlier layout evidence remains
dated, including its pending bbox audit at that time.
