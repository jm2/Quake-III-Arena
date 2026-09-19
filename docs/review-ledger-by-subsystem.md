# Quake III Arena Mac OS 9 subsystem evidence appendix

Last updated: 2026-09-18

This preserves the first review session's subsystem-oriented evidence and
local-fix notes. It is not the work-order queue. Use the priority/severity
sorted [task.md](task.md) as the authoritative handoff and update both files
when evidence here materially changes.

The July candidate changes are committed in `204fe36`. The September
[reassessment](review-2026-09-17.md) supersedes their old worktree status;
July build hashes and runtime limitations remain historical evidence.
Issue closure follows the applicable acceptance criteria and the PR/CI/bot
review gates in [task.md](task.md). Nested checkboxes record only the stated
implementation or check, without implying issue completion.

## Baseline and preserved evidence

- [x] Target `jm2/Quake-III-Arena`, branch `master`.
- [x] Review baseline commit `abe5028`.
- [x] Preserve the pre-existing untracked `q3-logs/` directory; its latest
      captured startup ends at `Couldn't load default.cfg`.
- [x] Confirm the fork had no issues before this review and enable issue-based
      tracking.
- [x] Initial from-scratch base-game Release cross-build in
      `/tmp/q3-macos9-clean-20260728`.
  - [x] PowerPC PEF header: `Joy!peff` / `pwpc`.
  - [x] Size: 3,661,116 bytes.
  - [x] SHA-256:
        `8d9ee4e0ad9d59576a09767086769982071c4d3b8f182893db73702e25792b64`.
- [x] Final current-worktree clean base build in
      `/tmp/q3-macos9-final-20260728`.
  - [x] Build log has no compiler `warning:` or `error:` diagnostics.
  - [x] PEF header: `Joy!peff` / `pwpc`.
  - [x] Size: 3,665,277 bytes.
  - [x] SHA-256:
        `8b65cc3dde86c6f4a7c17448757b14e76f8619af4d3fae80973df3f1a51c9f30`.
- [x] Latest security-review patch set builds with Retro68:
  - [x] Base PEF: 3,669,561 bytes, SHA-256
        `8a23e2225ce5e21f253f7f155a0d601ae5d659685e558150061eba677305115c`.
  - [x] Team Arena PEF: 3,818,135 bytes, SHA-256
        `b7fcfbc21c63c360b0b9afc3f1d5e34e5589317fd1c4526f90706e150713e480`.
  - [x] Both identify as PowerPC PEF and begin `Joy!peff` / `pwpc`.
- [ ] Boot the current PEF on real Mac OS 9 or an emulator with legal retail
      `baseq3` data.
- [ ] Capture a fresh console/flight-recorder log through menu entry, local map
      load, spawn, input, audio, disconnect, and clean quit.

## Review protocol

- [x] Inventory build entry points, Mac-specific sources, historical plan, and
      recent stabilization/security commits.
- [x] Complete independent read-only platform and engine/game/UI audit passes.
- [x] Convert the first 52 independently confirmed root causes into GitHub
      issues.
- [x] Audit a clean build log and classify every warning that can indicate
      truncation, ABI mismatch, undefined behavior, or ignored status.
      No compiler warnings were emitted by the current clean base build.
- [ ] Add host-runnable regression tests for parser tokens, queue ownership,
      startup argument bounds, path validation, and message bounds.
- [ ] Add a repeatable Classic Mac OS runtime smoke harness/checklist.
- [x] Complete a first-pass comparison against ioquake3 commit
      `588393618dbc82e7207c21c6ddecca229944a03a`; record accepted and confirmed
      missing families in [security-provenance.md](security-provenance.md).
- [ ] Complete the provenance matrix for every remaining upstream security
      family and add a regression for every accepted backport.
- [ ] Link fixing commits and target-test evidence from this ledger and each
      issue.

## Open issue ledger

### Build, package, resources, and release inputs

