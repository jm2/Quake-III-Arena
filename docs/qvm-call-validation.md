# VM call argument validation — 2026-09-17

The sixth step for issue #35 replaces the assumption that a call number and
varargs are contiguous with a counted argument array. The VM_Call macro uses
C99 array initializers and GNU empty-varargs support, both supported by the
project's GNU99 Mac compiler mode. Supplied arguments are evaluated once,
converted to VM integers, checked against the twelve-parameter limit, and
copied into a zero-filled command-plus-twelve array.

Native dispatch retains the fixed retail vmMain signature required by PPC.
Compiled and interpreted dispatch receive the same explicit array. Interpreter
entry reserves space for all thirteen words rather than reading ten words
from the address of a single call-number parameter. Missing parameters are
zero across all modes.

## Validation

The new VM call ASan/UBSan harness calls the real dispatcher with native,
compiled, and interpreted execution stubs. It checks zero, one, and twelve
parameters, negative values, zero padding, single argument/VM expression
evaluation, dispatcher state/restoration, and rejection of invalid VM/counts
before dispatch. The interpreter execution harness uses the full thirteen-word
array and updated frame bounds. All five sanitizer runners and eight Python
checks pass. The new call harness is included in Portable CI.

Both Retro68 products build without compiler warning/error diagnostics and
pass PEF validation with the temporary libraries described in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,885 | `ebbf6eda10d876128c596a15969250ecde64c2645f7e3fd0139d08d8a2ffa21e` |
| Quake3_TeamArena | 3,826,555 | `598e143bb7b613cc8615763061f0dc273dd7a77198bcc20248639c42694a59e2` |

## Remaining acceptance

Keep #35 open. Syscall-specific pointer/range handling remains incomplete.
The call harness checks dispatcher arguments with execution stubs; the runtime
harness checks the interpreter with explicit arrays. Retail baseq3/Team Arena
and Mac OS 9 execution remains deferred. No native Windows build or legacy
MSVC preprocessor compatibility result is claimed.
