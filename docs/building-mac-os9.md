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

## Checking and rebuilding the toolchain

Before CMake runs, `build_mac.sh` runs `check_retro68.sh` (`build_mac.ps1` runs
`check_retro68.ps1`). Finding the programs is not enough: after a host OS
upgrade `powerpc-apple-macos-gcc --version` can still work while `cc1`, `ar`,
Rez and MakeImport fail to load their shared libraries, and CMake would then
report only that the compiler identification is unknown. The check therefore
runs the toolchain in a scratch directory under `${TMPDIR:-/var/tmp}`, which
takes well under a second: `gcc --version`, `gcc -c` and `g++ -c` with the
`CMakeLists.txt` flags, `ar` and `ranlib` on the object, `ld --version`, and
MakePEF, MakeImport and Rez without input. It then links a small C file with
`-lm -lInterfaceLib`, converts it with MakePEF (the result must start with
`Joy!peff`/`pwpc`) and has Rez build an application from it. Run it by hand
with `bash check_retro68.sh [tools/Retro68-build]`.

If a step fails (exit status 1), the build stops before CMake and prints the
step and the tool's output, which names a library the host cannot load (for
example `libisl.so.23` for `cc1`). On Linux the check also lists every library
`ldd` cannot find for the toolchain's programs. Install the missing libraries
or rebuild the toolchain. Exit status 3 means the check itself could not run:
its scratch directory is missing, read-only, full or out of inodes, or a
signal from outside (the OOM killer, a timeout, Ctrl-C) stopped a step. It
says nothing about the toolchain, and the build stops with that message
instead.

`setup_retro68.sh` resumes an earlier build with `--skip-thirdparty` only if
`check_retro68.sh --tools-only` passes. If the tools cannot run, it rebuilds
everything, but it never deletes the toolchain: it first moves
`tools/Retro68-build` and `tools/Retro68-work` aside to `*.broken-<UTC time>`
(an incomplete earlier build to `*.previous-<UTC time>`) and prints how to
restore them. Moved-aside toolchains are never removed automatically; delete
them once the new toolchain works. The work tree holds only build
intermediates (several GB), so setup keeps one moved-aside copy of it and
removes the older one when it moves a newer one aside. If the check could not
run, for example because a signal from outside stopped a step or TMPDIR is out
of space or inodes, setup stops before changing anything. `setup_retro68.ps1`
does the same with `check_retro68.ps1` (it renames `tools/Retro68-build`
aside).

### Rebuilding on a host with GCC 16

`setup_retro68.sh` builds Retro68's GCC 12.2 with the host's default compiler.
With GCC 16 (Fedora 44) that fails: its C23/C++20 defaults break libcody
(`u8` literals) and `gcc/system.h` (`<memory>` against the `safe-ctype.h`
macros). Setting `CC`/`CXX` does not help, because `build-toolchain.bash`
overwrites them from its `--host-c-compiler`/`--host-cxx-compiler` options.
Building everything with Clang is not enough either: the Clang-built PowerPC
`ld` segfaults on large links such as Quake3. The toolchain in use since
2026-09-23 (Retro68 `83b9c8d2c5`, GCC 12.2.0) was rebuilt as follows, and
builds PEFs byte-identical to the previous toolchain's:

```sh
SRC=$PWD/tools/Retro68-src
PREFIX=$PWD/tools/Retro68-build.new   # must not exist yet
WORK=${TMPDIR:-/var/tmp}/retro68-rebuild-work
mkdir -p "$WORK" && cd "$WORK"

# GCC, the host tools and the target libraries, built with Clang. This stops
# at the LaunchAPPLServer sample (the Clang-built ld crashes on it), which
# Quake3 does not need.
bash "$SRC/build-toolchain.bash" --prefix="$PREFIX" --no-68k --no-carbon \
    --host-c-compiler=clang --host-cxx-compiler=clang++

# binutils again, built with host GCC in C17 mode, over the Clang-built ones.
mkdir binutils-build-ppc-gcc && cd binutils-build-ppc-gcc
CC=gcc CFLAGS='-O2 -std=gnu17' "$SRC/binutils/configure" --disable-plugins \
    --target=powerpc-apple-macos --prefix="$PREFIX" --disable-doc
make -j"$(nproc)" && make install
cd ..

# The target libraries the stopped build did not install.
cmake -P build-target-ppc/libretro/cmake_install.cmake
cmake -P build-target-ppc/Console/cmake_install.cmake
```

Then check the new toolchain with
`bash check_retro68.sh tools/Retro68-build.new`, move the old
`tools/Retro68-build` aside and rename the new one to `tools/Retro68-build`
(the install tree is relocatable). The next
`build_mac.sh` run copies the OpenGL SDK headers into its prepared include
directory. The rebuilt toolchain needs no `LD_LIBRARY_PATH`.

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
