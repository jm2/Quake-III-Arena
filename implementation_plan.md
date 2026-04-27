# Implementation Plan: Quake III Arena 1.32c on Mac OS 9 via Retro68

This plan synthesizes a holistic review of the current tree against its stated goal:
**build Q3 1.32c (with modern CVE fixes) for Mac OS 9 using the Retro68 cross
toolchain**, and translates the review into a concrete, ordered work program.

The current symptom is: app launches, the q3_ui CD-key screen draws and re-wipes
~20 times, then the machine halts. The plan starts by attacking the most likely
direct causes of that symptom, then works outward to structural debt that is
slowing every fix down.

Earlier review notes (preserved in conversation) framed the problem as "Q3_STATIC
dispatch may be missing." That hypothesis was checked against the tree and
**rejected**: `code/mac/mac_main.c:848` implements `Sys_LoadDll` correctly,
returning `Game_vmMain` / `CGame_vmMain` / `UI_vmMain` and calling each module's
`*_dllEntry` with the syscall pointer. The renames in `CMakeLists.txt` are
matched by real, callable definitions. That avenue is closed; the bug is
elsewhere. The plan reflects that.

---

## 0. Strategic framing (read first)

Three independent hard problems are stacked here:
1. The id 2005 GPL drop has none of ioquake3's accumulated security work.
2. Mac OS 9 is cooperatively scheduled with idiosyncratic conventions.
3. Retro68 is GCC-with-classic-Mac patches; its sharp edges (PEF emission,
   PowerPC calling conventions, MPW-vs-GCC pragma differences) are
   under-documented at this codebase size.

The single biggest leverage is **reduce scope while debugging.** Until the base
game runs to a playable state, treat every additional concern (Team Arena,
networking, sound, the resource/icon polish path) as a distraction. Phases
below are ordered to enforce that.

A rebase onto Quake3e was considered. **Recommendation: do not rebase yet.**
The tree already contains a working `Sys_LoadDll` static-dispatch shim, a
working WaitNextEvent pump, partial DrawSprocket / InputSprocket integration,
and the Raven JK/JA-derived `code/mac/` is a real upgrade over the id 2005 Mac
code. A rebase costs the platform-layer work that is closest to running. Defer
that decision to Phase 6, after the current symptoms are diagnosed.

---

## 1. Verified state of the tree (as of this plan)

Findings from reading the current tree, used to scope the plan:

- **Q3_STATIC dispatch IS wired.** `code/mac/mac_main.c:848-866` implements
  `Sys_LoadDll` and routes to the renamed entry points. The `CMakeLists.txt`
  `-DvmMain=Game_vmMain` etc. renames are real, callable, and correct.
- **`Sys_GetCwd` returns `""`** (`code/mac/mac_main.c:402`). It contains a large
  block of FSMakeFSSpec/FSpOpenDF/fopen diagnostic probes (lines 309-399) with
  hardcoded paths like `Macintosh HD:Desktop Folder:Quake 3 Arena:baseq3:pak0.pk3`.
  This means `fs_homepath` and `fs_basepath` are both empty, so all FS
  operations resolve relative to the current working volume, not the
  application directory.
- **`PATH_SEP` is correctly `:` on `__MACOS__`** (`code/game/q_shared.h:241`).
  `FS_BuildOSPath` normalizes via `FS_ReplaceSeparators`
  (`code/qcommon/files.c:458-466`).
- **The CD-key file path is built with hardcoded `/`** in three places in
  `code/qcommon/common.c` (lines 2346, 2376, 2408): `sprintf(fbuffer,
  "%s/q3key", filename)`. The result is then passed through
  `FS_SV_FOpenFileRead` which calls `FS_BuildOSPath`, which normalizes — so
  this *is* fine in isolation, but it's brittle and worth tightening.
- **`SIZE` resource is generous**: 128 MB preferred, 64 MB minimum
  (`code/mac/mac_resources.r:48-49`). `cfrg` resource exists (lines 10-20).
