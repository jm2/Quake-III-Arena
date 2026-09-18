# AAS portal and cluster reference ownership — 2026-09-18

The native loader publishes invalid portal/cluster references and inconsistent
routing slots. Before loaded publication, validate area/reach counts, signed
index spans, aggregate capacity, portal sides/local slots and inverse ownership.
Temporary heap bitmaps enforce disjoint cluster index spans and unique local
slots. Match native AAS_NumberClusterAreas: reachable areas occupy the prefix,
and every normal-area/portal-side slot has exactly one owner. Each real portal
has distinct sides and exactly one index occurrence in each declared cluster.

Preserve retail v4/v5 layouts, portal side order, reordered disjoint spans,
native payload bytes and commercial 1.32c interfaces. Unclustered native roots
remain supported. Workspace costs are checked, and nullable failures reject
without temporary ownership. No hard map-size cap is introduced.

## Validation

Six original actual-loader proofs fail: huge area-cluster and negative portal
indices, shared index spans, duplicate local slots, omitted sides and identical
side clusters still report success. Independent literal data covers both versions,
side orders, reordered spans, unclustered roots, multiple normal areas, reachable
portals and multiple portals. Fourteen accepted worlds retain all native mapping
bytes. Ninety malformed signed/one-past/count/span/inverse/slot/prefix/side cases
reject before publication, close once and clear partial logical owners.

Duplicate and partial spans reject even when aggregate counts fit. Each temporary
workspace physically releases on success/rejection; failures of either portal
bitmap or slot/side workspace reject cleanly in both versions. Clang normal/
release fast-math sanitizers and optimized GCC checks pass. Existing AAS seams,
nine Python checks, Bash syntax and diff checks pass. Both Retro68 products
build with zero compiler diagnostics and validate as PPC PEFs. The table includes
the inherited #125 complete reachability ownership fix.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,758,195 | `d73a4b80e3fd9d1cca68a807e22b28948a514db1cef839460830b478233941ff` |
| Quake3_TeamArena | 3,906,769 | `52b9d38b7f4375abf2a030334e6022ac1915717167a38a11efdd5c32d9b57964` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #47 open for full derived numeric/runtime safety, aggregate native hunk/
work budgets, physical arena recovery and transactional late replacement, plus
complete mover/entity-model and deferred retail/Mac OS 9 acceptance. These checks
establish reference/ownership bounds; they do not prove full convex geometry,
normal classification, derived routing costs or every runtime query argument.
Bots stay disabled through remaining root work. Earlier documents remain dated.
