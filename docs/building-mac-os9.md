# Building the Mac OS 9 port

This port uses Retro68 to build one monolithic PowerPC Classic Mac OS
application. The base game is the default; Team Arena is an explicit option.

## Linux/macOS host

From the repository root:

```sh
./setup_retro68.sh
./build_mac.sh --base-only
```

For Team Arena:

```sh
./build_mac.sh --team-arena
```

The script configures `build_mac/`, builds, converts the linked XCOFF image
with MakePEF, and rejects output unless it has the `Joy!peff` magic, `pwpc`
architecture, and a plausible size. Do not invoke MakePEF a second time on an
already converted PEF.

## Windows host

In PowerShell:

```powershell
.\setup_retro68.ps1
.\build_mac.ps1 --base-only
```

Use `--team-arena` for the expansion. The scripts parse successfully under
PowerShell, but this review has not completed a native Windows toolchain/build
run; that remains required.

## Legal game data and packaging

The GPL source does not include retail Quake III Arena or Team Arena data.
Packaging requires legally obtained `baseq3\pak0.pk3`; a Team Arena package
also requires `missionpack\pak0.pk3`. The old demo fallback was removed because
it placed `demoq3` data under `baseq3` and produced an application that could
not load `default.cfg`.

With data present under the repository tree, package with:

```sh
./build_mac.sh package --base-only
```

or:

```powershell
.\build_mac.ps1 package --base-only
```

Packaging additionally requires Rez and an HFS-capable `genisoimage`,
`mkisofs`, or the supported native macOS path. Missing PEF/resource/image
tools are fatal: an ordinary ZIP does not preserve a usable Classic Mac
resource fork.

Asset discovery is still provisional: it selects the first matching retail
tree and remote update inputs are not pinned by digest. Do not publish an
artifact until [#22](https://github.com/jm2/Quake-III-Arena/issues/22) and
[#23](https://github.com/jm2/Quake-III-Arena/issues/23) are resolved and the
mounted image boots on Mac OS 9.

## Current validation level

Cross-compilation and structural PEF validation pass. Sound defaults off,
Team Arena runtime menus are incomplete, and renderer/input/network failure
paths remain open. See [review-findings.md](review-findings.md) and
[task.md](task.md) before debugging or releasing.
