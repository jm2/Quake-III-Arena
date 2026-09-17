# VM call argument validation — 2026-09-17

The sixth step for issue #35 replaces the assumption that a call number and
varargs are contiguous with a counted argument array. The VM_Call macro uses
C99 array initializers and GNU empty-varargs support, both supported by the
project's GNU99 Mac compiler mode. Supplied arguments are evaluated once,
converted to VM integers, checked against the twelve-parameter limit, and
copied into a zero-filled command-plus-twelve array.

Native dispatch retains the fixed retail vmMain signature required by PPC.
Compiled and interpreted dispatch receive the same explicit array and use
`VM_SetupCallFrame` to marshal it, including x86 and both PPC compiled backends.
The shared helper validates and reserves space for all thirteen words rather than reading ten words
from the address of a single call-number parameter. x86 return-frame validation
uses the same frame size. Missing parameters are
zero across all modes.

## Validation

The new VM call ASan/UBSan harness calls the real dispatcher with native,
compiled, and interpreted execution stubs. It checks zero, one, and twelve
parameters, negative values, zero padding, single argument/VM expression
evaluation, dispatcher state/restoration, and rejection of invalid VM/counts
before dispatch. The shared frame is checked for all thirteen values, return
slots, and preservation of preceding memory. The interpreter execution harness uses the full thirteen-word
array and updated frame bounds. All five sanitizer runners and eight Python
checks pass. The new call harness is included in Portable CI.

Both Retro68 products build without compiler warning/error diagnostics and
pass PEF validation with the temporary libraries described in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,885 | `e84a733c8a2d98ac70888ec3cebfdabc314e98546ddb1737c75c2ceab19d96a1` |
| Quake3_TeamArena | 3,826,555 | `b06fb19730677c5f63aaad294ddadb322d1bebfa93315bd1d703e21d0153eb9d` |

## Remaining acceptance

Keep #35 open. Syscall-specific pointer/range handling remains incomplete.
The call harness checks dispatcher arguments with execution stubs; the runtime
harness checks the interpreter with explicit arrays. Retail baseq3/Team Arena
and Mac OS 9 execution remains deferred. No native Windows build or legacy
MSVC preprocessor compatibility result is claimed.
