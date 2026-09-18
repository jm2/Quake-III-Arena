# AAS portal and cluster reference ownership — 2026-09-18

The native loader publishes invalid cluster references and negative portal
indices. Validate cluster area/reach counts and subtraction-checked index spans,
aggregate referenced table capacity and disjoint index-span ownership, portal area/cluster/local slots and every
portal index before loaded publication. Relative normal-area slots must fit
clusters; negative portal areas must match inverse ownership. Complete clustering
requires both real portal sides and matching area ownership; each real cluster's
referenced portal must belong to it. Unclustered native roots remain supported
for subsequent native clustering initialization.

Preserve retail v4/v5 layouts, side ordering, native payload bytes and commercial
1.32c QVM/syscall interfaces. No hard map-size cap or allocator policy changes
are introduced.

## Validation

Three original actual-loader proofs fail: huge area-cluster and negative portal
indices, and two clusters sharing one index span, still report success. Independent literal area/portal/cluster data
checks both versions/sides, reordered index spans and unclustered roots: eight
accepted cases retain every
portal/index/cluster/settings byte. Seventy-six malformed signed/one-past/local/
count/span/inverse cases reject before publication, close once and clear partial
logical owners. Duplicate and partial spans reject even when aggregate counts
fit. A temporary heap bitmap releases on success/rejection; its nullable failure
rejects without temporary ownership in both versions. Normal/release fast-math sanitizers and optimized GCC checks
pass. All native AAS/allocator seams, nine Python checks, Bash syntax and diff
checks pass. Both Retro68 products build with zero compiler diagnostics and
validate as PPC PEFs.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,754,099 | `adb279f5bfc5b1681e4bb275c0ccafa998744db555744d612df9c555a6d54ef0` |
| Quake3_TeamArena | 3,902,673 | `6f5cdf26e233c52b7d59d90a47610e9f974dc99ce8920e262f74fb20f191c0d4` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for full derived numeric/runtime safety, aggregate native hunk/
work budgets, physical arena recovery and transactional late replacement, plus
complete mover/entity-model and deferred retail/Mac OS 9 acceptance. These checks
establish reference/ownership bounds; they do not prove full convex geometry,
normal classification, derived routing costs or every runtime query argument.
Bots stay disabled through remaining root work. Earlier documents remain dated.
