# Quake III Arena Mac OS 9 burndown ledger

Last updated: 2026-09-23

This is the authoritative continuation ledger. It lists every tracked GitHub
issue exactly once as a checkbox entry, grouped into burndown sections that
are worked roughly in order: what blocks a clean, bootable base game comes
first, then remote security, then target bring-up and the remaining hardening.
The [2026-09-23 review](review-2026-09-23.md) explains why the order changed.
The previous security-first queue for #1–#52, with every September sub-step
and its evidence links, is archived unchanged in
[task-2026-09-19.md](task-2026-09-19.md).

## Process

- One focused PR per fix. Merge only after CI passes on the current head
  and an independent adversarial reviewer (a fresh Claude reviewer agent that
  did not write the change) has approved that exact head in a PR comment,
  with every blocking finding fixed and re-reviewed. Absent, pending or
  failed results are not approval. CodeRabbit findings are addressed when it
  reviews a PR but are not a merge blocker. (From 2026-09-23; Codex review
  credits are exhausted and CodeRabbit is throttled to one review per hour.)
- Engine changes cross-build both products (base and `BUILD_TEAM_ARENA=ON`)
  with the local Retro68 toolchain and add no compiler warnings. Host changes
  run the relevant host checks.
- Evidence (commands, results, PEF size) goes in the PR description. Do not
  add per-PR `docs/*-validation.md` files; update an existing one only when it
  would otherwise state something false.
- Use `Closes #N` only when the issue's acceptance criteria can be met without
  Mac OS 9. Otherwise reference the issue and add the `needs target test`
  label after merge; the entry below stays unchecked until the target run.
- Behavior changes to retail-content handling must be checked against real
  data where it is available locally (never committed), or against the
  original `dbe4ddb` code in a differential host test.
