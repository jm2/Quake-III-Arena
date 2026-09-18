# AAS portal and cluster reference ownership — 2026-09-18

The native loader publishes invalid cluster references and negative portal
indices. Validate cluster area/reach counts and subtraction-checked index spans,
aggregate referenced table capacity, portal area/cluster/local slots and every
portal index before loaded publication. Relative normal-area slots must fit
clusters; negative portal areas must match inverse ownership. Complete clustering
requires both real portal sides and matching area ownership; each real cluster's
referenced portal must belong to it. Unclustered native roots remain supported
for subsequent native clustering initialization.

Preserve retail v4/v5 layouts, side ordering, native payload bytes and commercial
1.32c QVM/syscall interfaces. No hard map-size cap or allocator policy changes
are introduced.

## Validation

Two original actual-loader proofs fail: a huge area-cluster index and negative
portal index still report success. Independent literal area/portal/cluster data
checks both versions/sides and unclustered roots: six accepted cases retain every
portal/index/cluster/settings byte. Seventy-two malformed signed/one-past/local/
count/span/inverse cases reject before publication, close once and clear partial
logical owners. Normal/release fast-math sanitizers and optimized GCC checks
pass. All native AAS/allocator seams, nine Python checks, Bash syntax and diff
checks pass. Both Retro68 products build with zero compiler diagnostics and
validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,754,093 | `59aa62b680a0a1fb977a4a2e1ab43c96b77c3b5dc25680faf0c425f1639cfc13` |
| Quake3_TeamArena | 3,902,667 | `a4c887c7304c7cb8293c274527c8bb9a399ab6b6228a057b722b8ab4cc28fe56` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for full derived numeric/runtime safety, aggregate native hunk/
work budgets, physical arena recovery and transactional late replacement, plus
complete mover/entity-model and deferred retail/Mac OS 9 acceptance. These checks
establish reference/ownership bounds; they do not prove full convex geometry,
normal classification, derived routing costs or every runtime query argument.
Bots stay disabled through remaining root work. Earlier documents remain dated.
