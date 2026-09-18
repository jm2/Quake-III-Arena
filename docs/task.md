# Quake III Arena Mac OS 9 prioritized review queue

Last updated: 2026-09-18

This is the authoritative continuation ledger. Work is sorted first by
priority (`P0` through `P3`), then by severity and exploit/runtime impact
within each priority. Every confirmed GitHub issue from #1 through #52 appears
exactly once below. The subsystem evidence appendix is
[review-ledger-by-subsystem.md](review-ledger-by-subsystem.md).

An issue-level checkbox stays open until its fix is merged, its acceptance
criteria and applicable checks pass, and the linked GitHub issue is closed.
Engine changes require both Retro68 product builds and any Mac OS 9 tests
specified by the issue. Host-tool changes require the relevant host/format
checks; they do not automatically require an unrelated engine rebuild.
A checked nested item records only the stated implementation or check.

Every new step goes through a pull request. Before merging, require successful
CI for its current head and a completed clean Codex review, with every
CodeRabbit finding resolved. After fixes, re-run affected checks and obtain
a renewed clean current-head Codex review. Do not interpret absent, pending,
failed or unavailable CI/Codex review as clean. CodeRabbit rate limits or
skipped reviews do not replace the required Codex review; the user does not
require waiting for a follow-up CodeRabbit review during rate-limit backoff
once every finding is resolved and the CI/Codex gate passes.
Keep issues open when a merged step covers only part of their acceptance
criteria, and link the PR and remaining evidence in the issue.

The [2026-09-17 reassessment](review-2026-09-17.md) records the disposition of
all 52 issues, implementation dependencies, accepted decisions, and deferred
acceptance checks.
The July evidence below is historical unless explicitly dated otherwise;
“local” candidate fixes from that pass are committed in `204fe36`.

## Baseline and current evidence

- [x] Repository: `jm2/Quake-III-Arena`, branch `master`.
- [x] Original July review baseline:
      `abe5028afda280240d48d012a62c09e713689fd2`.
- [x] September source/issue baseline: `204fe36deccfde8ead94f6bba0422dd8c1ba396e`;
      all 52 issues remain open and there were no existing PRs.
- [x] Preserve the pre-existing untracked `q3-logs/`; its latest captured
      startup ends at `Couldn't load default.cfg`.
- [x] First-pass security comparison anchored to ioquake3
      `588393618dbc82e7207c21c6ddecca229944a03a` (2026-07-16).
- [x] Security mapping recorded in
      [security-provenance.md](security-provenance.md).
