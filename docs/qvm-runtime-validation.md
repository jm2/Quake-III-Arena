# QVM runtime stack and control-flow validation — 2026-09-17

The third step for issue #35 makes operand-stack, program-stack, and dynamic
control-flow checks unconditional in the interpreter. It checks stack growth
and operand availability before executing each opcode, validates entry and
ENTER/LEAVE frames before accessing them, and bounds CALL/JUMP instruction
indices. Writable return addresses must identify a prepared instruction, not
an immediate or alignment padding. Returning from the VM requires the expected
program-stack position and exactly one result operand.

Two initialized sentinel slots make the interpreter's cached operand reads
safe at depth zero. PUSH initializes its placeholder result; BCOM now writes
the top operand instead of the preceding slot. Existing instruction encoding,
valid call/return behavior, and the 255-operand capacity are retained.

A sandbox fault marks the VM before raising `ERR_DROP`. Module shutdown can
then re-enter the interpreter without executing the faulted VM again. Normal
recursive calls preserve the caller's interpretation state.

## Validation

`tests/run_vm_runtime_tests.sh` executes synthetic bytecode using the real
preparation and execution functions under ASan/UBSan. It covers:

- every operand-consuming opcode with insufficient stack depth;
- overflow through CONST, LOCAL, and PUSH, and the valid capacity boundary;
- invalid entry stacks, negative/unaligned/oversized ENTER and LEAVE frames;
- negative/out-of-range JUMP and out-of-range CALL indices;
- forged returns into operands or outside code, premature VM exit, malformed
  syscall returns, falling off code, and entering padding;
- valid direct calls, jumps, integer results, PUSH, and unary complement;
- valid recursive syscall entry and restoration of interpreter state;
- controlled `ERR_DROP` and safe faulted re-entry before the error longjmp.

The same harness linked against the previous interpreter fails immediately
with an ASan stack-buffer-underflow in `VM_CallInterpreted`. The new harness,
existing bytecode/loading/q_shared sanitizer runners, and all eight Python
checks pass. Runtime checks are included in Portable CI. The 64-bit host still
reports legacy pointer-width warnings in the block-copy implementation, which
is part of the next memory-access step.

Both Retro68 products build without compiler warning/error diagnostics and
pass PEF validation, using the temporary libraries described in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,777 | `b9ae994dd103fd98759668b3dcd6f7feab4db30c67afbcad46f07009437939c1` |
| Quake3_TeamArena | 3,822,351 | `36c01accef99cad563f36c57e24e9fa4d97e4d561875ecb3c2536045e78a21c2` |

## Remaining acceptance

The [next step](qvm-memory-validation.md) covers data-image accesses, block
copies, and syscall argument snapshots. Keep #35 open: arithmetic edge cases,
syscall pointer/range checks, and VM call argument marshalling still need work. The host harness calls the interpreter with an explicit argument array;
it does not validate `VM_Call`'s legacy varargs handling. No full sandbox or
retail/Mac OS 9 runtime compatibility claim is made. Target acceptance remains
deferred to the user's live-testing session.