- [ ] [#1 — incomplete Retro68/OpenGL readiness checks](https://github.com/jm2/Quake-III-Arena/issues/1)
  - [x] Bash/PowerShell local repair and complete-prerequisite checks drafted.
  - [x] Clean base cross-build succeeds with prepared prerequisites.
  - [ ] Test automatic repair from compiler-only and raw-SDK-only states.
  - [ ] Run the PowerShell setup/build path on Windows.
- [ ] [#2 — disabled Team Arena consumed stale binaries/assets](https://github.com/jm2/Quake-III-Arena/issues/2)
  - [x] All local conversion, validation, and staging gates follow the selected
        option.
  - [x] Bash OFF-with-stale-binary and ON cross-builds passed.
  - [ ] Run equivalent PowerShell checks on Windows.
- [ ] [#8 — demo fallback produced an invalid full-game package](https://github.com/jm2/Quake-III-Arena/issues/8)
  - [x] Local scripts now fail clearly without retail `baseq3/pak0.pk3`.
  - [x] Retail-only scope is documented in `building-mac-os9.md`.
  - [ ] Boot a generated package with legal retail data.
- [ ] [#9 — Finder creator differed from the BNDL signature](https://github.com/jm2/Quake-III-Arena/issues/9)
  - [x] Local resources, generator, and package mappings use `IDQ3`.
  - [ ] Mount and inspect a produced HFS artifact on target.
- [ ] [#22 — unverified asset selection and unpinned downloads](https://github.com/jm2/Quake-III-Arena/issues/22)
  - [x] Local scripts copy sibling paks and require missionpack data when
        Team Arena is enabled.
  - [ ] Add explicit asset roots, identity/digest checks, pinned downloads, and
        offline tests.
- [ ] [#23 — package could succeed without a complete Classic app](https://github.com/jm2/Quake-III-Arena/issues/23)
  - [x] Local scripts require MakePEF, Rez/resource output, and an HFS-capable
        image tool.
  - [x] PowerShell local PEF validation now checks `Joy!peff`, `pwpc`, and size.
  - [x] Local PowerShell packaging removes stale outputs, checks native/Python
        exit codes, and requires fresh nonempty images/MacBinary files.
  - [x] Missing icon resources now fail instead of generating a dummy resource.
  - [ ] Mount and validate PEF, resource fork, `cfrg`, `BNDL`, icon, Finder
        type/creator/flags for every package path.
- [ ] [#27 — build target selection was cache-dependent](https://github.com/jm2/Quake-III-Arena/issues/27)
  - [x] Local Bash/PowerShell scripts expose `--base-only` and `--team-arena`
        and always pass the selected value to CMake.
  - [x] Bash base-only build passed after alternating from Team Arena ON.
  - [x] Latest Team Arena ON build passed, then the workspace was returned to
        base-only.
  - [ ] Exercise both choices and package mode on PowerShell.
- [ ] [#28 — Release is forced to `-O0 -g` and GL fast paths are disabled](https://github.com/jm2/Quake-III-Arena/issues/28)
- [ ] [#31 — generated color icons use indexes without a matching CLUT](https://github.com/jm2/Quake-III-Arena/issues/31)
- [ ] [#32 — MacBinary output has invalid zero dates](https://github.com/jm2/Quake-III-Arena/issues/32)
  - [x] Local encoder writes input-mtime or `SOURCE_DATE_EPOCH` as valid Mac
        creation/modification dates.
  - [x] Host tests check deterministic dates and CRC using `binascii.crc_hqx`.
  - [ ] Validate with a complete independent decoder and on target.
- [ ] [#50 — Windows Rez include path was nonexistent](https://github.com/jm2/Quake-III-Arena/issues/50)
  - [x] Local packaging probes the prepared/source layouts and requires
        `Types.r` plus `CodeFragments.r`.
  - [ ] Run native Windows resource compilation and packaging.
- [ ] [#51 — setup required unrelated tools for cached inputs](https://github.com/jm2/Quake-III-Arena/issues/51)
  - [x] Local setup removes unused `hmount` and requires `wget` only on the
        download path.
  - [ ] Add controlled-PATH cached/partial/missing input tests.
- [ ] [#52 — MacBinary counted characters instead of encoded bytes](https://github.com/jm2/Quake-III-Arena/issues/52)
  - [x] Local encoder strictly encodes MacRoman first, then truncates/counts
        bytes and validates 32-bit fork lengths.
  - [x] Host fixtures cover 64-byte MacRoman truncation and unrepresentable input.
  - [ ] Add exact 63-byte/short non-ASCII cases and independent-reader validation.

### Mac platform, renderer, input, sound, and networking

- [ ] [#3 — anisotropic cvar dereference before registration](https://github.com/jm2/Quake-III-Arena/issues/3)
  - [x] Local registration fix cross-builds.
  - [ ] Exercise the extension path on compatible target hardware.
- [ ] [#5 — sound backend remains incomplete/default-disabled](https://github.com/jm2/Quake-III-Arena/issues/5)
  - [x] Removed the unconditional sound-init return; Mac sound is now opt-in.
  - [x] Local command registration cleanup, DMA ring position, and callback-UPP
        ownership fixes cross-build.
  - [ ] Check Sound Manager command failures and validate playback/restarts on
        target before enabling sound by default.
- [ ] [#6 — renderer forced `r_fullscreen 0`](https://github.com/jm2/Quake-III-Arena/issues/6)
  - [x] Removed the local override.
  - [ ] Test windowed/fullscreen selection and persistence.
- [ ] [#7 — 16-bit fallback still requested 24-bit color](https://github.com/jm2/Quake-III-Arena/issues/7)
  - [x] Local 16-bit path requests 5/5/5.
  - [ ] Fault-test pixel format fallback on target.
- [ ] [#10 — startup logging always opens RetroConsole](https://github.com/jm2/Quake-III-Arena/issues/10)
- [ ] [#15 — renderer setup leaks partial AGL/DrawSprocket state](https://github.com/jm2/Quake-III-Arena/issues/15)
- [ ] [#16 — gamma snapshot failures make restore unsafe](https://github.com/jm2/Quake-III-Arena/issues/16)
- [ ] [#17 — InputSprocket trusts counts and element ordering](https://github.com/jm2/Quake-III-Arena/issues/17)
  - [x] Local element-count clamp and missing-axis guard cross-build.
  - [ ] Replace index assumptions with kind/label mapping and transactional
        initialization/focus handling.
- [ ] [#18 — event queue overflow leaked payloads/could latch input](https://github.com/jm2/Quake-III-Arena/issues/18)
  - [x] Local queue now frees the evicted payload and preserves the newest
        event.
  - [ ] Add a focused ownership/key-transition stress test.
- [ ] [#19 — event pumping and application handlers are incomplete](https://github.com/jm2/Quake-III-Arena/issues/19)
  - [x] Local event code no longer reads an undefined `EventRecord` when no OS
        event arrived.
  - [ ] Implement bounded pumping, activation/key release, close/quit, and
        declared AppleEvents.
- [ ] [#20 — Open Transport ignores critical failures/network cvars](https://github.com/jm2/Quake-III-Arena/issues/20)
- [ ] [#21 — dedicated server busy-spins without console input](https://github.com/jm2/Quake-III-Arena/issues/21)
- [ ] [#33 — synchronous DNS can freeze the client for ten seconds](https://github.com/jm2/Quake-III-Arena/issues/33)
- [ ] [#34 — AGL console commands re-register on renderer init](https://github.com/jm2/Quake-III-Arena/issues/34)
  - [x] Local guard is set after first registration.
  - [ ] Loop `vid_restart` and renderer failure/retry paths on target.
- [ ] [#24 — startup command-line stack overflow](https://github.com/jm2/Quake-III-Arena/issues/24)
  - [x] Local bounded construction exits before overflow.
  - [ ] Add exactly-fitting/overlong host regression tests.
- [ ] [#25 — fatal engine errors exited with status zero](https://github.com/jm2/Quake-III-Arena/issues/25)
  - [x] Local fatal path shuts down and exits 1.
  - [ ] Verify normal/fatal status and log flushing under emulator automation.

### Filesystem, VM, client, Team Arena, and gameplay

- [ ] [#4 — `Sys_ListFiles` hardcoded filenames](https://github.com/jm2/Quake-III-Arena/issues/4)
  - [x] Local Catalog Manager enumeration supports directories, extensions,
        recursive filters, and custom content.
  - [ ] Validate HFS path resolution, aliases, recursion, and mods on target.
- [ ] [#11 — Team Arena parser cannot represent retail menu syntax](https://github.com/jm2/Quake-III-Arena/issues/11)
  - [x] Local parser now classifies numeric/signed numeric values and validates
        full file reads.
  - [ ] Restore full punctuation/preprocessor/source-location semantics.
  - [ ] Parse the retail missionpack corpus and traverse all menu flows.
- [ ] [#12 — Team Arena skips model and bot discovery](https://github.com/jm2/Quake-III-Arena/issues/12)
- [ ] [#13 — static modules override QVM-only mods](https://github.com/jm2/Quake-III-Arena/issues/13)
- [ ] [#14 — intro/idlogo cinematics are bypassed](https://github.com/jm2/Quake-III-Arena/issues/14)
- [ ] [#26 — recording plus open console froze client time](https://github.com/jm2/Quake-III-Arena/issues/26)
  - [x] Removed the port-added `msec = 0` condition.
  - [ ] Record/open-console runtime regression test.
- [ ] [#30 — stereo begins two eyes but renders one centered frame](https://github.com/jm2/Quake-III-Arena/issues/30)

### Security assurance

- [ ] [#29 — modern CVE coverage lacks provenance/tests](https://github.com/jm2/Quake-III-Arena/issues/29)
  - [x] Treat the “all modern CVEs fixed” claim as unverified in review docs.
  - [x] Add the first authoritative upstream-commit/local-code status matrix.
  - [ ] Finish the advisory inventory and malformed-input regression matrix.
- [ ] [#35 — interpreted QVM validation and sandbox bounds](https://github.com/jm2/Quake-III-Arena/issues/35)
  - [x] Create/restart header, file-range, allocation arithmetic, and restart
        size/image checks have host ASan/UBSan regressions and both Retro68 builds;
        see [loader evidence](qvm-loading-validation.md). Final syscall completeness
        audit and retail target compatibility remain open.
  - [x] Validate opcode/operand boundaries and conditional branch targets
        before interpreter setup; see [bytecode evidence](qvm-bytecode-validation.md).
  - [x] Runtime stack/control-flow checks and faulted shutdown re-entry have
        synthetic execution regressions; see [runtime evidence](qvm-runtime-validation.md).
  - [x] Data-image loads/stores, ARG, BLOCK_COPY, and syscall argument
        snapshots have [execution regressions](qvm-memory-validation.md).
  - [x] Arithmetic edge cases have [execution regressions](qvm-arithmetic-validation.md).
  - [x] The QVM libc and shipping static-module format sinks have bounded,
        terminating [formatter regressions](qvm-format-validation.md), and all
        six retail QVM products link with the formatter.
  - [x] Native/compiled/interpreted calls use counted, zero-padded arguments;
        see [call evidence](qvm-call-validation.md).
  - [x] Shared memory/string syscall traps have [range regressions](qvm-memory-trap-validation.md).
  - [x] UI syscall argument ranges and output boundaries have
        [sanitizer regressions](qvm-ui-syscall-validation.md).
  - [x] Cgame syscall ranges, polygon batches, and fragment capacities have
        [sanitizer regressions](qvm-cgame-syscall-validation.md).
  - [x] Core server traps, persistent game-data arrays, and native debug
        capacities have [sanitizer regressions](qvm-server-core-validation.md).
  - [x] Returned connection-denial strings have
        [ownership and termination regressions](qvm-returned-string-validation.md).
  - [x] Botlib common/navigation trap ranges, empty output capacities, and
        allocated client indices have [sanitizer regressions](qvm-botlib-navigation-validation.md).
  - [x] Bot chat buffers, cumulative variables, bounded in-place synonyms,
        and embedded match spans have [sanitizer regressions](qvm-botlib-chat-validation.md).
  - [x] Elementary-action VM arguments and native allocated client bounds
        have [sanitizer regressions](qvm-botlib-actions-validation.md).
  - [x] Remaining bot AI structures, inventory/rank arrays, optional goals,
        and native genetic endpoints have [sanitizer regressions](qvm-botlib-ai-validation.md).
  - [ ] Complete remaining syscall pointer/range handling before accepting
        untrusted QVMs.
- [ ] [#36 — oversized/truncated PK3 entries](https://github.com/jm2/Quake-III-Arena/issues/36)
  - [x] Local code rejects unrepresentable sizes before casts, checks unzip
        opens/reads, rejects short reads, and fixes short-name suffix checks.
  - [x] Base and Team Arena PPC cross-builds pass.
  - [x] Embedded inflate callback types and allocation products have actual
        ZIP/read/physical-release [regressions](unzip-allocation-validation.md).
  - [ ] Add malicious ZIP fixtures and prove every handle/buffer cleanup path.
- [ ] [#37 — connection/netchan lacks challenge binding](https://github.com/jm2/Quake-III-Arena/issues/37)
  - [x] User selected commercial 1.32c compatibility in the style of
        Quake3e/ioquake3 and preserving the legacy protocol by default.
  - [ ] Harden compatible challenge/setup paths, document protection limits
        for legacy peers, and test the selected behavior.
- [ ] [#38 — connectionless rate limiting is bypassable/unfair](https://github.com/jm2/Quake-III-Arena/issues/38)
- [ ] [#39 — QVMs can modify protected cvars/commands](https://github.com/jm2/Quake-III-Arena/issues/39)
- [ ] [#40 — server-controlled `clientNum` reached native indexes](https://github.com/jm2/Quake-III-Arena/issues/40)
  - [x] Local gamestate parser rejects values outside `[0, MAX_CLIENTS)`.
  - [x] Base and Team Arena PPC cross-builds pass.
  - [ ] Add malformed-gamestate tests and audit other native module indexes.

### Malformed assets, bot data, and UI allocation

- [ ] [#41 — RoQ chunk/dimension/audio/cursor bounds](https://github.com/jm2/Quake-III-Arena/issues/41)
  - [x] Chunk/audio bounds and early cleanup have [host regressions](roq-stream-validation.md).
  - [x] Frame/quad geometry and renderer source/output bounds have [host regressions](roq-frame-validation.md).
  - [x] Codebook/VQ cursors and frame preflight have [host regressions](roq-vq-validation.md).
  - [ ] Complete deferred retail cinematic acceptance.
- [ ] [#42 — BMP/PCX/TGA loaders need bounded cursors](https://github.com/jm2/Quake-III-Arena/issues/42)
  - [x] BMP checked cursors/rows/palettes have [host regressions](bmp-cursor-validation.md).
  - [x] PCX checked header/palette/RLE and padded rows have [host regressions](pcx-cursor-validation.md).
  - [x] TGA checked header/ID/raw/RLE and allocation preflight have [host regressions](tga-cursor-validation.md).
  - [ ] Complete deferred retail and strict-alignment PPC acceptance.
- [ ] [#43 — JPEG I/O is not length-aware and APIs are duplicated](https://github.com/jm2/Quake-III-Arena/issues/43)
  - [x] Local RGBA output allocation/dimension validation fixes one deterministic
        overwrite.
  - [x] Checked source/destination managers, recovery and sole compression
        APIs have [host regressions](jpeg-io-validation.md).
  - [ ] Complete deferred retail JPEG/screenshot acceptance.
- [ ] [#44 — MD3/MD4 layouts are not validated](https://github.com/jm2/Quake-III-Arena/issues/44)
  - [x] Complete MD3 layout validation before copying/swapping and staged
        LOD registration have [host regressions](md3-layout-validation.md).
  - [x] Complete MD4 layout/weight/index validation before allocation and native
        conversion has [host/build evidence](md4-layout-validation.md).
  - [x] MD3/MD4 steps #77/#79 merged after all current-head CI/review gates.
  - [ ] Complete deferred commercial 1.32c model/mod and PPC live acceptance.
- [ ] [#45 — BSP lumps/cross-references are not validated transactionally](https://github.com/jm2/Quake-III-Arena/issues/45)
  - [x] Shared collision/renderer header and lump-layout preflight has
        [host/build evidence](bsp-header-validation.md).
  - [x] Collision/material references have [host/build evidence](bsp-reference-validation.md).
  - [x] Exact RGB and duplicate lightmap uploads have [host/build evidence](bsp-lightmap-validation.md).
  - [x] Finite inputs, native control/tessellation storage and complete face
        allocations have [host/build evidence](bsp-geometry-validation.md).
  - [x] Entity/grid parsing, native grid strides and bounded edge sampling have
        [host/build evidence](bsp-entity-grid-validation.md).
  - [x] Tree/forest topology and linear parent initialization have
        [host/build evidence](bsp-tree-validation.md).
  - [x] Remaining native model-registry capacity has
        [real allocator evidence](bsp-model-capacity-validation.md).
  - [ ] Finish remaining geometry references, graph/geometry checks and full transactional
        loading, then deferred retail/PPC map acceptance.
  - [x] Overbright RGB/grid/geometry conversion handles signed cvar extremes
        without shift/product overflow; [evidence](bsp-lighting-arithmetic-validation.md).
  - [x] Deep collision box queries retain native callback/list behavior without
        recursion or query allocations; [evidence](bsp-box-query-validation.md).
  - [x] Deep projected-mark queries preserve native filters/deduplication and
        bounded lists without recursion; [evidence](bsp-mark-query-validation.md).
  - [x] Deep swept traces retain exact native clipping results with explicit
        bounded frames and owned-growth cleanup; [evidence](bsp-trace-validation.md).
  - [x] Deep world traversal preserves native culling/draw state with owned
        bounded frames; full lighting masks are defined; [evidence](bsp-world-validation.md).
  - [x] Collision patch subdivision capacity rejects before map reset and
        preserves native output; [evidence](bsp-patch-grid-validation.md).
  - [x] Native facet rejection releases winding ownership, with exact layout
        copies and repeated rejection [evidence](bsp-winding-validation.md).
  - [x] Native derived collision plane/facet/border capacity rejects before
        map reset, with staged-build [evidence](bsp-patch-budget-validation.md).
  - [x] Collision refinement/plane/winding arithmetic rejects nonfinite
        derived values before publication; [evidence](bsp-patch-numeric-validation.md).
  - [x] Renderer curves retain native output with checked derived geometry,
        attributes and bounds; [evidence](bsp-curve-numeric-validation.md).
  - [x] Permanent hunk allocations expose checked native alignment/debug costs
        and reject before bank changes; see [allocator evidence](hunk-allocation-validation.md).
  - [x] Aggregate collision allocations fit actual remaining hunk capacity,
        including derived indexes/visibility and real patch geometry; see
        [memory evidence](bsp-memory-budget-validation.md).
  - [x] Native planar face distances reject nonfinite derived results before
        world/shader/model changes; see [face evidence](bsp-face-numeric-validation.md).
  - [x] Native patch LOD propagation retains stock depth-first results with
        constant stack and no traversal allocation; see [LOD evidence](bsp-lod-validation.md).
- [ ] [#46 — shader/skin/font fixed limits and ownership](https://github.com/jm2/Quake-III-Arena/issues/46)
  - [x] Shader stage capacity checks precede native array access and rejected
        definitions preserve following parse/index/cache behavior; see
        [stage evidence](shader-stage-validation.md).
  - [x] Default/single-shader skins allocate complete native surfaces; safe
        names, 32-surface/token limits and balanced file ownership have
        [native skin evidence](skin-capacity-validation.md).
  - [x] Retail legacy font records decode bytewise from the actual FS length,
        validate fixed names/floats before imports and release input; see
        [layout/ownership evidence](font-legacy-layout-validation.md).
  - [x] Identity alpha and multitexture alpha-wave checks use their native
        alpha enums; [semantic evidence](shader-alpha-validation.md) retains
        distinct waveform passes and removes the two host enum warnings.
  - [x] Enabled native FreeType flow releases faces before backing input and
        balances temporary bitmap/page ownership on controlled failures; see
        [ownership evidence](font-freetype-ownership-validation.md).
  - [x] Optional font glyph/atlas arithmetic, complete page/final-glyph output,
        generated cache names and legacy LE serialization have
        [generation evidence](font-atlas-legacy-output-validation.md).
  - [x] Shader archives check native allocation sizes, balance list/file input,
        and clear stale state on empty restarts; see
        [archive ownership evidence](shader-archive-ownership-validation.md).
  - [x] Constant shader colors validate before byte conversion, preserve native
        valid rounding and propagate vector failures; see
        [constant/vector evidence](shader-constant-vector-validation.md).
  - [x] Archive definitions/index walks stay within their own file and preserve
        native duplicate priority across empty/multiple files; see
        [index isolation evidence](shader-file-index-validation.md).
  - [x] Waveforms, texture modifiers and deformations propagate missing/non-finite
        fields, check line/count/reciprocal limits and publish complete modifiers;
        see [numeric staging evidence](shader-wave-texmod-validation.md).
  - [x] Sky/sun/fog/sort metadata checks propagate numeric/path failures;
        rejected definitions retain previous sun state and valid fields/import
        order remain native; see [metadata evidence](shader-metadata-validation.md).
  - [x] Shader names/lightmap modes and image/remap inputs validate before
        lookup/index/publication while retaining native valid cache behavior;
        see [registration evidence](shader-registration-input-validation.md).
  - [x] Derived waveform/animation/color conversions check native int range and
        finite bits; noise reduces extreme finite cells before indexing while
        retaining its 256-cell period; see
        [runtime conversion evidence](shader-runtime-conversion-validation.md).
  - [x] Sky bounds clamp before subdivision conversion; cloud assembly validates
        capacities before writes and shares the first indexed mesh across all
        eight stages; see [sky evidence](sky-subdivision-validation.md).
  - [x] Cloud tables stage completely, preserve stable native results and use
        wide geometry for finite overflow; rejected definitions retain prior
        cloud state; see [cloud evidence](cloud-coordinate-validation.md).
  - [x] Rejected definitions and missing textures cache a complete native
        default material with discarded prefix metadata; see
        [fallback evidence](shader-fallback-validation.md).
  - [ ] Complete derived rendering conversions.
- [ ] [#47 — AAS lumps and graph indexes are trusted](https://github.com/jm2/Quake-III-Arena/issues/47)
  - [x] Local mover model boundary/look-up fixes cross-build.
  - [x] AAS v4/v5 header/lump preflight precedes world reset; exact reads,
        seeks and allocation failures close once and clear partial logical
        owners; see [AAS layout evidence](aas-layout-validation.md).
  - [x] Bbox float endian conversion retains fractional and finite extreme
        coordinates in independent endian models; existing uint16 travel-time
        swap remains unchanged; see [endian evidence](aas-endian-validation.md).
  - [x] AAS writer preserves native world bytes on every return, checks all
        writes/seeks and rejects invalid/overflowing output roots before opening;
        see [writer evidence](aas-writer-validation.md).
  - [x] Finite geometric fields, ordered bounds and edge/face/area ranges
        validate before loaded publication; signed orientations and six legacy
        plane types retain native bytes; see [geometry evidence](aas-geometry-validation.md).
  - [x] Root/node/paired-plane/area-leaf references and every component
        terminate before loaded publication; temporary heap workspace releases
        and native point queries retain results; see [node evidence](aas-node-validation.md).
  - [x] Reachability destinations/endpoints, area spans and aggregate reference
        ownership validate before loaded; ordinary references stay signed and
        special travel fields retain packed bits; see [reachability evidence](aas-reachability-validation.md).
  - [x] Portal/cluster indices, local area slots, spans and inverse ownership
        validate before loaded; native side ordering and unclustered roots
        retain bytes, while isolated-area routes preserve cache ownership;
        see [portal evidence](aas-portal-validation.md).
  - [x] Derived area travel times and cache sums saturate before overflowing
        native uint16 capacity; representable speed/minimum and route costs
        retain native results; see [travel-time evidence](aas-travel-time-validation.md).
  - [x] Routing start scratch follows incoming degree with checked heap costs
        and physical release; failed cache creation/update stays unpublished
        and can retry; see [workspace evidence](aas-routing-workspace-validation.md).
  - [x] Native routing cache counts, allocation costs and signed byte accounting
        reject before overflow/import; loaded caches are accounted and optional
        native dumps validate before publication with rebuilt runtime links;
        see [cache evidence](aas-cache-allocation-validation.md).
  - [x] Derived routing arrays check full signed costs and nullable imports;
        travel-matrix pointer rows stay aligned, partial owners release and
        failed initialization cannot enable the world;
        see [initialization evidence](aas-routing-init-validation.md).
  - [x] Routing cache cvar kilobytes convert to signed bytes without overflowing
        casts/multiplication; valid truncation/defaults remain intact;
        see [limit evidence](aas-cache-limit-validation.md).
  - [ ] Validate the complete file and graph before setting `loaded`.
- [ ] [#48 — bot preprocessor/token/path bounds](https://github.com/jm2/Quake-III-Arena/issues/48)
  - [x] Several upstream primitive/preprocessor/diagnostic bounds fixes are
        locally ported.
  - [x] Bot allocation adapters check signed import/prefix sizes, propagate
        nullable clearing and accept null cleanup while retaining heap/hunk
        ownership; see [allocator evidence](bot-memory-validation.md).
  - [x] Native numeric libvars preserve valid decimal values and reject malformed
        fractions/unrepresentable floats without trailing-NUL overread or signed
        divisor overflow; see [numeric evidence](bot-libvar-validation.md).
  - [x] Libvar value/name costs and nullable allocations preserve dictionary and
        prior value ownership on failure; aliased replacements clone before
        release; see [ownership evidence](bot-libvar-ownership-validation.md).
  - [x] Builtin date/time expansion borrows runtime time storage and releases
        copied tokens on failed/empty expansion while retaining native token
        text, types and location metadata;
        see [builtin evidence](bot-builtin-validation.md).
  - [x] Include paths append complete strings with NUL capacity, reject overflow
        and incomplete angle directives without partial lookups, preserve
        next-line tokens and release recursively rejected scripts;
        see [include evidence](bot-include-validation.md).
  - [x] Token stringize/paste operations reserve closing quotes and NUL space,
        reject without partial output, and release private argument/output
        chains after overflow, malformed arguments or nullable token copies;
        see [macro evidence](bot-macro-validation.md).
  - [x] Character filenames check complete native prefix/path costs, indexes
        80 and larger reject before narrowing/assignment, and selected skill
        EOF/nullable character-string imports release partial owners; real
        quote stripping handles overlap and empty text;
        see [character evidence](bot-character-validation.md).
  - [x] Native script/source creation checks complete signed buffer and path
        costs, rejects short reads, closes failed files and releases partial
        script/table/dictionary/global-copy owners; native byte classification
        and punctuation indexing avoid signed high-byte indexes;
        see [source evidence](bot-source-validation.md).
  - [x] Individual global definition deletion unlinks before freeing, preserves
        definitions copied by existing sources and exposes only remaining
        originals to later sources; native duplicate/case semantics remain;
        see [registry evidence](bot-global-validation.md).
  - [x] Character interpolation publishes only complete header/string owners,
        releases partial output after nullable imports and preserves input
        characters and ordinary native values;
        see [interpolation evidence](bot-interpolation-validation.md).
  - [x] Default inheritance clones missing strings before changing fields,
        rejects failed cached/new/reload targets, preserves existing owners
        and releases only newly loaded failed targets;
        see [default evidence](bot-default-validation.md).
  - [x] Numeric tokens reject unsigned overflow before wrap/index assignment,
        preserve native bases/suffixes, and check float conversion/fraction
        costs with bounded auxiliary integers;
        see [number evidence](bot-number-validation.md).
  - [x] Numeric escape values clamp before signed overflow, invalid escapes
        fail without output-byte mutation, and legacy literal/punctuation
        helpers keep quote consumption and buffer cursors bounded;
        see [escape evidence](bot-escape-validation.md).
  - [x] Source errors retain private status through suppressed diagnostics,
        string lookahead and include unwinding; failed lexical readers preserve
        queued owners and cannot continue into parents;
        see [error evidence](bot-source-error-validation.md).
  - [x] Weight configurations reject source errors before cache publication,
        release current names/partial trees on failure, and roll back nullable
        configuration/name/separator imports in cached and reload modes;
        see [weight evidence](bot-weight-validation.md).
  - [x] Public characteristic integer getters reject non-finite/unrepresentable
        casts; finite bounded floats clamp before conversion while representable
        truncation, signed integer fields and native error fallbacks remain;
        see [integer evidence](bot-character-integer-validation.md).
  - [x] Public characteristic string output ignores missing/nonpositive buffers
        before size subtraction/copy, preserving exact native truncation/NUL
        padding for every valid capacity;
        see [string evidence](bot-character-string-validation.md).
  - [x] Character numeric fields reject unrepresentable/non-finite floats and
        oversized native integer words before publication; prior strings/source
        owners release, while valid float and 32-bit word patterns remain;
        see [publication evidence](bot-character-numeric-validation.md).
  - [x] Requested NaN skills and unsafe cached skill casts reject before imports
        while native finite/infinity clamps, rounding/fallback/cache behavior and
        all existing character/string owners remain;
        see [skill evidence](bot-character-skill-validation.md).
  - [x] Interpolation rejects invalid endpoint/scale/result representations,
        releases staged output after numeric failure, preserves available equal
        fallback endpoints, and formats floating skill logs with matching types;
        see [blend evidence](bot-interpolation-numeric-validation.md).
  - [x] Public float getters reject invalid stored representations and NaN
        bounds while retaining exact finite clamping, signed integer conversion,
        negative zero and defined native infinity-bound behavior;
        see [float getter evidence](bot-character-float-getter-validation.md).
  - [x] Synonym loaders retain source diagnostics until validation, check aligned
        measured capacity in both passes, stage nullable writes in heap memory
        and publish/rebase one complete native hunk owner; invalid source/weight/
        changed-capacity failures release all staged owners;
        see [synonym evidence](bot-synonym-validation.md).
  - [x] Expression result tokens initialize magnitude metadata, format complete
        unsigned integer magnitudes without signed absolute overflow, reject
        non-finite float results and safely clamp auxiliary integer values;
        number/sign copies publish atomically with native text/type behavior;
        see [expression token evidence](bot-eval-token-validation.md).
  - [x] Expression collectors release copied operands on every parse/evaluation/
        nullable-copy failure, reject lexical errors and malformed dollar/defined
        grammar before publication, and retain valid macro/defined values and
        historical source-error recovery;
        see [operand evidence](bot-eval-collection-validation.md).
  - [x] Selected expression arithmetic checks signed add/subtract/multiply and
        division/remainder traps, validates shifts with native word semantics,
        avoids unused integer operations in float mode and rejects non-finite
        operands/results before publication; fractional float division remains;
        see [arithmetic evidence](bot-eval-arithmetic-validation.md).
  - [x] Failed unread-token copies preserve queued owners and record source
        status; nullable conditional pushes preserve the existing stack/skip
        count and return failure through entering directives; else/elif reuse
        complete frames and preserve failed-expression/EOF recovery;
        see [factory evidence](bot-source-factory-validation.md).
  - [x] Macro definitions stage names/parameters/body tokens before dictionary
        publication, preserve complete prior definitions on malformed/nullable
        failures, parse parameter names without expanding prior macros and clean
        external temporary dictionaries/scripts on every failed import;
        see [definition evidence](bot-define-validation.md).
  - [x] Complete empty macro expansion reports successful consumption so readers
        continue to subsequent tokens, concatenated strings, included parents and
        lexical errors; empty-only input reaches actual EOF and valid character
        fields publish with native values;
        see [empty-expansion evidence](bot-empty-expansion-validation.md).
  - [x] Movement setup stages all ten native libvar references, rejects each
        nullable import before brush/reference publication and preserves complete
        prior state; retries retain native cached/configured values and models;
        see [movement setup evidence](bot-move-setup-validation.md).
  - [x] File loading checks block-comment closure before compression removes
        diagnostics; malformed root/include/character imports release all owners
        while valid files retain native compressed bytes and token metadata;
        see [file-comment evidence](bot-file-comment-validation.md).
  - [x] Compressed files update the EOF pointer to their complete new length;
        exhausted roots/includes unwind conditional frames and skip state while
        preserving native bytes/tokens and complete parent recovery;
        see [file EOF evidence](bot-file-eof-validation.md).
  - [x] Elevator height checks use native floating magnitude for the float
        barrier libvar, avoiding unsafe integer conversion and retaining tested
        integer-barrier behavior while correcting fractional barrier decisions;
        see [distance evidence](bot-elevator-distance-validation.md).
  - [x] AAS/public setup validates native count conversions and entity cost,
        stages nullable cache/hunk imports before replacing world state and
        preserves prior entities on failure; native defaults and truncation stay;
        see [setup evidence](aas-setup-validation.md).
  - [x] Entity proximity compares both float coordinate magnitudes directly;
        defined native constant-40 decisions remain and unsafe integer casts
        disappear; see [proximity evidence](aas-nearest-distance-validation.md).
  - [x] Action setup stages complete input storage before replacing pointer/
        actual capacity; nullable failure preserves usable prior payload, and
        successful replacement releases its logical allocator record;
        see [action setup evidence](bot-action-setup-validation.md).
  - [x] Item configuration validates native count/cost and full filenames,
        parses checked heap staging, rejects source errors and releases complete
        sources before publishing a rebased hunk owner; native item values stay;
        see [item evidence](bot-item-config-validation.md).
  - [x] Structure numeric fields stage finite float results and checked native
        integer words/bounds before writing destination bytes; 16-bit ranges and
        ordinary native field values stay;
        see [structure evidence](bot-structure-number-validation.md).
  - [x] Goal setup validates native game-type conversion and stages complete
        config/weight references before item parsing/publication; failed imports
        retain prior goal payload and successful replacement releases its logical
        record; see [goal evidence](bot-goal-setup-validation.md).
  - [x] Level-item pool validates native count/cost, stages complete free links
        before heap/list replacement and propagates failures before public map
        information reset/map-API success; game load/setup failures disable bot
        creation/frames until success while human initialization continues. Engine
        readiness survives game VM resets through an existing syscall query. Retail
        void ABI, prior pool bytes and native positive counts remain;
        see [pool evidence](bot-level-pool-validation.md).
  - [x] Map metadata stages checked locations/camps and publishes roots with
        the complete level pool; nullable failure preserves all prior bytes,
        cleans private owners and propagates through the checked map API;
        see [metadata evidence](bot-map-info-validation.md).
  - [x] Projectile model descriptors use the projectile offset, preserve every
        scalar field across string order/boundaries and retain native layouts;
        see [projectile evidence](bot-projectile-model-validation.md).
  - [x] Weapon configuration checks native counts/full filenames and complete
        allocation cost, parses checked heap staging, rejects source errors and
        fixes projectiles before publishing rebased hunk arrays; failed loads keep
        prior payload and consume no additional physical hunk;
        see [weapon evidence](bot-weapon-config-validation.md).
  - [x] Weapon setup checks its filename variable and stages complete config
        before replacing the shared root; failure keeps every prior array/fixup
        byte and success releases the prior logical record;
        see [setup evidence](bot-weapon-setup-validation.md).
  - [ ] Finish expression work limits, remaining character/source allocation
        consumers, and aggregate parser work/recursion limits.
- [ ] [#49 — Team Arena UI allocation/reload safety](https://github.com/jm2/Quake-III-Arena/issues/49)
  - [x] Local pool/type/item/string checks, reload-name initialization, and
        bounded player-model cvar copies cross-build in both products.
  - [ ] Audit every direct allocation consumer and add injected-OOM tests.

## Previously completed stabilization in repository history

- [x] Remove dangerous pre-PEF stripping/section-GC flags.
- [x] Replace module OBJECT libraries with STATIC archives and force inclusion.
- [x] Default Team Arena bring-up off in CMake.
- [x] Implement a real Classic application-directory path.
- [x] Remove synchronous per-frame disk logging.
- [x] Use 64-bit Mac timekeeping and consistent event-time bases.
- [x] Implement Classic directory creation and vector snapping.
- [x] Force interpreted VMs where native VM compilation is unavailable.
- [x] Correct Toolbox packing and add ABI guards.
- [x] Harden initial userinfo, delta messages, demo/download names, CD keys,
      `fs_game`, and client entity counts.
- [x] Restore sound shutdown on client disconnect.

## Remaining broad audit lanes

- [x] Initial packet/message, fragments, decompression, snapshot, and download
      pass completed; local patches require the corpus listed below.
- [x] Initial connectionless/rcon/challenge/rate-limit/command-injection pass
      completed; unresolved work is #37–#39.
- [x] Initial filesystem/path/ZIP/handle pass completed; unresolved malicious
      archive testing is #36 and parser work is #41–#48.
- [x] Initial VM pass completed; unresolved sandbox and privilege boundaries
      are #35 and #39.
- [x] Initial renderer model/image/shader/world pass completed; unresolved
      root causes are #41–#46.
- [x] Initial botlib/AAS pass completed; unresolved root causes are #47–#48.
- [ ] Build and run the malformed-input corpora needed to turn all six initial
      audit passes into verified fixes.
- [ ] CD-key UI behavior and persistence without exposing a real key.
- [ ] Decide whether unsupported legacy master/auth protocols should default
      off.

## Verification matrix

- [x] Bash and PowerShell scripts parse; both help paths run.
- [x] Incremental base Retro68 build and strong PEF validation pass.
- [x] From-scratch default base Retro68 build and manual MakePEF validation
      pass.
- [x] Latest Bash Team Arena ON build and both strong PEF validations pass:
      base 3,669,561 bytes; Team Arena 3,818,135 bytes.
- [ ] PowerShell build runs on Windows.
- [ ] Packaging succeeds offline from an explicit legal asset root.
- [ ] Failure modes leave no falsely successful/incomplete image.
- [ ] Mounted artifacts contain correct resource/Finder metadata.
- [ ] Base game reaches main menu and loads a local skirmish.
- [ ] Movement, view, weapons, console, menus, and clean disconnect work.
- [ ] Audio initializes, plays, restarts, and shuts down safely.
- [ ] UDP loopback/LAN connect/disconnect works and malformed packets do not
      crash or hang.
- [ ] Config/CD-key writes land beside the app and survive restart.
- [ ] Fullscreen/windowed, gamma, suspend/resume, fatal exit, and normal quit
      restore the desktop.

## Exact continuation point

Current master is `31a6554caa3c941d6657d664849511e27233089d` through #172
and independent #180. The latest dated inventory, pending stack and exact-head
gates are in [continuation evidence](review-continuation-2026-09-18.md).
Earlier milestone snapshots below remain historical.

- [x] Current progress reconciled at master `cdc8c38` on 2026-09-18: BSP
      steps #83–#91/#93–#101 and shader capacity #102 are merged after
      successful current-head CI, clean Codex and resolved CodeRabbit findings.
- [x] Skin/font/shader and ledger steps #103–#114 merged at `fc6c10e`
      after exact-head CI, clean completed Codex and resolved bot findings.
      The [September 18 snapshot](review-2026-09-18.md) retains the earlier queue.
- [x] Derived shader conversions/noise #115 merged at `aa25e88` after the
      same exact-head CI/Codex/resolved-finding gate.
- [x] Sky subdivisions/shared cloud mesh #116 merged at `e59b66d` after
      the same exact-head CI/Codex/resolved-finding gate.
- [x] Cloud math/publication #117 merged at `c4251e7` after the same
      exact-head CI/Codex/resolved-finding gate.
- [ ] Gate complete native fallback and continue renderer/AAS/preprocessor
      root causes; retain deferred acceptance.
- [ ] Finish renderer aggregate capacity/full transactional publication and
      remaining query/candidate costs for #45. Keep deferred acceptance open.
- [x] Cross-build the full local security/UI/filesystem patch set as base and
      Team Arena, validate both PEFs, then return configuration to base-only.
- [x] Run a clean rebuild with captured output and classify warnings.
- [ ] P0 security: finish #35, #41, #42, #43, #44, #45, #46, #47, #48,
      then compatibility-sensitive #37; retain #29 and #36 validation gates.
- [ ] P1 security: fix #38 and #39 with deterministic host regressions.
- [ ] Add focused host tests for local candidates #18, #24, #25, #36, #40,
      #49, message/Huffman exact bounds and download pairs.
- [x] Add normal/optimized GCC/Clang sanitizer coverage for the known native
      format-string fixes, shared error/network boundaries, and shipping
      QVM/static-module formatter capacities.
- [ ] Prioritize target runtime blockers: #11 after #48, then #15, #16, #17,
      #5, #20, #19.
- [x] User authorized host checks and working cross-builds while legal retail
      assets and a Mac OS 9 environment are unavailable.
- [ ] Execute deferred end-to-end acceptance in a follow-up session; preserve
      resulting logs outside generated package staging.
- [ ] Do not close any issue solely because a cross-build passed.
