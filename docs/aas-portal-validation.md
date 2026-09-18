# AAS portal and cluster reference ownership — 2026-09-18

The native loader publishes invalid portal/cluster references and inconsistent
routing slots. Before loaded publication, validate area/reach counts, signed
index spans, aggregate capacity, portal sides/local slots and inverse ownership.
Temporary heap bitmaps enforce disjoint cluster index spans and unique local
slots. Match native AAS_NumberClusterAreas: reachable areas occupy the prefix,
and every normal-area/portal-side slot has exactly one owner. Complete worlds
cannot leave reachable normal areas in cluster zero. Each real portal
has distinct sides and exactly one index occurrence in each declared cluster.

Preserve retail v4/v5 layouts, portal side order, reordered disjoint spans,
native payload bytes and commercial 1.32c interfaces. Unclustered native roots
remain supported when they have zero clusters and native initialization will
rebuild them. A dummy-only cluster table cannot own reachable areas because
initialization skips rebuilding it. Isolated areas without outgoing reachabilities can legitimately remain in
cluster zero: native AAS_FindClusters explicitly skips them when nofaceflood is
enabled. Preserve their bytes; distinct start/goal routing queries return
unreachable before cache allocation or mutation. Workspace costs are checked, and nullable failures reject
without temporary ownership. No hard map-size cap is introduced.

## Validation

Eight original actual-loader proofs fail: huge area-cluster and negative portal
indices, shared index spans, duplicate local slots, omitted sides and identical
side clusters, reachable cluster-zero orphans and reachable dummy-only roots
still report success. Independent literal data covers both versions,
side orders, reordered spans, unclustered roots, multiple normal areas, reachable
portals and multiple portals. Sixteen accepted worlds retain all native mapping
bytes. Ninety-four malformed signed/one-past/count/span/inverse/slot/prefix/side cases
reject before publication, close once and clear partial logical owners.

Duplicate and partial spans reject even when aggregate counts fit. Each temporary
workspace physically releases on success/rejection; failures of either portal
bitmap or slot/side workspace reject cleanly in both versions. Clang normal/
release fast-math sanitizers and optimized GCC checks pass. Existing AAS seams,
nine Python checks, Bash syntax and diff checks pass. Both Retro68 products
build with zero compiler diagnostics and validate as PPC PEFs. The table includes
the inherited #125 complete reachability ownership fix, both reachable
cluster-zero orphan follow-ups and the isolated-area routing guard. The original actual-loader proofs fail; fixed normal/fast
sanitizers and both PPC rebuilds pass with zero product diagnostics.

One original actual-routing proof also fails: an isolated nonreachable goal
allocates caches and aliases another cluster's cache storage. The regression
runs actual AAS_FindClusters/AAS_NumberClusterAreas and routing/cache consumers,
proving native isolated areas remain unclustered and distinct queries leave
cache tables, physical owners and the LRU list unchanged. Clang normal/fast-math
sanitizers and optimized GCC checks pass.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,758,195 | `89d2fea8e3f9875dc8a636ff6cdcab6593ccbba095245650195690863dedf743` |
| Quake3_TeamArena | 3,906,769 | `a215311d19fc3b4f217d59a60b4a31692517b52c113debbd9a09681dde263e01` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for full derived numeric/runtime safety, aggregate native hunk/
work budgets, physical arena recovery and transactional late replacement, plus
complete mover/entity-model and deferred retail/Mac OS 9 acceptance. These checks
establish reference/ownership bounds; they do not prove full convex geometry,
normal classification, derived routing costs or every runtime query argument.
Bots stay disabled through remaining root work. Earlier documents remain dated.
