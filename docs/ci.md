# Continuous integration

`Portable CI` is the first automated signal layer for review and GasCity
workers. It runs on master pushes, pull requests, and manual dispatch without a
Retro68 installation, proprietary retail data, or a Mac OS 9 emulator.

Feature branch updates run through the pull-request trigger. Filtering the
push trigger to master avoids duplicate copies of the same four jobs for each
PR head while retaining checks on the merged default branch.

## Required checks

- `Scripts and review ledger`
  - parses Bash and PowerShell entry points;
  - runs both build-script help paths;
  - compiles the Python utilities;
  - requires `docs/task.md` to remain in P0-to-P3 order with exactly one direct
    link for every confirmed issue #1 through #52, allowing both open and
    completed checkboxes as the queue is worked down;
  - checks portable image decoders are included in Unix Make/Cons, Visual
    Studio, Xcode source phases and lint manifests, including the sole standard JPEG compressor APIs.
- `Packaging tools (Python 3.11)` and `(Python 3.14)`
  - validate AppleDouble entry offsets, resource data, Finder type/creator,
    and bundle flag;
  - validate MacBinary name bytes, fork lengths/padding, deterministic Mac
    dates, version fields, and CRC;
  - reject filenames that cannot be represented in MacRoman.
- `Host C regressions (ASan/UBSan)`
  - exercises actual world traversal with stock visibility/frustum/draw goldens,
    deep pending-state cleanup and all 32 dynamic-light mask bits;
  - exercises actual swept BSP traces with stock point/box/capsule clipping
    goldens, deep traversal/pruning and injected temporary-allocation failure;
  - builds a deliberately isolated portion of `q_shared.c`;
  - checks bounded extension stripping, formatting, and token termination
    under AddressSanitizer and UndefinedBehaviorSanitizer;
  - exercises the real QVM create/restart loaders with exact-sized files,
    every truncation of a small image, invalid signed ranges/counts, data-size
    overflow, larger/smaller replacement data images, and same-sized changes
    to code, counts, layout, BSS, or initial data (issue #35);
  - checks recoverable rejection, file-buffer ownership, no hunk allocation
    before validation, unchanged data on rejected restart, and valid data
    initialization/restart, plus cleanup after failed bytecode preparation;
  - exercises actual bytecode preparation with truncated operands, invalid
    opcodes/counts/branch targets, unaligned immediates, and valid translation.
  - executes synthetic QVMs through the actual interpreter to check operand
    and program stacks, CALL/JUMP targets, forged return addresses, recursive
    syscalls, faulted shutdown re-entry, and valid execution;
  - checks full-width loads, legacy masked stores, ARG boundaries, block-copy
    bounds/overlap, and bounded syscall argument snapshots;
  - checks wrapping integer arithmetic, division/modulo traps, shift counts,
    and float conversion limits;
  - checks native/compiled/interpreted VM dispatch with empty, partial, and
    twelve-parameter calls, zero padding, single argument evaluation, and
    invalid counts, and faulted VM re-entry;
  - checks common MEMSET/MEMCPY/STRNCPY trap ranges, overlap, null/negative/
    oversized buffers, terminating sources at image boundaries, and VM
    destination return values;
  - checks typed syscall buffer alignment, bounded strings, array-size
    arithmetic, nullable query/reset arguments, and nonempty string outputs;
  - exercises the real UI CD-key and parser filename outputs with exact-sized
    allocations;
  - exercises the real cgame polygon-batch and fragment filters to check
    dimension multiplication, complete array ranges, empty submissions, and
    rejection before renderer callbacks;
  - exercises actual server game-data registration and persistent access with
    private strides, saved client capacities, controlled index/slot rejection,
    and unchanged registration on failure;
  - checks native debug-polygon point limits, invalid handles, and zero-point
    line reservation;
  - checks returned connection-denial strings in the owning VM with a
    different active VM, boundary termination, aliases, and optional NULL.
  - checks actual botlib common/navigation dispatch with bounded strings,
    structs, complete/optional arrays, zero-capacity outputs, NULL map/entity
    operations, and client indices bounded by actual server allocation.
  - checks the actual bot chat dispatcher with complete output structures,
    nullable variables and their combined capacity, bounded match metadata,
    native synonym expansion/search bounds, and overlapping substring output.
  - checks actual bot action dispatch into native elementary-action routines,
    whole input output, bounded strings/vectors, separate server/bot capacities,
    invalid native client indices, allocation arithmetic, and shutdown reset.
  - checks remaining AI goal/move/weapon structures, retail inventory arrays,
    bounded output strings, optional no-goal operations, genetic arrays and
    scalar outputs, and the inclusive native random endpoint.
    Scalar indices and indirect native accesses remain under review.
  - checks actual RoQ open/run/stop, chunk capacities and short reads, final
    payloads, embedded packet boundaries/nesting, mono/stereo expansion limits,
    malformed looping movies, and cleanup before the first frame;
  - checks RoQ dimension/product limits, complete quad groups in both frame
    halves, exact-sized resampling sources and textures, hardware limits,
    native averaging, and matching preview/videoMap upload dimensions;
  - checks every full RoQ codebook truncation and fixed RGBA table entry,
    partial updates, all VQ opcodes, exact payload prefixes, control-word refill,
    both frame halves, signed/unaligned motion, and rejection before writes;
  - checks the actual BMP loader at four input alignments with every small
    file truncation, 8/16/24/32-bit pixels, palettes, row padding/offsets,
    orientation, extended headers, dimensions, and cleanup before errors;
  - checks the actual PCX loader with exact input/output allocations, every
    small-file truncation, all palette colors, literals/runs, padded scanlines,
    extents, maximum dimensions, and nonfatal failure ownership;
  - checks the actual TGA loader at four alignments with every small-file
    prefix, raw/gray/RLE pixels, IDs, packets crossing rows, native origin
    behavior, extreme dimensions, and cleanup before errors;
  - executes the actual JPEG renderer and bundled codec with exact input
    allocations, every small-file prefix, short files/refill boundaries,
    grayscale/RGB pixels, malformed tables/dimensions, complete cleanup,
    tiny screenshots, growing output and injected allocation failures;
  - executes actual MD3 registration/conversion with exact unaligned files,
    every small-file prefix, signed offsets/counts, disjoint sections, names,
    indexes, finite metadata, native format limits, LOD staging and fallback;
  - executes actual MD4 registration, complete layout validation, native endian
    conversion and animation skinning, with variable weights, LOD/surface
    progress, bones/back references, extreme frame indexes and tess bases;
  - executes native shader parsing/registration and both archive index passes
    with 0–10 stages, following definitions, quotes/comments/nesting, EOF tails,
    skipped excess-stage cinematics, native modifiers and valid/default caching,
    malformed/non-finite color/texture vectors and 1,021 native byte goldens,
    under both normal sanitizer and optimized release-fast-math configurations,
    plus native modifier/deformation bytes and every non-finite numeric field,
    sky/sun/fog/sort metadata, exact sky paths/imports and retained rejected sun state,
    native registration/remap names, lighting modes, images and extended-byte caches;
  - executes native shader archive initialization, checked file/aggregate sizes,
    reverse input release, empty startup/restarts, native paths and 4,096-file cap,
    malformed-file isolation and 768 stock duplicate/missing lookup goldens;
  - compares patch LOD propagation with 401 tiny stock-recursive oracles and
    passes a 4,096-patch chain with a 512 KB stack and no traversal allocation;
  - checks native planar face distances for finite-source product/sum/cancellation
    overflow with four-alignment state retention, plus exact finite classification
    and large-coordinate compatibility;
  - independently measures complete native collision-map allocations and checks
    exact aggregate fits and four-alignment over-budget rejection, including
    derived visibility/areas/inline indexes and actual repeated patch output;
  - executes the actual permanent allocator in release/debug modes, checking
    native cacheline/header costs, both banks, live temporary capacity, exact
    fits and failure ownership at negative/signed-maximum boundaries;
  - executes native renderer curve refinement with eight captured output
    fingerprints, finite-source overflow and workspace-failure rejection at
    four FS alignments, and actual nodraw skips before control allocation;
  - rejects finite collision controls that overflow native midpoint/distance,
    edge differences, cross products or normalization before publication,
    retaining the loaded patch and releasing file/grid/winding ownership;
  - executes native collision plane/facet budget rejection before map reset,
    checks last legal plane/border insertions and matching full-table planes,
    and retains loaded geometry and persistent debug state after preflight;
  - checks actual facet winding ownership on every missing border, full
    clipping and invalid bounds, plus exact native copies of zero to 64 points;
  - executes actual collision patch/winding refinement with captured native
    output, subdivision overflow rejection on both axes and all four FS
    alignments, plus workspace failure cleanup before world/checksum changes;
  - executes actual BSP/model allocation and null-model bootstrap with remaining
    cache-slot boundaries, complete brush descriptors/handle lookups and
    four-alignment rejection preserving registry/world/ownership;
  - validates all BSP tree/forest components before resets with actual loaders,
    native parents/point queries, cycle/alias/OOM mutations, exhaustive small
    graphs, deep/raised-budget trees and a limited test stack;
  - executes actual world loading, bounded entity tokenization and light-grid
    sampling with non-NUL/prefix text, remaps/partial settings, unsafe grid
    calculations, temporary OOM, exact one-point/corner arrays and RGB goldens;
  - executes actual collision/renderer geometry rejection before state changes,
    with finite-field mutations, native patch capacities and real face/triangle/
    curve-parser goldens at tessellation/storage boundaries;
  - executes the actual BSP lightmap loader with exact RGB input and bounded
    upload slots, including the single-source workaround, final RGB samples,
    color-coded textures, empty/partial records and aliased geometry alpha;
  - checks BSP payload material/index/span references through shared bytewise
    preflight and actual collision loading, preserving map/checksum/ownership
    before rejection, with retail area/submodel boundaries and ignored fields;
  - checks the shared collision/renderer BSP header preflight and actual
    collision loading with exact input, every header prefix, lump ranges/strides,
    actual native allocation sizes and raised map-compiler budgets,
    visibility rows, cleanup before errors and retained world state.

GitHub Actions dependencies are pinned to exact release commits, and
Dependabot is configured to propose GitHub Actions updates.

The native skin runner exercises complete default/plain surface allocations,
name and 32-surface/token boundaries, malformed exact input prefixes, file
ownership and valid/default cache reuse through the actual entry points.

The actual legacy font runner checks the unchanged 20,548-byte little-endian
layout, signed words, 256 glyph names/handles, all short input lengths and FS
alignments, malformed names/floats, publication boundaries, cache capacity and
balanced file ownership.

The enabled native FreeType ownership runner replaces only unavailable legacy
header imports in a disposable source copy. API/FS/graphics imports are isolated;
all native bodies remain unchanged. It checks face-before-input shutdown,
bitmap/page/file ownership, controlled failure/default output, checked requests
and repeated library init/shutdown. The same actual-body seam checks bounded
metric arithmetic, complete uploaded glyph rectangles across zero/small/max
sizes and pages, TGA output shapes, generated cache reuse, independent literal
legacy LE bytes and public-reader reload of saved multi-page output. It does
not test a real rasterizer.

The actual shader fixture also covers native identity-alpha skip and matching
multitexture alpha/RGB waves, rejects changes without mutating stages, and
checks real registration pass counts and cache reuse.

## Run the portable checks locally

```sh
export TMPDIR="${TMPDIR:-/var/tmp}"
bash -n build_mac.sh setup_retro68.sh tests/run_host_c_tests.sh
./build_mac.sh --help
python3 -m py_compile create_appledouble.py generate_icon_r.py macbinary_encode.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash tests/run_host_c_tests.sh
bash tests/run_vm_loading_tests.sh
bash tests/run_vm_bytecode_tests.sh
bash tests/run_vm_runtime_tests.sh
bash tests/run_vm_call_tests.sh
bash tests/run_vm_memory_trap_tests.sh
bash tests/run_ui_syscall_tests.sh
bash tests/run_cgame_syscall_tests.sh
bash tests/run_server_core_syscall_tests.sh
bash tests/run_vm_returned_string_tests.sh
bash tests/run_botlib_navigation_tests.sh
bash tests/run_botlib_chat_tests.sh
bash tests/run_botlib_actions_tests.sh
bash tests/run_botlib_ai_tests.sh
bash tests/run_roq_stream_tests.sh
bash tests/run_roq_frame_tests.sh
bash tests/run_roq_vq_tests.sh
bash tests/run_bmp_cursor_tests.sh
bash tests/run_pcx_cursor_tests.sh
bash tests/run_tga_cursor_tests.sh
bash tests/run_jpeg_io_tests.sh
bash tests/run_md3_layout_tests.sh
bash tests/run_md4_layout_tests.sh
bash tests/run_bsp_header_tests.sh
bash tests/run_bsp_reference_tests.sh
bash tests/run_bsp_lightmap_tests.sh
bash tests/run_bsp_geometry_tests.sh
bash tests/run_bsp_entity_grid_tests.sh
bash tests/run_bsp_tree_tests.sh
bash tests/run_bsp_model_capacity_tests.sh
bash tests/run_bsp_trace_tests.sh
bash tests/run_bsp_world_tests.sh
bash tests/run_bsp_patch_grid_tests.sh
bash tests/run_bsp_winding_tests.sh
bash tests/run_hunk_allocation_tests.sh
bash tests/run_bsp_lod_tests.sh
bash tests/run_shader_stage_tests.sh
bash tests/run_skin_capacity_tests.sh
bash tests/run_font_layout_tests.sh
bash tests/run_font_freetype_tests.sh
bash tests/run_shader_archive_tests.sh
bash tests/run_shader_runtime_tests.sh
bash tests/run_sky_bounds_tests.sh
pwsh -NoProfile -File ./build_mac.ps1 --help
```

PowerShell parser validation is also part of CI; see
`.github/workflows/portable-ci.yml` for the exact command.

The native shader runtime runner compiles the actual math/noise bodies in normal
and release fast-math sanitizer configurations. It compares 645 valid waveform/
color/alpha samples and 513 noise samples with the original native formulas under
the same compiler flags. Noise matches exactly in normal builds and within four
float epsilons under release reassociation. Conversion boundaries, every noise
coordinate's non-finite/extreme values and actual waveform, bulge and diffuse
consumers check overflow handling; all 8,192 native fog samples retain their
density and non-finite coordinates return zero. Graphics imports and target GPU behavior remain
deferred.

The native sky runner compares complete graphics traces and cloud vertex/UV/index
arrays with pre-fix native bodies across 1,122 finite bounds cases in normal and
release fast-math sanitizers. Non-finite sides skip before conversion; finite
extremes clamp to the cube before multiplication. Zero through eight stages share
one indexed cloud mesh, preserving draw geometry. Invalid grid/count/capacity
cases reject before buffer/counter writes and preserve backend sentinel slots.
The runner extracts the unchanged native Q_acos body from common.c into TMPDIR;
its exact source seam fails if the signature stops matching. Graphics callbacks
are isolated and do not claim live GPU acceptance.

The sky fixture also checks complete native cloud tables for 12 stable heights
(5,832 points), six extreme heights against independent sphere/direction
invariants and complete default fallback for invalid intersections/non-finite
inputs. Normal parameters/UVs match native values exactly. Release reassociation
must remain within eight float epsilons for parameters and four for normalized
UV directions; acos near an endpoint can amplify a final-bit rounding change.
The stage fixture checks that later shader failures preserve cloud state and
accepted duplicate sky fields publish only the final layer.

## What this CI does not prove

Portable CI does not compile a PowerPC PEF, preserve/inspect a Classic resource
fork inside a mounted HFS image, use retail PK3s, or exercise AGL,
DrawSprocket, InputSprocket, Sound Manager, Open Transport, Finder events, or
Mac OS 9 runtime behavior. Passing these starter checks alone does not close
security or target-runtime issues; each issue needs its specified regressions
and applicable target evidence.

For every new PR, require successful CI and a clean Codex review on the current
head, plus resolution of every CodeRabbit finding. The user confirmed that
CodeRabbit rate limits need not block a merge once this gate is satisfied.
Resolve findings, push fixes, and obtain successful checks and renewed Codex
review. Absent, pending, or failed Codex review is not a clean review.

The user has deferred retail-content and Mac OS 9 live testing to a follow-up
session. Host-tested fixes may merge with those limitations recorded; do not
mark the outstanding target acceptance checks complete.
Host-tool-only changes do not require unrelated engine or target tests unless
their issue's acceptance criteria specify them.

The next CI layers should be:

1. a legally provisioned/self-hosted Retro68 runner that builds base and Team
   Arena and validates `Joy!peff` / `pwpc`;
2. hostile-input ASan/UBSan harnesses for the P0 parser/protocol issues;
3. mounted HFS resource/Finder inspection;
4. emulator smoke tests using externally provisioned legal game data.

When adding a regression for a GitHub issue, name the issue in the test and
update its nested checkbox in [task.md](task.md); leave the issue-level
checkbox open until all required target evidence exists.
