# BSP header and lump layout validation — 2026-09-17

The first step for issue #45 adds shared bytewise preflight before collision
or renderer code performs checksums, native payload access, allocation or
world resets. Both paths retain actual FS length. Identification/version,
the complete 144-byte header and every lump range validate before a native
header copy is published. Renderer loading uses this local native header
instead of swapping the FS file header in place.

Lump offsets/lengths are nonnegative and fit the file by subtraction.
Nonempty payloads follow the header; typed arrays and byte-based
lightmap/lightgrid sizes contain complete records. Native word
lumps align to four bytes and nonempty lumps do not overlap. Visibility has
a complete eight-byte header and enough bytes for every declared PVS row,
including the bits needed by its cluster count. Zero/end empty-lump offsets
and byte-aligned entity/lightmap/lightgrid payloads remain accepted.
Native payload readers still rely on the aligned allocation returned by FS.

Compiler utility defaults in qfiles.h are not on-disk format caps. Each loader
checks its actual native array sizes and reserved elements against signed
allocation capacity, including combined renderer nodes/leaves, its doubled plane storage and
the collision box hull. The shared check uses division/subtraction before any
product. Raised shader/vertex/plane/map-compiler budgets remain supported.
Visibility storage fits the file without imposing its compiler byte budget.

On malformed layouts, both loaders free the input before controlled ERR_DROP.
Collision loading retains the previous map, checksum and patch ownership;
renderer preflight precedes world/sun/load-flag changes. The collision entity
copy now allocates one explicit terminator, including for an empty lump,
without reading beyond the declared bytes.

## Validation

ASan/UBSan exercises shared validation and actual CM_LoadMap with exact-sized
FS inputs. A minimal native collision-map golden checks loaded arrays,
checksum/cache behavior, 1025 shader records, ownership and a nonterminated
entity lump copied with a bounded terminator. Every header prefix at all four input alignments
rejects before allocation/checksum/reset and retains the loaded collision map.
All-lump native-header goldens execute the bytewise validator at four alignments.

Malformed cases cover every lump's negative/huge offsets and lengths,
header overlap, out-of-file spans, wrong strides, native alignment,
overlapping sections, short visibility headers and insufficient
PVS rows. NULL/negative/huge lengths, missing and negative-length FS results,
unchanged failed output headers and empty entity termination are also checked.
Allocation boundary checks cover exact capacity, one extra element, reserved
slots, impossible reservations and zero element sizes without large allocations.
Header goldens also exceed every qfiles.h utility default and use an 8 MiB
PVS payload. The fixture owns/releases every hunk and asserts input release
before errors.
It does not execute renderer payload loading or establish payload validity.

The BSP fixture and nine Python checks pass. Both Retro68 products build
without compiler diagnostics and validate as PPC PEFs using temporary
libraries from [loading evidence](qvm-loading-validation.md). CI retains every
previous runner and invokes the new header fixture.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,699,523 | `8f74c8b4987bea07cc86c8078d6a346e2e57bc34dbcd9bff3c6ebf7a8268cda1` |
| Quake3_TeamArena | 3,848,097 | `96282ace23efdf7b83607a6214cf2c23eb29d68ef289592715a5bde9c2a1cf4b` |

## Remaining acceptance

Keep #45 open. Payload cross references, graph/cycle validation, geometry
counts/products, finite values, geometry-dependent allocation sizes and complete
transactional publication still need validation before allocation/reset.
A valid header with malformed payload can still reach unsafe legacy paths;
this step does not establish safety for untrusted maps. Commercial 1.32c
retail collision/rendering and Mac OS 9 acceptance remain deferred.

Merged master `4fd62bd`, including all reviewed bot syscall steps, is
integrated. The affected sanitizer fixtures, nine Python checks and both
product builds pass; the table records the current integrated artifacts.