- **WaitNextEvent is pumped** in `Sys_SendKeyEvents` (`code/mac/mac_event.c:366`),
  called from `Sys_GetEvent` and from `GLimp_EndFrame` (`code/mac/mac_glimp.c:840`).
- **Only one Pascal callback in the tree**: `NotifyProc` in
  `code/mac/mac_net.c:64`. There are no DSp / ISp / Sound Manager / dialog
  callbacks registered through `New*UPP`, because the code doesn't register any
  — DrawSprocket and InputSprocket are used in pull-mode (`DSpContext_*`,
  `ISpElement_GetNextEvent`, `ISpElement_GetSimpleState`). UPP audit is still
  worth doing for `NotifyProc` but the calling-convention risk surface is much
  smaller than the prior review estimated.
- **CD-key menu trigger** is `code/q3_ui/ui_menu.c:269-284`. Every time
  `UI_MainMenu()` is entered with `ui_cdkeychecked.integer == 0` and
  `trap_VerifyCDKey()` returning false, it pushes `UI_CDKeyMenu()`. That is the
  loop currently observed.
- **Hot-path logging is heavy**: `Sys_LogPrintf` in `code/mac/mac_main.c:108`
  does a synchronous `fopen("retro68_console.log", "a") / fwrite / fclose`
  *every* call. Calls happen inside `Sys_GetEvent` — i.e. multiple times per
  frame. On classic Mac this is enormously expensive and may itself be enough
  to wedge the cooperative scheduler.
- **CMake build flags are problematic** (`CMakeLists.txt:9-10`):
  `-Os -ffunction-sections -fdata-sections -g0 ... -Wl,--gc-sections -Wl,-s`.
  Strip-before-MakePEF and aggressive section GC are both known foot-guns on
  the Retro68 / PEF path.
- **OBJECT/STATIC asymmetry**: `q3ui_lib` was promoted to STATIC with the
  comment "to work around Retro68 linker crash with OBJECT libraries"
  (`CMakeLists.txt:100`). Six other module libraries (`game_obj`, `cgame_obj`,
  `ui_obj`, plus the three `_mp_obj` variants) are still OBJECT.

---

## 2. Most likely direct causes of the CD-key wipe-then-halt

Updated hypotheses, ranked by likelihood given the verified tree:

**H1 — File I/O on the hot path is wedging the cooperative scheduler.**
`Sys_LogPrintf` synchronously opens, writes, and closes
`retro68_console.log` on every call, including from inside `Sys_GetEvent`,
`Sys_SendKeyEvents`, and the CDKey draw/event loop. On Mac OS 9 each
`fopen/fwrite/fclose` cycle traps into the File Manager, which yields
cooperatively but is not free; doing it tens of times per frame can starve
WaitNextEvent and the AGL flusher. The "wipes then halts" symptom matches
this exactly: visible animation continues for a while because the renderer
runs, but the system runs out of slack and stops servicing interrupts.

**H2 — `fs_homepath`/`fs_basepath` both empty → CD key file never persists.**
`Sys_GetCwd()` returns `""` (line 402), so `Sys_DefaultBasePath()` and
`Sys_DefaultHomePath()` both return `""`. `FS_BuildOSPath("", "baseq3",
"q3key")` produces `:baseq3:q3key`, which on Mac OS Classic resolves against
the current working volume — not the directory the app launched from. If
that directory doesn't have a `baseq3` subfolder, `Com_ReadCDKey` always
fails, `cl_cdkey` stays as 16 spaces, `trap_VerifyCDKey` returns false, and
`UI_MainMenu` pushes `UI_CDKeyMenu` again. (Note: even if the user typed a
key, `Com_WriteCDKey` would also fail to persist it, so the loop never
breaks.)

