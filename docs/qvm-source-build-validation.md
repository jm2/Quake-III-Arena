# Restore retail-format source QVM builds — 2026-09-18

A full legacy source build finds three blockers: COM_StripExtension calls strrchr
absent from the QVM libc; both client manifests omit cg_particles and its animation
lookup calls absent stricmp; UI diagnostics use printf/fflush/stdout unavailable to
QVM modules. These dependencies also prevent validating source modules alongside
commercial 1.32c retail QVM compatibility.

Use existing Q_strrchr and Q_stricmp helpers, compile/link cg_particles in both
client batch recipes/manifests, and route existing UI diagnostic content through
the existing trap_Print/va console service. Keep path stripping semantics, ASCII animation names, menu behavior,
retail syscall tables, bytecode format and protocol. Native diagnostics now use the
same console service as QVM diagnostics. Static/QVM module selection remains a
separate open issue; this step restores source builds.

## Validation

The repository legacy lcc/q3asm sources build as 32-bit host tools. A 64-bit host
build of this old compiler crashes; the installed host multilib permits a native
32-bit build. Use make -r with explicit BUILDDIR/TEMPDIR under the scratch root,
CC/LD='gcc -m32', CFLAGS='-O2 -std=gnu89 -fcommon'. The -r option prevents a missing
yacc from regenerating the checked-in grammar. Native tools retain their original
sources. Temporary tool outputs and compiler intermediates stay under the scratch
root; no tools or binaries are committed.

Compile every C entry using -DQ3_VM -S -Wf-target=bytecode -Wf-g and the module/game/
UI include directories, adding -DMISSIONPACK for Team Arena. Copy syscall assembly
and link the exact source order from game/game_ta, cgame/cgame_ta, q3_ui/q3_ui and
ui/ui manifests. Resolve shared game/UI source paths as their batch recipes do.
All six complete products link with zero assembler errors and retail 0x12721444
QVM headers. Historical legacy compiler warnings for void-pointer NULL assignments
to UI function pointers remain; PPC compilation emits no warnings.

Artifact sizes/hashes are recorded below from the complete source builds. They
are build evidence, not execution of retail assets or target acceptance.

| Module | QVM bytes | SHA-256 |
| --- | ---: | --- |
| game | 470,136 | `e255b05d6c11ad006f6025d349f39bd709780e0f01c1d6ef2e1659a06b7d5812` |
| game_ta | 552,116 | `2eb77600eee86266bb76ff992a898f3e1c023f047ad29de00ee6620896a89215` |
| cgame | 325,620 | `3a84375d417d5f8c46cd217519bdd41618a523b6a22ab33ed2e5e9c7468baca7` |
| cgame_ta | 489,804 | `3e523a757a613969670aa7dd5db73210bb14e08d1a70317e85698620698d2968` |
| q3_ui | 275,276 | `f9508ee96bb237f67bf7e6e4284be26c63e1f67eecee6efbc77abc648351aa0d` |
| ui | 284,848 | `3237aae43e6541729faa332b98e02a81dc0ca79434eb6a5a8dd3e29fa0077304` |

Existing actual q_shared regressions pass Clang ASan/UBSan and optimized GCC,
including ordinary/in-place extension removal, dots before slashes and truncated
outputs. GCC/Clang CI already runs this fixture. Five ledger/manifest checks and
diff checks pass. Both PPC products build with zero diagnostics and valid PEF
headers; final artifact sizes/hashes are recorded below.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,770,803 | `6c5687860fba58eb6bf829c4e545e89e4ee0f3b83596d063e7abbb9d051a9192` |
| Quake3_TeamArena | 3,919,377 | `fd059db0cc5c3c6c920262caeb3ed85cf2090adae0459d7994a138adc82be3d8` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #35/#13 open for remaining sandbox/syscall bounds, retail QVM execution,
static-module selection and deferred Mac OS 9 acceptance. This step needs no retail
asset files; it does not claim that runtime/mod acceptance has completed.