- The paused hardening PRs (#171, #184–#196, #211–#219) are not merged until
  they are re-scoped against this order. Their issues stay in B6 and B4.
- A checkbox is ticked when its issue closes.

## Current evidence

- Last Mac OS 9 run: 2026-04-27, which stopped at `Couldn't load default.cfg`
  after demo data was packaged as `baseq3` (#8). No target run since.
- `master` cross-builds with Retro68 (GCC 12.2.0, Retro68 `83b9c8d2c5`):
  base `Quake3.pef` 3,796,539 bytes, `Joy!peff`/`pwpc`, 0 compiler warnings
  (2026-09-23). The maintainer's toolchain needs a host-library shim or a
  rebuild after the Fedora 44 upgrade (#269).
- All 122 host regression runners pass with GCC 16.2.1 and Clang 22.1.8
  (2026-09-23).
- Emulators cannot pass the accelerated-renderer check (#268); rendering,
  gamma, fullscreen and performance acceptance need real hardware.

## B0 — Process and CI unblock

Land these first: every later PR depends on a CI run that finishes and a ledger that can take new issues.

- [ ] [#220 — setup_retro68.sh syntax error aborts every fresh setup after the toolchain build](https://github.com/jm2/Quake-III-Arena/issues/220) — **high**; fresh setup exits 2 after the toolchain build; PR #278.
- [ ] [#221 — CI Bash syntax check parses only the first script on each line](https://github.com/jm2/Quake-III-Arena/issues/221) — **medium**; CI syntax step only parsed the first script; PR #278.
- [ ] [#222 — Host C regression job serializes ~190 runners and now exceeds its 30-minute timeout](https://github.com/jm2/Quake-III-Arena/issues/222) — **medium**; host job serial and at its 30-minute limit; PR #279.
- [ ] [#228 — CI cancels in-progress master runs, leaving most merged commits untested](https://github.com/jm2/Quake-III-Arena/issues/228) — **low**; master runs cancelled by later merges; PR #279.
- [ ] [#229 — Review ledger test freezes the issue set at #1-#52 and blocks triage of new issues](https://github.com/jm2/Quake-III-Arena/issues/229) — **low**; ledger test frozen at #1–#52; this ledger rewrite.

## B1 — Regressions and runtime bugs to clear before the first target run

Each blocks or corrupts a normal base-game session. All are host-verifiable except #256.

- [ ] [#270 — SV_GentityNum bound faults the server on botlib passent -1 traces during map load](https://github.com/jm2/Quake-III-Arena/issues/270) — **critical**; server faults on botlib passent -1 traces while loading maps with suspended items.
- [ ] [#243 — BSP reference preflight rejects retail q3dm17 (flare surfaces with fogNum 0 and no fogs)](https://github.com/jm2/Quake-III-Arena/issues/243) — **critical**; retail q3dm17 rejected by both BSP loaders.
- [ ] [#233 — Monolithic link aliases botlib g_gametype/bot_developer onto the game's vmCvar_t globals](https://github.com/jm2/Quake-III-Arena/issues/233) — **high**; static link aliases botlib ints onto game vmCvar_t globals; gametype rules corrupt.
- [ ] [#246 — Unsigned char match-variable offsets make SV_GameBotMatch drop the server on PPC](https://github.com/jm2/Quake-III-Arena/issues/246) — **high**; unsigned char match offsets drop the server on PPC.
- [ ] [#223 — Retro68 target compiles with unsigned char, diverging from retail 1.32c and all host tests](https://github.com/jm2/Quake-III-Arena/issues/223) — **medium**; target char signedness differs from retail and every host test.
- [ ] [#244 — MD3 frame-bounds check rejects all stock tag-only weapon hand models](https://github.com/jm2/Quake-III-Arena/issues/244) — **high**; stock weapon hand models rejected.
- [ ] [#245 — Bot synonym replacement can no longer lengthen text, and chat word matching changed](https://github.com/jm2/Quake-III-Arena/issues/245) — **high**; bots stop understanding lengthening synonyms and some chat.
- [ ] [#237 — QVM libc shim bg_lib.c replaces libc rand/atof/memmove/qsort in the native executable](https://github.com/jm2/Quake-III-Arena/issues/237) — **medium**; bg_lib.c replaces libc rand/atof/memmove/qsort; PR #280.
- [ ] [#256 — Modifier keys only register when another OS event arrives (Ctrl-fire/Shift-run latch)](https://github.com/jm2/Quake-III-Arena/issues/256) — **high**; Ctrl/Shift/Alt only noticed on unrelated events; PR #281; needs target test.
- [ ] [#240 — Info_SetValueForKey_Big rejects values of 1024+ chars, dropping pure pak lists from systeminfo](https://github.com/jm2/Quake-III-Arena/issues/240) — **medium**; pure pak lists of 1024+ chars dropped from systeminfo.
- [ ] [#236 — glconfig_t layout change breaks retail 1.32c cgame/UI QVMs](https://github.com/jm2/Quake-III-Arena/issues/236) — **high**; glconfig_t layout break for retail cgame/UI QVMs.
- [ ] [#248 — Interpreter traps on shifts >= 32 break retail cgame QVM scoreboards with clients >= 32](https://github.com/jm2/Quake-III-Arena/issues/248) — **medium**; interpreter traps on shifts that retail QVMs rely on.
- [ ] [#247 — RoQ codebook rule truncates retail idlogo.RoQ, and VQ decode is ~5x slower](https://github.com/jm2/Quake-III-Arena/issues/247) — **medium**; retail idlogo.RoQ truncated (latent behind #14); decode ~5x slower.
- [ ] [#252 — Botlib disables bots on level-item pool exhaustion and drops zero-cost goal routes](https://github.com/jm2/Quake-III-Arena/issues/252) — **low**; botlib fails closed on pool exhaustion and zero-cost routes.
- [ ] [#242 — trap_BotMutateGoalFuzzyLogic passes a float without PASSFLOAT](https://github.com/jm2/Quake-III-Arena/issues/242) — **low**; missing PASSFLOAT on one botlib trap.
- [ ] [#241 — Monolithic cgame compiles the non-retail cg_particles.c instead of the 1.32 particle code](https://github.com/jm2/Quake-III-Arena/issues/241) — **low**; non-retail particle code compiled into cgame.

## B2 — Build, toolchain and packaging

A launchable application must come out of every build, from a pinned toolchain, before release packaging work.

- [ ] [#226 — Default build produces a non-launchable PEF; resources are only compiled in package mode](https://github.com/jm2/Quake-III-Arena/issues/226) — **medium**; default build output is a bare PEF; resources only in package mode.
- [ ] [#225 — Packaging on a macOS host fails because Rez output lives in the resource fork](https://github.com/jm2/Quake-III-Arena/issues/225) — **medium**; macOS-host packaging reads the empty data fork.
- [ ] [#269 — Toolchain readiness accepts binaries that cannot run, and setup cannot repair them](https://github.com/jm2/Quake-III-Arena/issues/269) — **medium**; readiness accepts a toolchain that cannot run.
- [ ] [#227 — Retro68 toolchain and Apple SDK inputs are unpinned and unverified](https://github.com/jm2/Quake-III-Arena/issues/227) — **medium**; Retro68 and SDK inputs unpinned.
- [ ] [#1 — Build scripts accept incomplete Retro68 toolchain and fail on missing prepared OpenGL headers](https://github.com/jm2/Quake-III-Arena/issues/1) — **high**; readiness does not check prepared OpenGL SDK files.
- [ ] [#230 — Minimum SIZE partition leaves ~5 MB headroom; enabling sound can corrupt memory](https://github.com/jm2/Quake-III-Arena/issues/230) — **medium**; SIZE partition headroom about 5 MB; sound pool unchecked.
- [ ] [#8 — Demo packaging fallback produces an unusable directory/layout](https://github.com/jm2/Quake-III-Arena/issues/8) — **high**; retail data required; demo fallback removed.
- [ ] [#22 — Packaging selects unverified assets and unpinned downloads](https://github.com/jm2/Quake-III-Arena/issues/22) — **high**; asset provenance and pinned downloads.
- [ ] [#23 — Packaging can report success without a complete Classic application](https://github.com/jm2/Quake-III-Arena/issues/23) — **high**; mounted-image resource and Finder validation.
- [ ] [#2 — Disabled Team Arena builds still validate and package stale binaries](https://github.com/jm2/Quake-III-Arena/issues/2) — **high**; stale Team Arena artifacts.
- [ ] [#27 — Build target selection is cache-dependent](https://github.com/jm2/Quake-III-Arena/issues/27) — **medium**; cache-dependent target selection.
- [ ] [#50 — Windows packaging uses a nonexistent Retro68 Rez include path](https://github.com/jm2/Quake-III-Arena/issues/50) — **high**; Windows Rez include path.
- [ ] [#51 — Retro68 setup requires unrelated tools even for cached inputs](https://github.com/jm2/Quake-III-Arena/issues/51) — **medium**; setup needs unrelated tools.
- [ ] [#9 — Finder creator code does not match the BNDL icon signature](https://github.com/jm2/Quake-III-Arena/issues/9) — **medium**; IDQ3 everywhere; needs Finder inspection of a mounted image.
- [ ] [#231 — Build/setup scripts depend on the caller's directory and lack xxd/ruby/exit-code checks](https://github.com/jm2/Quake-III-Arena/issues/231) — **low**; scripts depend on the caller directory; missing tool checks.
- [ ] [#232 — Remove stale build inputs: MacGamma.cpp, empty q3.rsrc, unused ui_obj and CMake variables](https://github.com/jm2/Quake-III-Arena/issues/232) — **low**; stale build inputs.

## B3 — Test and CI coverage

Close the gaps that let the September regressions through.

- [ ] [#253 — No real-content equivalence tests; synthetic fixtures missed stock-asset regressions](https://github.com/jm2/Quake-III-Arena/issues/253) — **high**; real-content equivalence tests against the pre-hardening baseline.
- [ ] [#224 — CI never builds the PPC product or tests any 32-bit big-endian configuration](https://github.com/jm2/Quake-III-Arena/issues/224) — **medium**; 32-bit big-endian and unsigned-char CI; PPC product build.
- [ ] [#29 — Modern CVE coverage has no auditable provenance or regression matrix](https://github.com/jm2/Quake-III-Arena/issues/29) — **assurance gate**; CVE provenance and regression matrix.
- [ ] [#276 — Code comments and plan cite CVE ids that belong to unrelated products or other bugs](https://github.com/jm2/Quake-III-Arena/issues/276) — **low**; CVE ids in comments that belong to other products.

## B4 — Remote and network security

Remote memory corruption and denial of service reachable from the network come before further local-content hardening.

- [ ] [#254 — Netchan fragment reassembly overflows the receive buffer by 4 bytes](https://github.com/jm2/Quake-III-Arena/issues/254) — **critical**; netchan reassembly overruns the receive buffer by 4 bytes.
- [ ] [#255 — Unauthenticated master-server responses overflow cls.globalServerAddresses](https://github.com/jm2/Quake-III-Arena/issues/255) — **critical**; unauthenticated master replies overflow the server list.
- [ ] [#257 — Sys_SendPacket errors on replies over 1400 bytes; one getstatus drops a Mac server](https://github.com/jm2/Quake-III-Arena/issues/257) — **high**; one oversized getstatus reply drops a Mac-hosted server.
- [ ] [#271 — Repeated donedl commands queue unbounded gamestate copies and exhaust the server zone](https://github.com/jm2/Quake-III-Arena/issues/271) — **high**; repeated donedl exhausts the server zone.
- [ ] [#239 — Native cgame trusts server tinfo client numbers and entity weapon indices](https://github.com/jm2/Quake-III-Arena/issues/239) — **high**; server-controlled tinfo/weapon indices write native cgame memory.
- [ ] [#238 — UI/cgame syscalls pass unchecked key, ping and entity indices to native arrays](https://github.com/jm2/Quake-III-Arena/issues/238) — **high**; QVM key/ping/sound indices write native arrays.
- [ ] [#37 — Bind connection setup and sequenced packets to negotiated challenges](https://github.com/jm2/Quake-III-Arena/issues/37) — **high**; challenge binding for connection setup and netchan, 1.32c compatible.
- [ ] [#36 — Reject oversized and truncated PK3 entries before allocation](https://github.com/jm2/Quake-III-Arena/issues/36) — **high**; hostile ZIP fixtures around caps and truncation.
- [ ] [#272 — One client's userinfo burst overflows every other client's reliable-command window](https://github.com/jm2/Quake-III-Arena/issues/272) — **medium**; userinfo bursts overflow other clients.
- [ ] [#273 — Clients can remove or forge the server-maintained ip userinfo key and evade IP bans](https://github.com/jm2/Quake-III-Arena/issues/273) — **medium**; client-forged ip userinfo evades bans.
- [ ] [#274 — CVE-2017-6903 only partly ported: configs load from pk3s, VM writes are not extension-restricted](https://github.com/jm2/Quake-III-Arena/issues/274) — **medium**; CVE-2017-6903 partial: configs from pk3s, VM writes.
- [ ] [#262 — HFS ':' separators bypass qpath and fs_game traversal checks](https://github.com/jm2/Quake-III-Arena/issues/262) — **medium**; HFS : separators bypass traversal checks.
- [ ] [#38 — Rate-limit all connectionless commands fairly per address and globally](https://github.com/jm2/Quake-III-Arena/issues/38) — **medium**; per-address and global connectionless rate limits.
- [ ] [#39 — Prevent QVMs from modifying protected cvars and engine commands](https://github.com/jm2/Quake-III-Arena/issues/39) — **medium**; protected cvars and engine commands.
- [ ] [#40 — Validate server-controlled clientNum before native cgame initialization](https://github.com/jm2/Quake-III-Arena/issues/40) — **medium**; clientNum check present; malformed-gamestate tests and index audit remain.
- [ ] [#275 — Client echo/print connectionless handlers accept any source address](https://github.com/jm2/Quake-III-Arena/issues/275) — **low**; echo/print accept any sender.
- [ ] [#277 — Mac Sys_StringToAdr copies unbounded hostnames into a 256-byte DNSAddress](https://github.com/jm2/Quake-III-Arena/issues/277) — **low**; unbounded hostnames into a 256-byte DNSAddress.
- [ ] [#265 — Sys_GetPacket ignores T_MORE, splitting oversize datagrams into two packets](https://github.com/jm2/Quake-III-Arena/issues/265) — **low**; T_MORE datagrams split into two packets.

## B5 — Mac OS 9 platform and target bring-up

Most items need Mac OS 9 hardware with a 3D accelerator (#268). Entries marked needs target test already have code fixes.

- [ ] [#268 — Renderer requires accelerated AGL, so Mac OS 9 emulators cannot run acceptance tests](https://github.com/jm2/Quake-III-Arena/issues/268) — **medium**; emulators fail the accelerated-renderer check; define hardware-only checks.
- [ ] [#258 — Retro68 open() requests read/write for every fopen, so locked or read-only data cannot load](https://github.com/jm2/Quake-III-Arena/issues/258) — **high**; Retro68 open() asks for write access; read-only media fail.
- [ ] [#15 — Mac renderer initialization leaks partial AGL and DrawSprocket state](https://github.com/jm2/Quake-III-Arena/issues/15) — **high**; partial AGL/DrawSprocket state on failure.
- [ ] [#16 — Gamma snapshot allocation failures make restore unsafe](https://github.com/jm2/Quake-III-Arena/issues/16) — **high**; gamma snapshot allocation failure.
- [ ] [#17 — InputSprocket initialization trusts counts and element ordering](https://github.com/jm2/Quake-III-Arena/issues/17) — **high**; InputSprocket counts and element ordering.
- [ ] [#19 — Classic Mac event pumping and application event handlers are incomplete](https://github.com/jm2/Quake-III-Arena/issues/19) — **high**; incomplete event pump and application events.
- [ ] [#20 — Open Transport initialization ignores critical errors and network cvars](https://github.com/jm2/Quake-III-Arena/issues/20) — **high**; Open Transport errors and network cvars.
- [ ] [#5 — Classic Mac sound backend remains disabled and unvalidated](https://github.com/jm2/Quake-III-Arena/issues/5) — **high**; sound backend disabled and unvalidated.
- [ ] [#259 — Retro68 rename/unlink are stubs; downloads fall back to an unchecked whole-file malloc copy](https://github.com/jm2/Quake-III-Arena/issues/259) — **medium**; rename/unlink stubs; unchecked copy fallback.
- [ ] [#260 — Unique pk3 reads buffer whole entries in the 16 MB zone and fatally fail on large RoQ/music](https://github.com/jm2/Quake-III-Arena/issues/260) — **medium**; unique pk3 entries buffered whole in the zone.
- [ ] [#261 — Classic Mac build has no startup parameters: +set, safe and fs_game are unreachable](https://github.com/jm2/Quake-III-Arena/issues/261) — **medium**; no startup parameters on Classic Mac OS.
- [ ] [#13 — Static built-in modules override QVM-only mods](https://github.com/jm2/Quake-III-Arena/issues/13) — **high**; static modules override QVM-only mods.
- [ ] [#14 — Intro and idlogo cinematics are unconditionally bypassed](https://github.com/jm2/Quake-III-Arena/issues/14) — **high**; intro/idlogo bypass.
- [ ] [#18 — Classic Mac event queue overflow leaks payloads and can latch input](https://github.com/jm2/Quake-III-Arena/issues/18) — **high**; event queue ownership; needs stress tests.
- [ ] [#24 — Classic Mac startup command-line assembly can overflow](https://github.com/jm2/Quake-III-Arena/issues/24) — **high**; bounded command line; input path is dead until #261.
- [ ] [#4 — Classic Mac Sys_ListFiles hardcodes filenames and disables mods/custom content](https://github.com/jm2/Quake-III-Arena/issues/4) — **high**; Catalog Manager enumeration; needs target test.
- [ ] [#3 — Mac GL extension detection dereferences an unregistered anisotropic-filter cvar](https://github.com/jm2/Quake-III-Arena/issues/3) — **high**; anisotropic cvar registered; needs target test.
- [ ] [#6 — Mac renderer overrides r_fullscreen and cannot honor fullscreen configuration](https://github.com/jm2/Quake-III-Arena/issues/6) — **medium**; r_fullscreen override removed; needs target test.
- [ ] [#7 — Mac 16-bit pixel-format fallback still requests 24-bit color](https://github.com/jm2/Quake-III-Arena/issues/7) — **medium**; 16-bit 5/5/5 request; needs target test.
- [ ] [#25 — Fatal engine errors exit with success status](https://github.com/jm2/Quake-III-Arena/issues/25) — **medium**; fatal exit status 1; needs target test.
- [ ] [#26 — Opening the console while recording freezes client time](https://github.com/jm2/Quake-III-Arena/issues/26) — **medium**; record/console clock fix; needs target test.
- [ ] [#34 — AGL console commands re-register on every renderer initialization](https://github.com/jm2/Quake-III-Arena/issues/34) — **medium-low**; AGL command registration guard; needs target test.
- [ ] [#10 — Startup logging always opens the Retro68 console even when viewlog is hidden](https://github.com/jm2/Quake-III-Arena/issues/10) — **medium**; RetroConsole always opens.
- [ ] [#21 — Dedicated server networking busy-spins without console input](https://github.com/jm2/Quake-III-Arena/issues/21) — **medium**; dedicated server busy-spins.
- [ ] [#33 — Synchronous DNS resolution can freeze the Mac client for ten seconds](https://github.com/jm2/Quake-III-Arena/issues/33) — **medium**; synchronous DNS.
- [ ] [#263 — Toolbox is initialized only as a side effect of the first printf](https://github.com/jm2/Quake-III-Arena/issues/263) — **low**; Toolbox initialized only by the first printf.
- [ ] [#264 — Sound Manager runs at 22254.5 Hz while the mixer assumes 22050 Hz](https://github.com/jm2/Quake-III-Arena/issues/264) — **low**; Sound Manager rate 22254.5 Hz vs mixer 22050 Hz.
- [ ] [#266 — Mac console input: navigation keys emit control characters and clipboard paste is a stub](https://github.com/jm2/Quake-III-Arena/issues/266) — **low**; navigation keys emit control chars; clipboard stub.
- [ ] [#267 — Base path at a volume root lacks a trailing ':' and fails catalog enumeration](https://github.com/jm2/Quake-III-Arena/issues/267) — **low**; base path at a volume root.
- [ ] [#30 — Stereo screen path begins two eyes but renders one centered frame](https://github.com/jm2/Quake-III-Arena/issues/30) — **low**; stereo sequencing.

## B6 — Remaining content-hardening parents

The paused September queue. Resume only after B1–B4, and fix false positives against real content (#253) before extending checks.

- [ ] [#35 — Harden interpreted QVM validation and sandbox bounds](https://github.com/jm2/Quake-III-Arena/issues/35) — **high**; QVM sandbox: remaining syscall ranges and retail QVM compatibility.
- [ ] [#41 — Bound RoQ chunks, dimensions, audio output, and decoder cursors](https://github.com/jm2/Quake-III-Arena/issues/41) — **high**; RoQ: retail acceptance.
- [ ] [#42 — Replace legacy BMP, PCX, and TGA loaders with bounded cursor decoders](https://github.com/jm2/Quake-III-Arena/issues/42) — **high**; BMP/PCX/TGA: retail and PPC acceptance.
- [ ] [#43 — Make JPEG I/O length-aware and consolidate duplicate libjpeg APIs](https://github.com/jm2/Quake-III-Arena/issues/43) — **high**; JPEG: retail and screenshot acceptance.
- [ ] [#44 — Validate MD3 and MD4 layout before allocation, copy, or endian swap](https://github.com/jm2/Quake-III-Arena/issues/44) — **high**; MD3/MD4: retail and missionpack acceptance.
- [ ] [#45 — Validate BSP lumps and cross-references transactionally](https://github.com/jm2/Quake-III-Arena/issues/45) — **high**; BSP: remaining payload and graph validation.
- [ ] [#46 — Enforce fixed limits and ownership in shader, skin, and font parsers](https://github.com/jm2/Quake-III-Arena/issues/46) — **high**; shader/skin/font: derived rendering conversions.
- [ ] [#47 — Validate AAS lumps and graph indexes before enabling bot world](https://github.com/jm2/Quake-III-Arena/issues/47) — **high**; AAS: validate the whole file before loaded.
- [ ] [#48 — Bound bot preprocessor, token, and path operations](https://github.com/jm2/Quake-III-Arena/issues/48) — **high**; bot parser: work and recursion limits (open PRs #214–#217).

## B7 — Performance

Measure on target after B1; the product still builds at -O0.

- [ ] [#28 — Release builds force debug optimization and disable key GL fast paths](https://github.com/jm2/Quake-III-Arena/issues/28) — **medium**; Release builds at -O0 with GL fast paths disabled.
- [ ] [#249 — Per-vertex float guards call out-of-line Com_Memcpy (5-13x slower lighting loops)](https://github.com/jm2/Quake-III-Arena/issues/249) — **medium**; per-vertex float guards 5–13x slower.
- [ ] [#250 — Interpreted VM executes ~3x more PPC instructions per QVM instruction](https://github.com/jm2/Quake-III-Arena/issues/250) — **medium**; interpreted VM about 3x more instructions per op.
- [ ] [#251 — Map-load preflights and non-recursive box queries add 1.2-2.3x load and query cost](https://github.com/jm2/Quake-III-Arena/issues/251) — **low**; map-load preflights and box queries 1.2–2.3x slower.

## B8 — Team Arena

Team Arena cannot reach its menu today; keep it building (BUILD_TEAM_ARENA=ON) and fix after the base game.

- [ ] [#235 — Team Arena executable never mounts missionpack data on a Finder launch](https://github.com/jm2/Quake-III-Arena/issues/235) — **high**; executable never mounts missionpack.
- [ ] [#234 — Team Arena cgame uses the UI module's ui_shared.c and issues UI syscalls in cgame context](https://github.com/jm2/Quake-III-Arena/issues/234) — **high**; cgame uses the UI module ui_shared.c.
- [ ] [#11 — Team Arena simple parser cannot represent retail menu syntax](https://github.com/jm2/Quake-III-Arena/issues/11) — **high**; simple parser cannot read retail menus.
- [ ] [#12 — Team Arena skips model and bot discovery](https://github.com/jm2/Quake-III-Arena/issues/12) — **high**; model and bot discovery skipped.
- [ ] [#49 — Make Team Arena UI menu and reload failures allocation-safe](https://github.com/jm2/Quake-III-Arena/issues/49) — **moderate-high**; UI allocation and reload failures.

## B9 — Low-risk metadata

- [ ] [#31 — Generated icl8/ics8 icons use adaptive indexes without a matching CLUT](https://github.com/jm2/Quake-III-Arena/issues/31) — **low**; icon CLUT.
- [ ] [#32 — MacBinary encoder writes invalid zero creation and modification dates](https://github.com/jm2/Quake-III-Arena/issues/32) — **low**; MacBinary dates; host tests pass, target check remains.
- [ ] [#52 — MacBinary filename length counts Unicode characters instead of encoded bytes](https://github.com/jm2/Quake-III-Arena/issues/52) — **low**; MacBinary name bytes; host tests pass, independent reader remains.
## Next steps

- Merge B0 so CI finishes on every PR and new issues can be added here.
- Clear B1, then B2 items #226/#269/#227 so a launchable application comes
  out of a pinned toolchain.
- Add real-content (#253) and 32-bit big-endian (#224) CI before resuming any
  B6 hardening.
- Work B4 remote-security items before the paused parser queue.
- First target run (base game, retail data, real hardware): main menu, a bot
  match on q3dm1, disconnect, normal quit, fatal-exit status. Record results
  here and on each `needs target test` issue.
