# Continuous integration

`Portable CI` is the first automated signal layer for review and GasCity
workers. It runs on pushes, pull requests, and manual dispatch without a
Retro68 installation, proprietary retail data, or a Mac OS 9 emulator.

## Required checks

- `Scripts and review ledger`
  - parses Bash and PowerShell entry points;
  - runs both build-script help paths;
  - compiles the Python utilities;
  - requires `docs/task.md` to remain in P0-to-P3 order with exactly one direct
    link for every confirmed issue #1 through #52.
- `Packaging tools (Python 3.11)` and `(Python 3.14)`
  - validate AppleDouble entry offsets, resource data, Finder type/creator,
    and bundle flag;
  - validate MacBinary name bytes, fork lengths/padding, deterministic Mac
    dates, version fields, and CRC;
  - reject filenames that cannot be represented in MacRoman.
- `Host C regressions (ASan/UBSan)`
  - builds a deliberately isolated portion of `q_shared.c`;
  - checks bounded extension stripping, formatting, and token termination
    under AddressSanitizer and UndefinedBehaviorSanitizer.

GitHub Actions dependencies are pinned to exact release commits, and
Dependabot is configured to propose GitHub Actions updates.

## Run the portable checks locally

```sh
bash -n build_mac.sh setup_retro68.sh tests/run_host_c_tests.sh
./build_mac.sh --help
python3 -m py_compile create_appledouble.py generate_icon_r.py macbinary_encode.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash tests/run_host_c_tests.sh
pwsh -NoProfile -File ./build_mac.ps1 --help
```

PowerShell parser validation is also part of CI; see
`.github/workflows/portable-ci.yml` for the exact command.

## What this CI does not prove

Portable CI does not compile a PowerPC PEF, preserve/inspect a Classic resource
fork inside a mounted HFS image, use retail PK3s, or exercise AGL,
DrawSprocket, InputSprocket, Sound Manager, Open Transport, Finder events, or
Mac OS 9 runtime behavior. It must never be cited as proof that a security
issue or target-runtime issue is closed.

The next CI layers should be:

1. a legally provisioned/self-hosted Retro68 runner that builds base and Team
   Arena and validates `Joy!peff` / `pwpc`;
2. hostile-input ASan/UBSan harnesses for the P0 parser/protocol issues;
3. mounted HFS resource/Finder inspection;
4. emulator smoke tests using externally provisioned legal game data.

When adding a regression for a GitHub issue, name the issue in the test and
update its nested checkbox in [task.md](task.md); leave the issue-level
checkbox open until all required target evidence exists.
