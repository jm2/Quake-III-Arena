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

The script configures `build_mac/` and builds. As Retro68's `add_application`
does, CMake links `Quake3.xcoff`, converts it with MakePEF to `Quake3.pef`, and
runs Rez to combine that PEF (the data fork) with `code/mac/mac_resources.r`
and `code/mac/quake3_icons.r` into an `APPL`/`IDQ3` application. The script
rejects a PEF without the `Joy!peff` magic, `pwpc` architecture, and a
plausible size. No game data is needed.

Every build writes the same launchable application in host-independent
containers:

| Output | Contents |
| --- | --- |
| `build_mac/Quake3.bin` | MacBinary; decode on the Mac or copy into an emulator |
| `build_mac/Quake3.dsk` | HFS disk image holding the application; mount it in an emulator |
| `build_mac/Quake3.ad` + `build_mac/%Quake3.ad` | AppleDouble data fork + resource fork/Finder info pair, as `genisoimage -hfs -double` reads it |

A Team Arena build adds the same set for `Quake3_TeamArena`. Rez cannot set
Finder flags, so `mac_app.py` sets kHasBundle (without it the Finder ignores
the `BNDL`) and fails the build unless every container is `APPL`/`IDQ3`,
carries the PEF byte for byte as its data fork, has non-empty `cfrg`, `SIZE`,
`BNDL`, `FREF`, `ICN#`, `ics#`, `icl8` and `ics8` resources, a `cfrg` (0)
naming a PowerPC application in the data fork, and a `BNDL` signed `IDQ3`.
Re-check a build with:

```sh
python3 mac_app.py verify --pef build_mac/Quake3.pef \
    build_mac/Quake3.bin build_mac/Quake3.dsk build_mac/%Quake3.ad
```

Building needs Retro68 (compiler, MakePEF, Rez and its `RIncludes`), CMake
3.12 or newer, and Python 3.

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

Packaging only adds game data: it stages the applications the build already
produced instead of recompiling resources, so it no longer depends on how the
host's Rez stores resource forks. With `genisoimage` or `mkisofs` it copies
each `Name.ad`/`%Name.ad` pair into place for `-hfs -double`, then re-verifies
every application inside the finished HFS images. With the native macOS
`hdiutil` path it copies the data fork and writes the resource fork and Finder
info exported from `Name.bin` (`mac_app.py resource-fork` / `finder-info`);
that path has not yet been run on a Mac host since this change. Packaging
requires an HFS-capable `genisoimage`, `mkisofs`, or `hdiutil`. Missing image
tools are fatal: an ordinary ZIP does not preserve a usable Classic Mac
resource fork.

Asset discovery is still provisional: it selects the first matching retail
tree and remote update inputs are not pinned by digest. Do not publish an
artifact until [#22](https://github.com/jm2/Quake-III-Arena/issues/22) and
[#23](https://github.com/jm2/Quake-III-Arena/issues/23) are resolved and the
mounted image boots on Mac OS 9.

## Current validation level

Cross-compilation, structural PEF validation, and the container checks above
pass for both products. Sound defaults off,
Team Arena runtime menus are incomplete, and renderer/input/network failure
paths remain open. See [review-findings.md](review-findings.md) and
[task.md](task.md) before debugging or releasing.
