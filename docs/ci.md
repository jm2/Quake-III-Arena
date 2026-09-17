# Continuous integration

`Portable CI` is the first automated signal layer for review and GasCity
workers. It runs on master pushes, pull requests, and manual dispatch without a
Retro68 installation, proprietary retail data, or a Mac OS 9 emulator.

Feature branch updates run through the pull-request trigger. Filtering the
push trigger to master avoids duplicate copies of the same four jobs for each
PR head while retaining checks on the merged default branch.

## Required checks

- `Scripts and review ledger`
  - parses Bash and PowerShell entry points;
  - runs both build-script help paths;
  - compiles the Python utilities;
  - requires `docs/task.md` to remain in P0-to-P3 order with exactly one direct
    link for every confirmed issue #1 through #52, allowing both open and
    completed checkboxes as the queue is worked down.
- `Packaging tools (Python 3.11)` and `(Python 3.14)`
  - validate AppleDouble entry offsets, resource data, Finder type/creator,
    and bundle flag;
  - validate MacBinary name bytes, fork lengths/padding, deterministic Mac
    dates, version fields, and CRC;
  - reject filenames that cannot be represented in MacRoman.
- `Host C regressions (ASan/UBSan)`
  - builds a deliberately isolated portion of `q_shared.c`;
  - checks bounded extension stripping, formatting, and token termination
    under AddressSanitizer and UndefinedBehaviorSanitizer;
  - exercises the real QVM create/restart loaders with exact-sized files,
    every truncation of a small image, invalid signed ranges/counts, data-size
    overflow, larger/smaller replacement data images, and same-sized changes
    to code, counts, layout, BSS, or initial data (issue #35);
  - checks recoverable rejection, file-buffer ownership, no hunk allocation
    before validation, unchanged data on rejected restart, and valid data
    initialization/restart, plus cleanup after failed bytecode preparation;
  - exercises actual bytecode preparation with truncated operands, invalid
    opcodes/counts/branch targets, unaligned immediates, and valid translation.
  - executes synthetic QVMs through the actual interpreter to check operand
    and program stacks, CALL/JUMP targets, forged return addresses, recursive
    syscalls, faulted shutdown re-entry, and valid execution;
  - checks full-width loads, legacy masked stores, ARG boundaries, block-copy
    bounds/overlap, and bounded syscall argument snapshots;
  - checks wrapping integer arithmetic, division/modulo traps, shift counts,
    and float conversion limits. Syscall-specific pointer/range checks and
    VM call marshalling remain incomplete.

GitHub Actions dependencies are pinned to exact release commits, and
Dependabot is configured to propose GitHub Actions updates.

## Run the portable checks locally

```sh
export TMPDIR="${TMPDIR:-/var/tmp}"
bash -n build_mac.sh setup_retro68.sh tests/run_host_c_tests.sh
./build_mac.sh --help
python3 -m py_compile create_appledouble.py generate_icon_r.py macbinary_encode.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash tests/run_host_c_tests.sh
bash tests/run_vm_loading_tests.sh
bash tests/run_vm_bytecode_tests.sh
bash tests/run_vm_runtime_tests.sh
pwsh -NoProfile -File ./build_mac.ps1 --help
```

PowerShell parser validation is also part of CI; see
`.github/workflows/portable-ci.yml` for the exact command.

## What this CI does not prove

Portable CI does not compile a PowerPC PEF, preserve/inspect a Classic resource
fork inside a mounted HFS image, use retail PK3s, or exercise AGL,
DrawSprocket, InputSprocket, Sound Manager, Open Transport, Finder events, or
Mac OS 9 runtime behavior. Passing these starter checks alone does not close
security or target-runtime issues; each issue needs its specified regressions
and applicable target evidence.

For every new PR, require successful CI and a clean Codex review on the current
head, plus resolution of every CodeRabbit finding. The user confirmed that
CodeRabbit rate limits need not block a merge once this gate is satisfied.
Resolve findings, push fixes, and obtain successful checks and renewed Codex
review. Absent, pending, or failed Codex review is not a clean review.

The user has deferred retail-content and Mac OS 9 live testing to a follow-up
session. Host-tested fixes may merge with those limitations recorded; do not
mark the outstanding target acceptance checks complete.
Host-tool-only changes do not require unrelated engine or target tests unless
their issue's acceptance criteria specify them.

The next CI layers should be:

1. a legally provisioned/self-hosted Retro68 runner that builds base and Team
   Arena and validates `Joy!peff` / `pwpc`;
2. hostile-input ASan/UBSan harnesses for the P0 parser/protocol issues;
3. mounted HFS resource/Finder inspection;
4. emulator smoke tests using externally provisioned legal game data.

When adding a regression for a GitHub issue, name the issue in the test and
update its nested checkbox in [task.md](task.md); leave the issue-level
checkbox open until all required target evidence exists.