- [x] 52 confirmed root causes opened in the
      [GitHub issue tracker](https://github.com/jm2/Quake-III-Arena/issues).
- [x] July locally tested base PEF: `Joy!peff` / `pwpc`, 3,669,561 bytes,
      SHA-256
      `8a23e2225ce5e21f253f7f155a0d601ae5d659685e558150061eba677305115c`.
- [x] July locally tested Team Arena PEF: `Joy!peff` / `pwpc`,
      3,818,135 bytes, SHA-256
      `b7fcfbc21c63c360b0b9afc3f1d5e34e5589317fd1c4526f90706e150713e480`.
- [x] Re-record the two PEF sizes/hashes after the final source changes in
      this review pass; the build cache is left with `BUILD_TEAM_ARENA=OFF`.
- [ ] Boot current artifacts on Mac OS 9 hardware/emulator with legal retail
      baseq3 and missionpack data.
- [ ] Capture menu, local-map, spawn, input, audio, networking, disconnect,
      normal quit, and fatal-exit evidence.

## P0 — security release gates

Do not describe the port as safe for untrusted servers, mods, PK3s, maps, or
QVMs while any P0 item is open.

- [ ] [#29 — complete security provenance and regression coverage](https://github.com/jm2/Quake-III-Arena/issues/29)
      — **assurance gate**.
  - [x] First upstream/local status matrix and GitHub provenance comment added;
        the matrix is committed in `204fe36`, not an uncommitted draft.
  - [ ] Complete the advisory inventory and add a malformed-input regression
        for every accepted security family.
- [ ] [#35 — harden interpreted QVM validation and sandbox bounds](https://github.com/jm2/Quake-III-Arena/issues/35)
      — **high**, native memory corruption from malformed QVMs.
  - [x] Shared create/restart header validation checks file ranges, signed
        allocation arithmetic, initialized-word alignment, and unchanged
        restart allocation size/image. Host ASan/UBSan loader regressions and both
        Retro68 product builds pass; see [loader evidence](qvm-loading-validation.md).
  - [x] Validate bytecode decoding and branch targets before interpreter setup,
        with unaligned little-endian reads and host ASan/UBSan regressions;
        see [bytecode evidence](qvm-bytecode-validation.md).
  - [x] Enforce operand/program stack bounds and CALL/JUMP/return targets
        during normal execution, including safe faulted shutdown re-entry;
        see [runtime evidence](qvm-runtime-validation.md).
  - [x] Bound full-width data accesses, stack arguments, block copies, and
        syscall argument snapshots; see [memory evidence](qvm-memory-validation.md).
  - [x] Define wrapping integer arithmetic and reject division, shift, and
        float conversion traps; see [arithmetic evidence](qvm-arithmetic-validation.md).
  - [x] Marshal counted VM call arguments and zero padding explicitly for
        all execution modes; see [call evidence](qvm-call-validation.md).
  - [x] Common MEMSET/MEMCPY/STRNCPY syscalls validate full buffers and
        string boundaries; see [trap evidence](qvm-memory-trap-validation.md).
  - [x] UI syscall strings, buffers, structs, and arrays validate full ranges
        and alignment; CD-key and parser filename output honor their bounds;
        see [UI evidence](qvm-ui-syscall-validation.md).
  - [x] Cgame syscall structures, strings, vectors, and arrays validate full
        ranges, including polygon batches and fragment output capacities;
        see [cgame evidence](qvm-cgame-syscall-validation.md).
  - [x] Core server syscall ranges, persistent game-array registration and
        indices, and native debug-polygon capacities have sanitizer coverage;
        see [server evidence](qvm-server-core-validation.md).
  - [x] Connection-denial strings validate termination in their owning VM
        after calls return; see [return evidence](qvm-returned-string-validation.md).
  - [x] User approved the original string length as the safe in-place limit
        for the size-less legacy synonym syscall; preserve the 1.32c ABI and
        skip growing replacements when they cannot fit. This merged in PR #66.
  - [x] Botlib common/navigation trap ranges, empty output capacities, and
        allocated client indices have [sanitizer regressions](qvm-botlib-navigation-validation.md).
  - [x] Bot chat buffers, cumulative variables, bounded in-place synonyms,
        and embedded match spans have [sanitizer regressions](qvm-botlib-chat-validation.md).
  - [x] Elementary-action VM arguments and native allocated client bounds
        have [sanitizer regressions](qvm-botlib-actions-validation.md).
  - [x] Remaining bot AI structures, inventory/rank arrays, optional goals,
        and native genetic endpoints have [sanitizer regressions](qvm-botlib-ai-validation.md).
  - [ ] Complete remaining syscall pointer/range checks and retain valid PPC
        QVM compatibility during deferred live acceptance.
- [ ] [#37 — bind connection and netchan packets to negotiated challenges](https://github.com/jm2/Quake-III-Arena/issues/37)
      — **high**, connection redirection/injection/hijack.
  - [x] User selected commercial 1.32c compatibility (Quake3e/ioquake3 style).
        Preserve legacy protocol compatibility by default; harden compatible
        paths without requiring a different wire protocol.
  - [ ] Document protection limits for legacy peers and test compatible setup,
        rejection of spoofed responses, and any explicitly negotiated extension.
- [ ] [#36 — reject oversized and truncated PK3 entries](https://github.com/jm2/Quake-III-Arena/issues/36)
      — **high**, ZIP-controlled allocation/decompression corruption.
  - [x] Local size/cast, exact-read, open-result, cleanup, and short-suffix
        checks cross-build.
  - [ ] Add malicious ZIP fixtures around caps, `INT_MAX`, `UINT32_MAX`, and
        truncated streams.
- [ ] [#41 — bound RoQ chunks, dimensions, audio output, and cursors](https://github.com/jm2/Quake-III-Arena/issues/41)
      — **high**, deterministic cinematic buffer corruption.
  - [x] Bound disk/packet payloads, exact reads, mono/stereo output and early
        failure cleanup; see [chunk/audio evidence](roq-stream-validation.md).
  - [x] Validate frame geometry, both halves, complete quad groups and
        bounded preview/videoMap resizing; see [frame evidence](roq-frame-validation.md).
  - [x] Check codebook/VQ ranges and all motion-source rows/columns before
        any frame write; see [VQ evidence](roq-vq-validation.md).
  - [ ] Complete deferred retail 1.32c and Mac OS 9 cinematic acceptance.
- [ ] [#42 — replace BMP/PCX/TGA loaders with bounded cursors](https://github.com/jm2/Quake-III-Arena/issues/42)
      — **high**, deterministic image heap/OOB corruption.
  - [x] BMP headers/palettes/rows use checked byte spans and allocation
        arithmetic, with [host regressions](bmp-cursor-validation.md).
  - [x] PCX header/palette/RLE bounds and complete padded rows have
        [host regressions](pcx-cursor-validation.md).
  - [x] TGA header/ID/raw/RLE bounds and rejection before allocation have
        [host regressions](tga-cursor-validation.md).
  - [ ] Complete deferred retail 1.32c and strict-alignment PPC acceptance.
- [ ] [#43 — make JPEG I/O length-aware and remove duplicate APIs](https://github.com/jm2/Quake-III-Arena/issues/43)
      — **high**, OOB decode and link-order ambiguity.
  - [x] Local RGBA output sizing/dimension checks fix one overwrite.
  - [x] Explicit input lengths, checked growing output, recoverable cleanup
        and sole standard compression APIs have [host regressions](jpeg-io-validation.md).
  - [ ] Complete deferred retail 1.32c JPEG and screenshot acceptance.
- [ ] [#44 — validate MD3/MD4 layouts before allocation or swapping](https://github.com/jm2/Quake-III-Arena/issues/44)
      — **high**, malformed model memory corruption/hangs.
  - [x] MD3 step [#77](https://github.com/jm2/Quake-III-Arena/pull/77) and
        MD4 step [#79](https://github.com/jm2/Quake-III-Arena/pull/79) merged
        after their current-head CI/review gates. Host fixtures cover full
        layouts, native conversion, cleanup, tag/frame bounds and MD4 skinning.
  - [x] Complete MD3 layout validation before copying/swapping and staged
        LOD registration have [host regressions](md3-layout-validation.md).
  - [x] Complete MD4 layout/weight/index validation before allocation and native
        conversion has [host and build evidence](md4-layout-validation.md).
  - [ ] Complete deferred commercial 1.32c model/mod and PPC live acceptance.
- [ ] [#45 — validate BSP lumps and cross-references transactionally](https://github.com/jm2/Quake-III-Arena/issues/45)
      — **high**, malformed map corruption, graph hangs, and partial state.
  - [x] Shared collision/renderer header and full lump-layout preflight precedes
        checksum, allocation or world reset; [header evidence](bsp-header-validation.md)
        covers exact inputs and collision ownership/state.
  - [x] Shared collision/material references validate before checksum or reset;
        [reference evidence](bsp-reference-validation.md) covers exact FS mutations.
  - [x] BSP lightmap upload reads complete RGB records and retains the legacy
        single-source duplicate texture; [lightmap evidence](bsp-lightmap-validation.md).
  - [x] Finite geometry inputs, patch controls and native surface capacities
        have [actual collision/renderer evidence](bsp-geometry-validation.md).
  - [x] Bounded entity parsing, checked light-grid calculations and boundary
        sampling have [actual renderer evidence](bsp-entity-grid-validation.md).
  - [x] Acyclic native tree/forest topology and linear parent initialization
        have [actual loader evidence](bsp-tree-validation.md).
  - [x] Submodel loads fit remaining native renderer model slots, with
        [real allocator evidence](bsp-model-capacity-validation.md).
  - [ ] Complete all payload cross-reference, graph and geometry validation
        before publishing a map; retain deferred retail/PPC map acceptance.
  - [x] Lighting bytes use defined overbright arithmetic across all signed cvar
        values, preserving valid legacy RGB and geometry alpha; see
        [lighting evidence](bsp-lighting-arithmetic-validation.md).
  - [x] Collision leaf/brush box queries traverse validated decision parents
        with constant C stack space; see [box-query evidence](bsp-box-query-validation.md).
  - [x] Projected-mark queries use validated renderer decision parents with
        native filtering/order and constant stack; see [mark evidence](bsp-mark-query-validation.md).
  - [x] Swept collision traversal uses bounded pending segments, preserving
        stock clipping/pruning and cleaning OOM; see [trace evidence](bsp-trace-validation.md).
  - [x] World traversal preserves inherited culling/light masks with bounded
        frames and OOM cleanup; all 32 light bits use defined shifts; see
        [world evidence](bsp-world-validation.md).
  - [x] Collision patch refinement fits the native grid before map reset, with
        [native goldens and ownership evidence](bsp-patch-grid-validation.md).
  - [x] Missing facet borders release native winding storage; copies use the
        actual header/point size; see [ownership evidence](bsp-winding-validation.md).
  - [x] Native collision plane/facet/border budgets validate before checksum,
        reset or hunk publication; see [build evidence](bsp-patch-budget-validation.md).
  - [x] Finite collision controls cannot publish nonfinite refinement, planes
        or winding vertices; see [numeric evidence](bsp-patch-numeric-validation.md).
  - [x] Renderer curve refinement, attributes and derived bounds validate
        before publication; see [native curve evidence](bsp-curve-numeric-validation.md).
  - [x] Permanent hunk allocations expose checked native alignment/debug costs
        and reject before bank changes; see [allocator evidence](hunk-allocation-validation.md).
  - [x] Aggregate collision allocations fit actual remaining hunk capacity,
        including derived indexes/visibility and real patch geometry; see
        [memory evidence](bsp-memory-budget-validation.md).
  - [x] Native planar face distances reject nonfinite derived results before
        world/shader/model changes; see [face evidence](bsp-face-numeric-validation.md).
  - [x] Native patch LOD propagation retains stock depth-first results with
        constant stack and no traversal allocation; see [LOD evidence](bsp-lod-validation.md).
- [ ] [#46 — enforce shader/skin/font limits and ownership](https://github.com/jm2/Quake-III-Arena/issues/46)
      — **high**, fixed-array writes and unsafe serialized resources.
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
- [ ] [#47 — validate AAS lumps and graph indexes before enabling bots](https://github.com/jm2/Quake-III-Arena/issues/47)
      — **high**, server OOB access/infinite traversal.
  - [x] Local mover model boundary/look-up fixes cross-build.
  - [x] AAS v4/v5 header/lump preflight precedes world reset; exact reads,
        seeks and allocation failures close once and clear partial logical
        owners; see [AAS layout evidence](aas-layout-validation.md).
  - [x] Bbox coordinates use float endian conversion; independent native/swapped
        models retain fractions, signed zero, finite extremes and existing
        uint16 travel-time conversion; see [endian evidence](aas-endian-validation.md).
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
  - [ ] Validate the entire AAS file and graph before setting `loaded`.
- [ ] [#48 — bound bot preprocessor, token, and path operations](https://github.com/jm2/Quake-III-Arena/issues/48)
      — **high**, parser fixed-buffer corruption/invalid cleanup.
  - [x] Several primitive, diagnostic, and preprocessor bounds fixes are
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
  - [ ] Finish expression bounds, remaining character/source allocation
        consumers, and aggregate parser work/recursion limits.

## P1 — high-impact security, runtime, and release blockers

- [ ] [#22 — packaging selects unverified assets and unpinned downloads](https://github.com/jm2/Quake-III-Arena/issues/22)
      — **high**, release input/supply-chain integrity.
  - [x] Local scripts copy sibling paks and require missionpack data when
        Team Arena is selected.
  - [ ] Require an explicit legal asset root, identities/digests, pinned
        downloads, and offline tests.
- [ ] [#23 — packaging can report success without a complete Classic app](https://github.com/jm2/Quake-III-Arena/issues/23)
      — **high**, false/recoverably unusable releases.
  - [x] Local scripts require PEF conversion, Rez output, resources, and an
        HFS-capable image tool.
  - [x] Local PowerShell path removes stale outputs, checks exit codes, and
        requires fresh nonempty image/MacBinary outputs.
  - [ ] Mount and validate PEF, `cfrg`, `BNDL`, icons, resource fork,
        type/creator, and Finder flags for every package path.
- [ ] [#1 — incomplete Retro68/OpenGL readiness checks](https://github.com/jm2/Quake-III-Arena/issues/1)
      — **high**, build blocker/false toolchain readiness.
  - [x] Bash/PowerShell repair and complete-prerequisite checks drafted.
  - [ ] Test compiler-only, raw-SDK-only, missing-SDK, and Windows setup paths.
- [ ] [#2 — disabled Team Arena consumed stale binaries/assets](https://github.com/jm2/Quake-III-Arena/issues/2)
      — **high**, release artifact provenance.
  - [x] Local conversion, validation, and packaging follow the configured
        target; Bash OFF/ON alternation passed.
  - [ ] Run equivalent PowerShell stale-artifact tests.
- [ ] [#8 — demo fallback produced an unusable full-game package](https://github.com/jm2/Quake-III-Arena/issues/8)
      — **high**, guaranteed startup failure.
  - [x] Local scripts fail clearly without retail `baseq3/pak0.pk3`.
- [ ] [#50 — Windows packaging used a nonexistent Rez include path](https://github.com/jm2/Quake-III-Arena/issues/50)
      — **high**, Windows resource/package blocker.
  - [x] Local code probes prepared/source layouts and requires `Types.r` plus
        `CodeFragments.r`.
  - [ ] Run native Windows Rez and full packaging.
- [ ] [#15 — renderer initialization leaks partial AGL/DrawSprocket state](https://github.com/jm2/Quake-III-Arena/issues/15)
      — **high**, display/context corruption on retry/failure.
- [ ] [#16 — gamma snapshot allocation failure makes restore unsafe](https://github.com/jm2/Quake-III-Arena/issues/16)
      — **high**, invalid restore/desktop damage.
- [ ] [#17 — InputSprocket trusts counts and element ordering](https://github.com/jm2/Quake-III-Arena/issues/17)
      — **high**, input OOB/latched state.
  - [x] Local count clamp and missing-axis guard cross-build.
  - [ ] Map by kind/label and make initialization/focus cleanup transactional.
- [ ] [#20 — Open Transport ignores critical failures and network cvars](https://github.com/jm2/Quake-III-Arena/issues/20)
      — **high**, invalid endpoints or main-loop blocking.
- [ ] [#19 — Classic event pumping/application handlers are incomplete](https://github.com/jm2/Quake-III-Arena/issues/19)
      — **high**, starvation and broken activation/quit behavior.
  - [x] Local code no longer consumes an undefined `EventRecord`.
- [ ] [#5 — Classic Mac sound remains opt-in and unvalidated](https://github.com/jm2/Quake-III-Arena/issues/5)
      — **high**, major runtime subsystem incomplete.
  - [x] Local init return, command lifecycle, DMA position, and callback UPP
        candidates cross-build.
  - [ ] Validate playback, restart, suspend, disconnect, and shutdown on target.
- [ ] [#4 — `Sys_ListFiles` hardcoded filenames and hid mods/content](https://github.com/jm2/Quake-III-Arena/issues/4)
      — **high**, core filesystem/mod functionality.
  - [x] Local Catalog Manager enumeration handles files, directories, suffixes,
        and recursive filters.
  - [ ] Validate HFS aliases, recursion, and mod discovery on target.
- [ ] [#11 — Team Arena parser cannot represent retail menu syntax](https://github.com/jm2/Quake-III-Arena/issues/11)
      — **high**, product UI blocker.
  - [x] Local numeric/signed token classification and exact reads cross-build.
  - [ ] Restore retail punctuation, preprocessing, include, define, and source
        location behavior; parse/traverse the full menu corpus.
- [ ] [#12 — Team Arena skips model and bot discovery](https://github.com/jm2/Quake-III-Arena/issues/12)
      — **high**, player setup/add-bot blocker.
- [ ] [#13 — static modules override QVM-only mods](https://github.com/jm2/Quake-III-Arena/issues/13)
      — **high**, mod compatibility/trust-boundary error.
- [ ] [#14 — intro/idlogo cinematics are unconditionally bypassed](https://github.com/jm2/Quake-III-Arena/issues/14)
      — **high runtime**, decoder failure hidden by content-name filtering.
- [ ] [#3 — anisotropic extension dereferences an unregistered cvar](https://github.com/jm2/Quake-III-Arena/issues/3)
      — **high target crash**.
  - [x] Local registration fix cross-builds.
  - [ ] Exercise extension-present and extension-absent target paths.
- [ ] [#24 — Classic startup command-line assembly can overflow](https://github.com/jm2/Quake-III-Arena/issues/24)
      — **high**, stack corruption.
  - [x] Local bounded construction exits before overflow.
  - [ ] Add exact-fit and overlong argument tests.
- [ ] [#18 — Classic event queue overflow leaks payloads/latches input](https://github.com/jm2/Quake-III-Arena/issues/18)
      — **high correctness**, ownership and key-release loss.
  - [x] Local queue frees the evicted payload and preserves the newest event.
  - [ ] Add pointer-ownership and key-transition stress tests.
- [ ] [#49 — Team Arena UI allocation/reload failures are unsafe](https://github.com/jm2/Quake-III-Arena/issues/49)
      — **moderate-high**, deterministic OOM/cvar crashes.
  - [x] Local pool/type/item/string checks, reload-name initialization, and
        bounded model cvar copies cross-build.
  - [ ] Audit every direct allocator consumer and inject OOM at each site.
- [ ] [#38 — connectionless rate limiting is bypassable and unfair](https://github.com/jm2/Quake-III-Arena/issues/38)
      — **medium security**, reflection/DoS and RCON starvation.
- [ ] [#39 — QVMs can modify protected cvars and engine commands](https://github.com/jm2/Quake-III-Arena/issues/39)
      — **medium security**, module privilege-boundary failure.
- [ ] [#40 — server-controlled `clientNum` reaches native indexes](https://github.com/jm2/Quake-III-Arena/issues/40)
      — **medium security**, native client OOB.
  - [x] Local parser rejects values outside `[0, MAX_CLIENTS)`.
  - [ ] Add malformed-gamestate tests and audit other native module indexes.

## P2 — medium correctness, target validation, and build quality

- [ ] [#6 — renderer forced `r_fullscreen 0`](https://github.com/jm2/Quake-III-Arena/issues/6)
      — **medium**.
  - [x] Local override removed.
  - [ ] Test windowed/fullscreen persistence and cleanup.
- [ ] [#7 — 16-bit fallback still requested 24-bit color](https://github.com/jm2/Quake-III-Arena/issues/7)
      — **medium**.
  - [x] Local 16-bit path requests 5/5/5.
  - [ ] Fault-test pixel-format fallback.
- [ ] [#9 — Finder creator code differed from BNDL signature](https://github.com/jm2/Quake-III-Arena/issues/9)
      — **medium release metadata**.
  - [x] Local resources/package mappings use `IDQ3`.
  - [ ] Inspect a mounted HFS artifact in Finder.
- [ ] [#10 — startup logging always opens RetroConsole](https://github.com/jm2/Quake-III-Arena/issues/10)
      — **medium presentation/fullscreen policy**.
- [ ] [#21 — dedicated networking busy-spins without console input](https://github.com/jm2/Quake-III-Arena/issues/21)
      — **medium dedicated-server functionality**.
- [ ] [#25 — fatal engine errors exit with success status](https://github.com/jm2/Quake-III-Arena/issues/25)
      — **medium automation/release correctness**.
  - [x] Local fatal path shuts down and exits 1.
  - [ ] Verify normal/fatal status and flushing under emulator automation.
- [ ] [#26 — recording with an open console freezes client time](https://github.com/jm2/Quake-III-Arena/issues/26)
      — **medium gameplay/network correctness**.
  - [x] Local `msec = 0` condition removed.
  - [ ] Record/open-console runtime regression.
- [ ] [#27 — build target selection is cache-dependent](https://github.com/jm2/Quake-III-Arena/issues/27)
      — **medium build reproducibility**.
  - [x] Local Bash/PowerShell flags always pass the selected CMake value.
  - [x] Bash base/Team Arena alternation passed.
  - [ ] Exercise both modes and packaging on Windows.
- [ ] [#28 — Release forces `-O0 -g` and disables GL fast paths](https://github.com/jm2/Quake-III-Arena/issues/28)
      — **medium performance/playability**.
- [ ] [#33 — synchronous DNS freezes the client for ten seconds](https://github.com/jm2/Quake-III-Arena/issues/33)
      — **medium responsiveness**.
- [ ] [#51 — setup requires unrelated tools for cached inputs](https://github.com/jm2/Quake-III-Arena/issues/51)
      — **medium offline/setup portability**.
  - [x] Local setup removes unused `hmount` and checks `wget` only on download.
  - [ ] Add controlled-PATH cached/partial/missing input tests.
- [ ] [#34 — AGL commands re-register on renderer initialization](https://github.com/jm2/Quake-III-Arena/issues/34)
      — **medium-low lifecycle correctness**.
  - [x] Local registration guard is set.
  - [ ] Loop `vid_restart` and renderer retry paths.

## P3 — low-risk metadata, presentation, and latent features

- [ ] [#30 — stereo begins two eyes but renders one centered frame](https://github.com/jm2/Quake-III-Arena/issues/30)
      — **low/latent** while Mac stereo is not requested.
- [ ] [#31 — generated color icons lack a matching CLUT](https://github.com/jm2/Quake-III-Arena/issues/31)
      — **low visual metadata**.
- [ ] [#32 — MacBinary output writes invalid zero dates](https://github.com/jm2/Quake-III-Arena/issues/32)
      — **low metadata**.
  - [x] Local encoder writes `SOURCE_DATE_EPOCH` or input mtime in Mac epoch.
  - [x] Host fixture checks dates, fork layout, and CRC against Python's
        independent `binascii.crc_hqx` implementation.
  - [ ] Validate with a complete independent decoder and on target.
- [ ] [#52 — MacBinary filename length counted characters, not bytes](https://github.com/jm2/Quake-III-Arena/issues/52)
      — **low metadata**.
  - [x] Local encoder strictly encodes MacRoman, then truncates/counts bytes and
        validates fork widths.
  - [x] Host tests cover a 64-byte MacRoman name truncated to 63 bytes and
        rejection of an unrepresentable name.
  - [ ] Add exact 63-byte and short representable non-ASCII fixtures; validate
        the complete result with an independent MacBinary II reader.

## Completed review work not tied to one open issue

- [x] Inventory build entry points, Classic Mac sources, historical plans, and
      stabilization/security history.
- [x] Complete independent platform, engine/protocol, asset/parser, and
      release-tool source passes.
- [x] Cross-build base and Team Arena with the local review patch set.
- [x] Validate PEF architecture/header and classify compiler output.
- [x] Add first-pass ioquake3 provenance and accepted/missing family mapping.
- [x] Preserve pre-existing user logs during the July review; its candidate
      changes were subsequently committed in `204fe36`. New work follows the
      PR/CI/bot-review merge process above.
- [x] Add portable GitHub Actions starter CI, packaging/ledger unit tests,
      isolated ASan/UBSan C regressions, CI documentation, and pinned-action
      Dependabot updates.

## Required validation matrix

- [x] Bash scripts parse.
- [x] PowerShell scripts parse and build help runs.
- [x] Python utilities compile.
- [x] Base and Team Arena PPC cross-build and strong PEF checks pass.
- [x] Deterministic MacBinary fixture is recognized with valid dates, CRC,
      type, creator, name, and fork length.
- [ ] Host ASan/UBSan malformed-input corpora for messages/Huffman, downloads,
      ZIP, QVM, RoQ, images/JPEG, models, BSP, shader/skin/font, bot/AAS, UI
      allocation, and format strings.
- [ ] PowerShell setup/build/package on native Windows.
- [ ] Offline package from an explicit legal asset root.
- [ ] Mounted package resource/Finder validation.
- [ ] Base and Team Arena end-to-end Mac OS 9 smoke tests.
- [ ] Sound, InputSprocket, Open Transport, fullscreen/gamma, suspend/resume,
      fatal exit, and normal quit target tests.

## Exact continuation point

- [x] Reconcile all 52 open issues against `204fe36`, existing test coverage,
      and July evidence; retain their current priorities and closure gates.
- [x] Re-run the eight Python tests, q_shared ASan/UBSan harness, Bash syntax
      and help, and PowerShell syntax and help on 2026-09-17.
- [x] Confirm Codex and CodeRabbit are enabled and review PRs automatically.
- [x] User confirmed the merge gate: clean current-head Codex review plus
      resolved CodeRabbit findings; no additional merge approval is needed.
- [x] User requires commercial Quake III Arena 1.32c wire compatibility for
      #37, as supported by Quake3e/ioquake3; no mandatory incompatible fields.
- [x] User authorized host tests and cross-builds without retail assets or a
      Mac OS 9 environment; live acceptance is deferred to a follow-up session.
- [x] QVM steps #54–#67/#69 and single-run CI #68 are merged at master
      `4fd62bd`, each after successful CI, clean completed Codex review and
      resolved CodeRabbit findings.
- [x] RoQ/image/JPEG/model/BSP steps #70–#77/#79–#82 and assessment #78
      are merged at master `95a6c18`, after the same current-head CI/review gates.
- [x] BSP steps #83–#91/#93–#101 and shader capacity #102 are merged at
      master `cdc8c38` after the same current-head CI/review gates.
- [x] Skin/font/shader and ledger PRs #103–#114 are merged at master
      `fc6c10e`, each after exact-head CI, completed clean Codex review and
      resolution of every bot finding. The [September 18 snapshot](review-2026-09-18.md)
      records the earlier queue; its dated evidence remains unchanged.
- [x] Derived shader conversions/noise PR #115 merged at `aa25e88` after
      exact-head CI, clean completed Codex and resolved bot findings.
- [x] Sky subdivisions/shared cloud mesh #116 merged at `e59b66d` after
      exact-head CI, clean completed Codex and resolved bot findings.
- [x] Cloud-layer math/publication #117 merged at `c4251e7` after the
      same exact-head CI/Codex/resolved-finding gate.
- [x] Native material fallback #118 merged at `f18ca23` after exact-head
      CI, completed clean Codex and resolved review findings.
- [ ] Continue renderer budgets/transactions and AAS/preprocessor roots;
      retain deferred acceptance.
- [x] AAS layout/endian/writer PRs #119–#121 merged at `431e272` after
      exact-head CI, completed clean Codex and resolved review findings.
- [x] AAS geometry #122 merged at `b242458` and allocator/import #123
      at `ed42d87`, after exact-head CI, clean completed Codex and all
      CodeRabbit findings resolved.
- [x] Node/reachability #124–#125 merged at `86c1667` after exact-head
      CI, clean completed Codex and resolved review findings.
- [ ] Gate portal/travel-cost/workspace steps and finish #47 runtime query, memory/
      work budget and late-load transaction work.
- [ ] Finish #45 renderer aggregate capacity and full transactional
      publication; retain remaining query/candidate costs and deferred target
      acceptance. Collision aggregate and derived geometry/facet checks are
      merged; keep parent acceptance open.
- [ ] Record and execute the deferred retail/target compatibility checks when
      the user provides the assets and test environment.

- [x] Run the new portable CI suite locally and correct every failure.
- [x] Re-run both product builds after the last formatter/release-tool/CI
      edits; update the PEF sizes/hashes above and leave CMake base-only.
- [x] Remove only the review-generated `__pycache__/`; preserve `q3-logs/`.
- [ ] P0 implementation order: #35, #41, #42, #43, #44, #45, #46, #47,
      #48, then the compatibility-sensitive #37.
- [ ] P0 validation order for local candidates: #36, message/Huffman exact
      bounds, downloads, and known format-string fixes.
- [ ] P1 target/runtime order: #11 after #48; #15, #16, #17, #5, #20, #19.
      Re-enable #12 after #47/#48, #13 after #35/#39, and #14 after #41.
      Validate fullscreen/gamma behavior after #15/#16.
- [ ] Add a legally provisioned Retro68 CI runner for both product builds
      before treating portable CI as release evidence.
- [ ] Do not close an issue solely because a cross-build passed.