**H3 — Input never reaches the menu.**
The CDKey menu's text field expects `SE_CHAR` events. `Sys_SendKeyEvents`
generates them in `DoKeyDown` (`code/mac/mac_event.c:133-143`). Verify those
events actually flow through `Com_EventLoop` to the UI when the CDKey menu
is on top. If `cls.keyCatchers` isn't `KEYCATCH_UI` at that point, keys go
to the console binding instead. (The InputSprocket suspend-on-`!CA_ACTIVE`
guard in `Sys_Input` was disabled — `code/mac/mac_input.c:166-173` — which
is good for the menu, but worth confirming the keyboard side also works.)

**H4 — `--gc-sections` + `-Wl,-s` corrupting the PEF.**
The pre-strip + section GC combination can drop sections that MakePEF needs
to emit a valid PEF. Failure modes are subtle: the binary loads, runs for
a while, then dies on something that referenced a GC'd weak symbol. This
matches "halts after some frames."

**H5 — Heap fragmentation from per-frame string ops.**
`Sys_LogPrintf` plus the `vsprintf` formatting plus the printf calls plus
the q3 console buffer all touch the C runtime's heap. Mac OS 9 heap
compaction is cooperative; sustained churn fragments the application heap.
Symptom: app runs OK for ~30 s then can't service an allocation and halts.

H1, H2, and H4 are independently sufficient to produce the observed symptom.
The plan addresses them in that order.

---

## 3. Phase 1 — Stop the bleeding (highest leverage, lowest risk)

**Goal:** make the build debuggable and remove the most likely cause of the
halt before changing any application logic. Each item should be its own
commit so a regression can be bisected.

### 3.1 Strip dangerous build flags
File: `CMakeLists.txt:9-10`. Replace with debug-friendly flags:
- Drop `-Wl,-s` (strip before PEF is dangerous; strip after if at all).
- Drop `-Wl,--gc-sections` (likely dropping sections MakePEF needs).
- Drop `-ffunction-sections -fdata-sections` (their only purpose was to
  feed `--gc-sections`).
- Switch `-Os -g0` → `-O0 -g`. Bigger binary; not the problem.
- Add `-fno-strict-aliasing` (Q3 type-puns aggressively; default GCC
  optimization assumes otherwise).

Acceptance: `Quake3` (post-MakePEF) launches at least as far as it does
today, and is debuggable.

### 3.2 Make `Sys_LogPrintf` non-blocking
File: `code/mac/mac_main.c:108-139`. The synchronous fopen/fwrite/fclose
per call is almost certainly contributing to the halt. Options, in
preference order:
1. Buffer to the in-memory ring (`retroLogBuffer`) only; flush to disk
   on `Sys_Quit`, `Sys_Error`, and via the existing Cmd-D panic key.
2. If on-disk persistence during normal operation is required, open the
   file once at startup and only `fwrite` (no fclose) per call, with a
   periodic `fflush` (every N seconds, not per call).

Also: the call sites in `Sys_GetEvent` (`mac_main.c:213-221`) log around
each `Sys_SendKeyEvents` / `Sys_Input`. Gate those behind a `developer`
cvar check, not at compile time, so they can be re-enabled without rebuilding.

Acceptance: `Sys_GetEvent` returns in O(microseconds), not O(File Manager
trap roundtrips).

### 3.3 Give the engine a real working directory
File: `code/mac/mac_main.c:309-411`. Replace the diagnostic `Sys_GetCwd`
with a real implementation that returns the application's parent directory
as an HFS path string (e.g. `MyDisk:Quake 3 Arena:`). At startup, derive
this from `ProcessSerialNumber` + `GetProcessInformation` →
`ProcessInfoRec.processAppSpec`. Cache the result.

Then make `Sys_DefaultBasePath` / `Sys_DefaultHomePath` return that
directory. Strip the diagnostic FSMakeFSSpec/fopen probes — they will be
unreachable once the path is correct, and they're noise in the log.

Acceptance: `fs_basepath` cvar at startup is a non-empty HFS path; an
explicit `dir` console command lists pak files under `baseq3:`.

### 3.4 Drop Team Arena from the build
File: `CMakeLists.txt:162-173, 198-211`. Comment out or guard
`Quake3_TeamArena` and the `*_mp_obj` libraries behind a CMake option
defaulting to OFF. Halves link time, shrinks symbol tables, and removes a
class of "did this fix break the MP build" noise from every iteration.

