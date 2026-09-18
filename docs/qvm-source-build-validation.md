# Restore retail-format source QVM builds — 2026-09-18

A full legacy source build finds three blockers: COM_StripExtension calls strrchr
absent from the QVM libc; both client manifests omit cg_particles and its animation
lookup calls absent stricmp; UI diagnostics use printf/fflush/stdout unavailable to
QVM modules. These dependencies also prevent validating source modules alongside
commercial 1.32c retail QVM compatibility.

Use existing Q_strrchr and Q_stricmp helpers, compile/link cg_particles in both
client batch/shell recipes and manifests, and route existing UI diagnostic content through
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
Both client recipes define CGAME, which is also required when compiling their
shared UI source. Native syscall C wrappers are excluded from QVM recipes; each
manifest links the existing syscall assembly, using portable forward slash paths.
All six complete products link with zero assembler errors and retail 0x12721444
QVM headers. Historical legacy compiler warnings for void-pointer NULL assignments
to UI function pointers remain; PPC compilation emits no warnings.

Artifact sizes/hashes below come from complete source builds combined with master
`a0364783c25e7b242e2c0324275cb9df0086239e` through bot steps #170 and CI #180.
The workflow conflict combines all current regression scripts with the five QVM
recipe syntax checks. All six QVM products and both PPC products were rebuilt
from this combined source, and shared-function checks passed both compilers.
Artifact sizes/hashes are recorded below from the complete source builds. They
are build evidence, not execution of retail assets or target acceptance.

| Module | QVM bytes | SHA-256 |
| --- | ---: | --- |
| game | 470,592 | `39c26916c91a0eef34d066fc9a297347a04d4d10bd3b06414e11cc0f05a80a1a` |
| game_ta | 552,572 | `28235cf6600f0b285e2080acc90ad284b71bbfa15cfca8d6cd5480fe0349ae40` |
| cgame | 325,620 | `3a84375d417d5f8c46cd217519bdd41618a523b6a22ab33ed2e5e9c7468baca7` |
| cgame_ta | 489,804 | `efe068bf875c0b870c218a42cb73e74f85893a185c79254782d1d28a1fc98e07` |
| q3_ui | 275,276 | `f9508ee96bb237f67bf7e6e4284be26c63e1f67eecee6efbc77abc648351aa0d` |
| ui | 284,848 | `3237aae43e6541729faa332b98e02a81dc0ca79434eb6a5a8dd3e29fa0077304` |

Five actual Unix recipes (game/game_ta, cgame/cgame_ta and q3_ui) also run
from clean module directories with the 32-bit tools on PATH. Only the assembler
output destination is overridden into the owned scratch tree. Compiler logs have
no errors; assemblers report zero errors and all five outputs have valid QVM
headers. Both client recipes produce cg_particles.asm. Those recipe checks preceded the current-master rebuild; recipe inputs are
unchanged. Updated complete QVM/PPC products are recorded here.

Existing actual q_shared regressions pass Clang ASan/UBSan and optimized GCC,
including ordinary/in-place extension removal, dots before slashes and truncated
outputs. GCC/Clang CI already runs this fixture. Five ledger/manifest checks and
diff checks pass. Both PPC products build with zero diagnostics and valid PEF
headers; final artifact sizes/hashes are recorded below.

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,783,921 | `ac2d6c2679fbc6763e6ef84d6ac776ba769c8f96fda82e73225304aa0d062e67` |
| Quake3_TeamArena | 3,932,495 | `2ef8204a0c4df7c20093715c04797ec656c1968c5dcfd34e21d861d0fa8db8aa` |

Temporary toolchain libraries: [loading evidence](qvm-loading-validation.md).

## Remaining acceptance

Keep #35/#13 open for remaining sandbox/syscall bounds, retail QVM execution,
static-module selection and deferred Mac OS 9 acceptance. This step needs no retail
asset files; it does not claim that runtime/mod acceptance has completed.
