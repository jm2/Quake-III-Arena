# Quake III Arena Mac OS 9 prioritized review queue

Last updated: 2026-07-28

This is the authoritative continuation ledger. Work is sorted first by
priority (`P0` through `P3`), then by severity and exploit/runtime impact
within each priority. Every confirmed GitHub issue from #1 through #52 appears
exactly once below. The subsystem evidence appendix is
[review-ledger-by-subsystem.md](review-ledger-by-subsystem.md).

An issue-level checkbox stays open until its fix is committed, focused checks
pass, the full Retro68 build succeeds, required Mac OS 9 testing is complete,
and the linked GitHub issue is closed. A checked nested item means only that
the stated local candidate or check is complete.

## Baseline and current evidence

- [x] Repository: `jm2/Quake-III-Arena`, branch `master`.
- [x] Review baseline:
      `abe5028afda280240d48d012a62c09e713689fd2`.
- [x] Preserve the pre-existing untracked `q3-logs/`; its latest captured
      startup ends at `Couldn't load default.cfg`.
- [x] First-pass security comparison anchored to ioquake3
      `588393618dbc82e7207c21c6ddecca229944a03a` (2026-07-16).
- [x] Security mapping recorded in
      [security-provenance.md](security-provenance.md).
- [x] 52 confirmed root causes opened in the
      [GitHub issue tracker](https://github.com/jm2/Quake-III-Arena/issues).
- [x] Latest locally tested base PEF: `Joy!peff` / `pwpc`, 3,669,561 bytes,
      SHA-256
      `8a23e2225ce5e21f253f7f155a0d601ae5d659685e558150061eba677305115c`.
- [x] Latest locally tested Team Arena PEF: `Joy!peff` / `pwpc`,
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

- [ ] [#29 — modern CVE coverage lacks auditable provenance and regressions](https://github.com/jm2/Quake-III-Arena/issues/29)
      — **assurance gate**.
  - [x] First upstream/local status matrix and GitHub provenance comment added.
  - [ ] Complete the advisory inventory and add a malformed-input regression
        for every accepted security family.
- [ ] [#35 — harden interpreted QVM validation and sandbox bounds](https://github.com/jm2/Quake-III-Arena/issues/35)
      — **high**, native memory corruption from malformed QVMs.
  - [ ] Port complete header/range, bytecode, branch, stack, syscall, and
        data-image checks; retain valid PPC QVM compatibility.
- [ ] [#37 — bind connection and netchan packets to negotiated challenges](https://github.com/jm2/Quake-III-Arena/issues/37)
      — **high**, connection redirection/injection/hijack.
  - [ ] Decide and document secure-versus-legacy wire compatibility before
        implementation.
- [ ] [#36 — reject oversized and truncated PK3 entries](https://github.com/jm2/Quake-III-Arena/issues/36)
      — **high**, ZIP-controlled allocation/decompression corruption.
  - [x] Local size/cast, exact-read, open-result, cleanup, and short-suffix
        checks cross-build.
  - [ ] Add malicious ZIP fixtures around caps, `INT_MAX`, `UINT32_MAX`, and
        truncated streams.
- [ ] [#41 — bound RoQ chunks, dimensions, audio output, and cursors](https://github.com/jm2/Quake-III-Arena/issues/41)
      — **high**, deterministic cinematic buffer corruption.
- [ ] [#42 — replace BMP/PCX/TGA loaders with bounded cursors](https://github.com/jm2/Quake-III-Arena/issues/42)
      — **high**, deterministic image heap/OOB corruption.
- [ ] [#43 — make JPEG I/O length-aware and remove duplicate APIs](https://github.com/jm2/Quake-III-Arena/issues/43)
      — **high**, OOB decode and link-order ambiguity.
  - [x] Local RGBA output sizing/dimension checks fix one overwrite.
  - [ ] Replace source/destination managers, fatal error flow, and duplicate
        libjpeg entry points.
- [ ] [#44 — validate MD3/MD4 layouts before allocation or swapping](https://github.com/jm2/Quake-III-Arena/issues/44)
      — **high**, malformed model memory corruption/hangs.
- [ ] [#45 — validate BSP lumps and cross-references transactionally](https://github.com/jm2/Quake-III-Arena/issues/45)
      — **high**, malformed map corruption, graph hangs, and partial state.
- [ ] [#46 — enforce shader/skin/font limits and ownership](https://github.com/jm2/Quake-III-Arena/issues/46)
      — **high**, fixed-array writes and unsafe serialized resources.
- [ ] [#47 — validate AAS lumps and graph indexes before enabling bots](https://github.com/jm2/Quake-III-Arena/issues/47)
      — **high**, server OOB access/infinite traversal.
  - [x] Local mover model boundary/look-up fixes cross-build.
  - [ ] Validate the entire AAS file and graph before setting `loaded`.
- [ ] [#48 — bound bot preprocessor, token, and path operations](https://github.com/jm2/Quake-III-Arena/issues/48)
      — **high**, parser fixed-buffer corruption/invalid cleanup.
  - [x] Several primitive, diagnostic, and preprocessor bounds fixes are
        locally ported.
  - [ ] Finish token merge/stringize, include paths, time macros, character
        paths/indexes, and allocation cleanup.

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
  - [ ] Validate CRC/dates with an independent decoder and on target.
- [ ] [#52 — MacBinary filename length counted characters, not bytes](https://github.com/jm2/Quake-III-Arena/issues/52)
      — **low metadata**.
  - [x] Local encoder strictly encodes MacRoman, then truncates/counts bytes and
        validates fork widths.
  - [ ] Test 63/64-byte and representable/unrepresentable names.

## Completed review work not tied to one open issue

- [x] Inventory build entry points, Classic Mac sources, historical plans, and
      stabilization/security history.
- [x] Complete independent platform, engine/protocol, asset/parser, and
      release-tool source passes.
- [x] Cross-build base and Team Arena with the local review patch set.
- [x] Validate PEF architecture/header and classify compiler output.
- [x] Add first-pass ioquake3 provenance and accepted/missing family mapping.
- [x] Preserve pre-existing user logs and avoid committing/pushing changes.
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

- [x] Run the new portable CI suite locally and correct every failure.
- [x] Re-run both product builds after the last formatter/release-tool/CI
      edits; update the PEF sizes/hashes above and leave CMake base-only.
- [x] Remove only the review-generated `__pycache__/`; preserve `q3-logs/`.
- [ ] P0 implementation order: #35, #41, #42, #43, #44, #45, #46, #47,
      #48, then the compatibility-sensitive #37.
- [ ] P0 validation order for local candidates: #36, message/Huffman exact
      bounds, downloads, and known format-string fixes.
- [ ] P1 target/runtime order: #11, #15, #16, #17, #5, #20, #19.
- [ ] Add a legally provisioned Retro68 CI runner for both product builds
      before treating portable CI as release evidence.
- [ ] Do not close an issue solely because a cross-build passed.