Acceptance: `cmake --build` produces only `Quake3`; the MP path can still
be re-enabled with a single flag flip later.

### 3.5 Promote remaining OBJECT libraries to STATIC
File: `CMakeLists.txt:68, 84, 92, 118, 126, 134`. Switch each `add_library
... OBJECT` to `STATIC` and link via `target_link_libraries(Quake3 PUBLIC
... -Wl,--whole-archive game_lib cgame_lib -Wl,--no-whole-archive ...)`
(or the equivalent `$<LINK_GROUP:RESCAN,...>` pattern in newer CMake).

The existing comment on `q3ui_lib` (line 100) already calls out a
"Retro68 linker crash with OBJECT libraries"; if it bites for q3_ui it
likely silently miscompiles for the others.

Acceptance: link succeeds and the resulting binary still launches.

### 3.6 Verify PEF after MakePEF
File: `build_mac.sh:46-76`. After MakePEF, dump and grep:
- `MakePEF -dump Quake3` (or whatever the Retro68 dump tool is) — confirm
  the import table contains `InterfaceLib`, `DrawSprocketLib`,
  `InputSprocketLib`, `OpenGLLibrary`, `OpenTransportLib`, etc.
- Confirm exported `main` exists.
- Confirm code section size > a sanity threshold (e.g. > 1 MB; if it's
  under 200 KB after we removed `--gc-sections`, something is wrong).

Acceptance: a CI-style sanity check that fails fast if MakePEF produces
a bad PEF.

---

## 4. Phase 2 — Confirm the failure mode

With Phase 1 done, re-run on target. Three outcomes:

**A. The wipe-loop is gone.** Move to Phase 3 (CD-key validation flow).
**B. The wipe-loop is faster or different.** That confirms the build
flags / logging were exacerbating it, not causing it. Go to Phase 3.
**C. The wipe-loop is unchanged.** Use the now-debuggable build to
instrument the menu push path:

Add three `Com_Printf` calls and observe:
- `code/q3_ui/ui_menu.c:280` — log `key`, the result of `trap_VerifyCDKey`,
  and `ui_cdkeychecked.integer`.
- `code/qcommon/common.c:2341` (`Com_ReadCDKey`) — log `fbuffer`, whether
  `FS_SV_FOpenFileRead` returned a handle, and the bytes it read.
- `code/q3_ui/ui_atoms.c:86` (`UI_PushMenu`) — log `menusp` and the menu
  function being pushed each frame.

Expected output if H2 is the cause: `Com_ReadCDKey` opens nothing (homepath
is empty / wrong volume), `cl_cdkey` is 16 spaces, `trap_VerifyCDKey`
returns false, `UI_PushMenu(UI_CDKeyMenu)` fires every UI frame.

If H3 is the cause: `Com_ReadCDKey` runs once, but a second push happens
after the user types — meaning the SE_CHAR events aren't routed to the
menu's text field.

---

## 5. Phase 3 — Fix the CD-key flow itself

Ordered by what the Phase 2 instrumentation reveals.

### 5.1 If `Com_ReadCDKey` never finds the file
- Verify Phase 1.3 actually set `fs_homepath` to something real.
- Verify the q3key file lives at `${fs_homepath}:baseq3:q3key`.
- Verify `Sys_Mkdir` (currently a no-op at `code/mac/mac_main.c:873`) is
  implemented — `Com_WriteCDKey` will silently fail to persist if the
  directory doesn't exist and Sys_Mkdir does nothing.
- Tighten the `sprintf(fbuffer, "%s/q3key", filename)` calls in
  `code/qcommon/common.c:2346, 2376, 2408`: these work *only* because
  `FS_BuildOSPath` happens to normalize. Defense in depth: change to
  `Com_sprintf(fbuffer, sizeof(fbuffer), "%s%cq3key", filename, PATH_SEP)`.

