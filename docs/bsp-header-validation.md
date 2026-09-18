# BSP header and lump layout validation — 2026-09-17

The first step for issue #45 adds shared bytewise preflight before collision
or renderer code performs checksums, native payload access, allocation or
world resets. Both paths retain actual FS length. Identification/version,
the complete 144-byte header and every lump range validate before a native
header copy is published. Renderer loading uses this local native header
instead of swapping the FS file header in place.

Lump offsets/lengths are nonnegative and fit the file by subtraction.
Nonempty payloads follow the header; complete typed arrays and byte-based
lightmap/lightgrid sizes fit the version-46 limits in qfiles.h. Native word
lumps align to four bytes and nonempty lumps do not overlap. Visibility has
a complete eight-byte header and enough bytes for every declared PVS row,
including the bits needed by its cluster count. Zero/end empty-lump offsets
and byte-aligned entity/lightmap/lightgrid payloads remain accepted.
Native payload readers still rely on the aligned allocation returned by FS.

On malformed layouts, both loaders free the input before controlled ERR_DROP.
Collision loading retains the previous map, checksum and patch ownership;
renderer preflight precedes world/sun/load-flag changes. The collision entity
copy now allocates one explicit terminator, including for an empty lump,
without reading beyond the declared bytes.

## Validation

ASan/UBSan exercises shared validation and actual CM_LoadMap with exact-sized
FS inputs. A minimal native collision-map golden checks loaded arrays,
checksum/cache behavior, ownership and a nonterminated entity lump copied
with a bounded terminator. Every header prefix at all four input alignments
rejects before allocation/checksum/reset and retains the loaded collision map.
All-lump native-header goldens execute the bytewise validator at four alignments.

Malformed cases cover every lump's negative/huge offsets and lengths,
header overlap, out-of-file spans, wrong strides, native alignment and format
count caps, overlapping sections, short visibility headers and insufficient
PVS rows. NULL/negative/huge lengths, missing and negative-length FS results,
unchanged failed output headers and empty entity termination are also checked.
The fixture owns/releases every hunk and asserts input release before errors.
It does not execute renderer payload loading or establish payload validity.

The BSP fixture and nine Python checks pass. Both Retro68 products build
without compiler diagnostics and validate as PPC PEFs using temporary
libraries from [loading evidence](qvm-loading-validation.md). CI retains every
previous runner and invokes the new header fixture.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,691,143 | `ed4836c30b0e7e9c525b5f32b85bd4805f7d3f17b77ec3941f85cc6115ce5ff7` |
| Quake3_TeamArena | 3,839,717 | `7ebcf1f08141a2f9e3741cbbfe873edbf72b59c13265c079440854565636982b` |

## Remaining acceptance

Keep #45 open. Payload cross references, graph/cycle validation, geometry
counts/products, finite values, renderer allocation sizes and complete
transactional publication still need validation before allocation/reset.
A valid header with malformed payload can still reach unsafe legacy paths;
this step does not establish safety for untrusted maps. Commercial 1.32c
retail collision/rendering and Mac OS 9 acceptance remain deferred.
