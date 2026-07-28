# Quake III Arena Mac OS 9 subsystem evidence appendix

Last updated: 2026-07-28

This preserves the first review session's subsystem-oriented evidence and
local-fix notes. It is not the work-order queue. Use the priority/severity
sorted [task.md](task.md) as the authoritative handoff and update both files
when evidence here materially changes.

An issue-level checkbox stays open until its fix is committed, focused checks
pass, the full Retro68 build succeeds, required target testing is complete, and
the GitHub issue is closed. Nested checkboxes may record locally drafted or
partially verified work without implying completion.

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
  - [ ] Add a separately correct demo product or document retail-only scope.
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
  - [ ] Validate dates and CRC with an independent decoder and on target.
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
  - [ ] Add 63/64-byte and representable/unrepresentable filename fixtures.

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
  - [ ] Port the complete header, bytecode, branch, stack, and VM data-image
        validation family before accepting untrusted QVMs.
- [ ] [#36 — oversized/truncated PK3 entries](https://github.com/jm2/Quake-III-Arena/issues/36)
  - [x] Local code rejects unrepresentable sizes before casts, checks unzip
        opens/reads, rejects short reads, and fixes short-name suffix checks.
  - [x] Base and Team Arena PPC cross-builds pass.
  - [ ] Add malicious ZIP fixtures and prove every handle/buffer cleanup path.
- [ ] [#37 — connection/netchan lacks challenge binding](https://github.com/jm2/Quake-III-Arena/issues/37)
  - [ ] Decide secure/legacy wire compatibility, then port and test the complete
        challenge/checksum protocol family.
- [ ] [#38 — connectionless rate limiting is bypassable/unfair](https://github.com/jm2/Quake-III-Arena/issues/38)
- [ ] [#39 — QVMs can modify protected cvars/commands](https://github.com/jm2/Quake-III-Arena/issues/39)
- [ ] [#40 — server-controlled `clientNum` reached native indexes](https://github.com/jm2/Quake-III-Arena/issues/40)
  - [x] Local gamestate parser rejects values outside `[0, MAX_CLIENTS)`.
  - [x] Base and Team Arena PPC cross-builds pass.
  - [ ] Add malformed-gamestate tests and audit other native module indexes.

### Malformed assets, bot data, and UI allocation

- [ ] [#41 — RoQ chunk/dimension/audio/cursor bounds](https://github.com/jm2/Quake-III-Arena/issues/41)
- [ ] [#42 — BMP/PCX/TGA loaders need bounded cursors](https://github.com/jm2/Quake-III-Arena/issues/42)
- [ ] [#43 — JPEG I/O is not length-aware and APIs are duplicated](https://github.com/jm2/Quake-III-Arena/issues/43)
  - [x] Local RGBA output allocation/dimension validation fixes one deterministic
        overwrite.
  - [ ] Replace the source/destination managers and duplicate APIs.
- [ ] [#44 — MD3/MD4 layouts are not validated](https://github.com/jm2/Quake-III-Arena/issues/44)
- [ ] [#45 — BSP lumps/cross-references are not validated transactionally](https://github.com/jm2/Quake-III-Arena/issues/45)
- [ ] [#46 — shader/skin/font fixed limits and ownership](https://github.com/jm2/Quake-III-Arena/issues/46)
- [ ] [#47 — AAS lumps and graph indexes are trusted](https://github.com/jm2/Quake-III-Arena/issues/47)
  - [x] Local mover model boundary/look-up fixes cross-build.
  - [ ] Validate the complete file and graph before setting `loaded`.
- [ ] [#48 — bot preprocessor/token/path bounds](https://github.com/jm2/Quake-III-Arena/issues/48)
  - [x] Several upstream primitive/preprocessor/diagnostic bounds fixes are
        locally ported.
  - [ ] Finish token merging, include paths, time macros, character paths, and
        allocation cleanup.
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

- [x] Cross-build the full local security/UI/filesystem patch set as base and
      Team Arena, validate both PEFs, then return configuration to base-only.
- [x] Run a clean rebuild with captured output and classify warnings.
- [ ] P0 security: fix #35, #41, #42, #43, #44, #45, and #47 before treating
      downloaded/installed mod content as untrusted.
- [ ] P1 security: decide the compatibility policy for #37, then fix #38, #39,
      #46, and #48.
- [ ] Add focused host tests for local candidates #18, #24, #25, #36, #40,
      #49, message/Huffman exact bounds, download pairs, and format strings.
- [ ] Prioritize target runtime blockers: #11, #15, #16, #17, #5, then #20.
- [ ] Obtain legal retail assets and a Mac OS 9 emulator/hardware run for the
      first end-to-end smoke test; preserve resulting logs outside generated
      package staging.
- [ ] Do not close any issue solely because a cross-build passed.