### 5.2 If keyboard input doesn't reach the menu field
- Add a `Com_Printf` in the q3_ui MField handler to confirm SE_CHAR /
  SE_KEY events arrive when the menu is on top.
- Verify `cls.keyCatchers & KEYCATCH_UI` while CDKey menu is active.
- Audit `code/mac/mac_event.c:114-131` (`vkeyToQuakeKey`). For the
  menu-input path the SE_CHAR is the relevant event; SE_KEY is for
  bindings. If `event->message`'s `charCodeMask` decode is wrong on PPC
  (endian / bit position), neither will work.

### 5.3 Hard-bypass for development
Add a `+set ui_cdkeychecked 1` startup option, and/or a debug-only `cl_cdkey`
default that auto-validates. This lets the app skip past the CDKey menu so
you can test main menu → in-game without solving the persistence problem
first.

Acceptance: with the bypass set, `UI_MainMenu` is reached and stays
on-screen; main menu items are clickable / keyboard-navigable.

---

## 6. Phase 4 — Stabilize the platform layer

These are required for stability beyond the CD-key screen but not blocking
that screen specifically. Tackle after Phase 3.

### 6.1 Path handling audit
- Grep for every `"/"` string literal in `code/qcommon/`,
  `code/client/`, `code/server/`. The known offenders are
  `code/qcommon/common.c:2346, 2376, 2408`. Anything that constructs a
  path component without going through `FS_BuildOSPath` is a Mac OS 9
  hazard.
- Grep for every `fopen` / `FILE *` call. On Mac, those go through
  Retro68's libc shim which expects HFS-style paths. Anything passing a
  forward-slash path will fail silently.
- Audit `Sys_Mkdir` (`code/mac/mac_main.c:873`) and implement it via
  `FSpDirCreate` or `DirCreate`.

### 6.2 PStrings everywhere they're needed
- `code/mac/mac_main.c:53-68` has `PStringToCString` /
  `CStringToPString`. Confirm every Toolbox call that takes a `Str255`
  is fed a Pascal string, and every Toolbox call that returns one is
  decoded before use.
- The diagnostic probes at lines 333-379 already do this dance correctly
  for FSMakeFSSpec; use the same pattern in production code.

### 6.3 Pascal callback / UPP audit
- `code/mac/mac_net.c:64` `pascal void NotifyProc(...)`. Confirm:
  - Retro68's `<OpenTransport.h>` declares the OT notifier with `pascal`
    too.
  - `OTInstallNotifier` is called with the result of
    `NewOTNotifyUPP(NotifyProc)`, not a bare function pointer.
  - Both prototype and definition agree on `pascal`.
- This is the only known UPP risk surface; DSp/ISp are used in pull mode
  in this tree.

### 6.4 Networking gating
- For initial bring-up, gate `Sys_InitNetworking` behind a cvar default
  off. Open Transport startup is non-trivial on Mac OS 9 and a known
  source of long startup hangs. Re-enable after the rest is stable.

### 6.5 Sound gating
- `code/mac/mac_snddma.c` (140 lines, mostly stubs). Ensure
  `S_Init` returns false / no-op cleanly when sound init fails, so
  startup doesn't hang on a missing Sound Manager path.

---

## 7. Phase 5 — Renderer and AGL

After Phase 4, the engine should reach `UI_MainMenu` reliably. From there:

- Profile `GLimp_EndFrame` (`code/mac/mac_glimp.c:831-843`) — the
  unconditional `Sys_SendKeyEvents` per frame is correct for cooperative
  multitasking but pairs badly with the synchronous logging if Phase 1.2
  isn't done.
- Confirm `aglSwapBuffers` actually flips. The CDKey wipe animation
  rendering implies AGL is at least running, but a vsync stall can still
  manifest as "draws fine, halts" on some hardware.
- Audit `MacGamma.c` (486 lines). Gamma fade-in/out interacts with DSp's
  context state machine; mismatched fade calls can lock the display.

Acceptance: enter / exit a single map (devmap on a small map like q3dm17
or pro-q3dm6) without a crash.

---

## 8. Phase 6 — CVE backports and rebase decision

Only after Phase 5 is stable (i.e. you can play a single bot match) is it
sensible to revisit:

### 8.1 CVE backport audit
The "1.32c with all modern CVEs fixed" goal needs a tracked checklist.
Required minimum set:
- CVE-2017-11721 (cl_guid info-string truncation)
- CVE-2017-11722 (server info-string handling)
- CVE-2018-12100 (out-of-bounds read in client)
- `getstatus` / `getinfo` reflection amplification mitigation
- Zone allocator hardening (heap overflows in `Z_Malloc` callers)
- Filesystem path traversal (`..` checks, currently only at
  `code/qcommon/files.c:506-507`)

For each: a commit referencing the upstream ioquake3/Quake3e commit
hash, with the patch applied to this tree's matching file. Track in
`SECURITY.md` (separate from this plan).

### 8.2 Rebase decision
Re-evaluate Quake3e rebase only after the current platform layer is
debugged. Two questions to answer at that point:
- Is the platform layer stable enough that lifting it to Quake3e's
  `code/macclassic/` is straightforward?
- Are CVE backports cumulatively painful enough that taking them all
  for free is worth the rebase cost?

If both are yes, rebase. The Raven JK/JA-derived Mac code in `code/mac/`
ports forward without large changes — the engine API surface
(`Sys_LoadDll`, `Sys_GetEvent`, `GLimp_*`, `IN_*`, `S_*`) is stable
across the Q3 1.32 family. The work is real but bounded.

---

## 9. Things explicitly out of scope until Phase 5+

Discipline matters more than completeness here:

- Team Arena / mission pack — gated off in Phase 1.4.
- Multiplayer / Open Transport — gated off in Phase 4.4.
- Sound — minimally stubbed; revisit in Phase 4.5.
- Icon polish, AppleDouble creator-code consistency, BNDL signature
  alignment (`generate_icon_r.py` uses `'Q3A '`, `create_appledouble.py`
  uses `'IDQ3'`). Cosmetic; fix after game runs.
- 68k support. The whole pipeline targets PowerPC PEF.
- Mac OS X / Carbon hybrid build. The current tree is classic-only.

---

## 10. Hard rules for the next iteration

These exist because the prior LLM-driven iterations introduced ghost bugs
faster than they fixed them. Every change must satisfy *all* of:

1. **One commit, one concern.** Build-flag changes don't ride along with
   logic changes. Logic changes don't ride along with refactors.
2. **Reproduce before fix.** Every fix commit message names the symptom
   it reproduces (in emulator or on hardware) and the symptom that
   confirms the fix.
3. **No new abstractions.** No "while we're here, let's refactor."
   Q3 is a 1.32-locked codebase; the value of clever is negative.
4. **Logging is gated by `developer` cvar, not `#ifdef DEBUG`.**
   So we don't have to rebuild to investigate.
5. **No silent fallbacks.** If `Sys_GetCwd` can't determine the
   directory, `Sys_Error` — don't return `""` and let the engine limp
   into a corrupted state.
6. **Don't widen scope until Phase 6.** Mission pack, multiplayer,
   sound, icons, OS X compatibility — all wait.

---

## 11. Suggested next session (concrete starting steps)

When picking this up, in order:

1. Apply 3.1 (build flags) — single CMakeLists.txt commit.
2. Apply 3.2 (sync logging) — single mac_main.c commit.
3. Build, run on target. If symptom changes meaningfully, document it.
4. Apply 3.3 (Sys_GetCwd) — single mac_main.c commit.
5. Apply 3.4 (drop Team Arena) — single CMakeLists.txt commit.
6. Apply 3.5 (OBJECT → STATIC) — single CMakeLists.txt commit.
7. Build, run. Re-evaluate symptom. Branch into Phase 2 or 3 based on
   what the now-debuggable build shows.

Stop here. Do not start Phase 4 until Phase 3 ends in "main menu reachable
and stable for at least 60 seconds."
