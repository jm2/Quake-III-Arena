#!/usr/bin/env bash
# Issues #2 and #27: build_mac.sh and build_mac.ps1 choose their targets from
# the --base-only/--team-arena flag, never from what an earlier run left in
# build_mac.
#
#   - The flag (default --base-only) always reaches CMake as
#     -DBUILD_TEAM_ARENA=OFF/ON, so alternating the flags over a reused
#     build directory gives the requested targets, with and without package
#     mode, and overrides a value configured by hand in CMakeCache.txt (#27).
#   - With Team Arena disabled, a stale Quake3_TeamArena.* in build_mac is
#     neither validated, staged into the install image nor copied to
#     release_mac as a bare application, and no missionpack data is packaged;
#     with it enabled both applications are processed (#2).
#
# Each case runs the scripts in a scratch copy of the repository whose
# toolchain, check_retro68 and game data are fixtures (see make_game_data).
# Stub cmake, make, python3/python (mac_app.py and ndif_image.py),
# genisoimage and uname on PATH record their arguments. The stub cmake keeps
# BUILD_TEAM_ARENA in CMakeCache.txt across runs, as CMake does, when no
# -DBUILD_TEAM_ARENA is given; the stub make writes the PEF (at least 1 MiB),
# containers and manifest of each application the cache enables, and leaves
# any other file alone, as make does. Nothing real is built, downloaded or
# packaged.
#
# The PowerShell cases run when pwsh is installed (it is in CI; see
# tests/run_retro68_readiness_tests.sh), and are reported as SKIP otherwise.
#
# Q3_SCRIPTS_UNDER_TEST (default: the repository root) names the directory
# whose build_mac.sh and build_mac.ps1 are tested, so a modified copy can be
# checked against these cases.
set -uo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
export LC_ALL=C
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_SCRIPTS="${Q3_SCRIPTS_UNDER_TEST:-$Q3_TEST_ROOT}"
Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-build-mac-flags.XXXXXX")"
trap 'chmod -R u+w "$Q3_TEST_WORK" 2> /dev/null; rm -rf -- "$Q3_TEST_WORK"' EXIT

Q3_OUT="$Q3_TEST_WORK/output.log"
Q3_STATUS=0
Q3_FAILED=0

pass() { echo "PASS: $1"; }
fail() {
    echo "FAIL: $1"
    [ -z "${2:-}" ] || echo "  $2"
    echo "  output:"
    sed 's/^/    /' "$Q3_OUT"
    Q3_FAILED=1
}

# write_stubs ROOT: the stub commands in ROOT/stubs, logging to ROOT/logs.
write_stubs() {
    local root="$1"
    mkdir -p "$root/stubs"
    {
        printf '#!/bin/bash\nroot=%q\n' "$root"
        cat <<'STUB'
echo "cmake $*" >> "$root/logs/cmake.log"
case " $* " in *" --build "*) exec "$root/stubs/make" ;; esac
ta=""
for arg; do case "$arg" in -DBUILD_TEAM_ARENA=*) ta="${arg#*=}" ;; esac; done
# Like CMake, keep the cached value when the option is not given again.
[ -n "$ta" ] || ta=$(sed -n 's/^BUILD_TEAM_ARENA:BOOL=//p' CMakeCache.txt 2> /dev/null)
[ -n "$ta" ] || ta=OFF
echo "BUILD_TEAM_ARENA:BOOL=$ta" > CMakeCache.txt
STUB
    } > "$root/stubs/cmake"
    {
        printf '#!/bin/bash\nroot=%q\n' "$root"
        cat <<'STUB'
apps=Quake3
grep -q '^BUILD_TEAM_ARENA:BOOL=ON$' CMakeCache.txt && apps="Quake3 Quake3_TeamArena"
echo "make built: $apps" >> "$root/logs/make.log"
for app in $apps; do
    { printf 'Joy!peffpwpc'; head -c 1048576 /dev/zero; } > "$app.pef"
    for container in "$app.bin" "$app.ad" "%$app.ad"; do
        echo "$app $container" > "$container"
    done
    if command -v sha256sum > /dev/null 2>&1; then sha=$(sha256sum < "$app.pef")
    else sha=$(shasum -a 256 < "$app.pef"); fi
    printf 'pef=%s\npef_sha256=%s\n' "$app.pef" "${sha%% *}" > "$app.manifest.txt"
done
STUB
    } > "$root/stubs/make"
    # mac_app.py verify passes; ndif_image.py IMAGE OUT NAME writes OUT (more
    # than the 128-byte MacBinary header); pak_identity.py verify GAME DIR
    # passes unless a pak in DIR reads "bad" (the fixture paks are not id's,
    # so the real check is tests/test_packaging_tools.py's). Anything else
    # fails.
    {
        printf '#!/bin/bash\nroot=%q\n' "$root"
        cat <<'STUB'
echo "python $*" >> "$root/logs/python.log"
case "$1" in
    *mac_app.py) case "$2" in verify|set-dates) exit 0 ;; esac ;;
    *ndif_image.py) { head -c 128 /dev/zero; cat "$2"; } > "$3" && exit 0 ;;
    *pak_identity.py)
        [ "$2" = verify ] || exit 1
        ls "$4" | sed "s|^|$4/|" >> "$root/logs/verified.log"
        if grep -l -x -F bad "$4"/pak*.pk3 2> /dev/null | grep . > /dev/null; then
            echo "pak_identity.py: $4: not id's retail $3 paks" >&2
            exit 1
        fi
        exit 0 ;;
esac
echo "stub python: unexpected python $*" >&2
exit 1
STUB
    } > "$root/stubs/python3"
    cp "$root/stubs/python3" "$root/stubs/python"
    # genisoimage ... -o IMAGE -V NAME DIR: logs every file staged in DIR.
    {
        printf '#!/bin/bash\nroot=%q\n' "$root"
        cat <<'STUB'
out="" volume="" prev=""
for arg; do
    case "$prev" in -o) out="$arg" ;; -V) volume="$arg" ;; esac
    prev="$arg"
done
dir="${!#}"
(cd "$dir" && find . -type f | sed "s|^\./|$volume: |") >> "$root/logs/staged.log"
echo "image of $dir" > "$out"
STUB
    } > "$root/stubs/genisoimage"
    # Take the genisoimage branch on any host (hdiutil is used on Darwin).
    printf '#!/bin/sh\necho Linux\n' > "$root/stubs/uname"
    # The fixture data has the point release paks, so nothing may download:
    # a download would reach the network, and only warn if it failed.
    for downloader in curl wget; do
        printf '#!/bin/sh\necho "%s $*" >> %q\nexit 7\n' "$downloader" "$root/logs/download.log" \
            > "$root/stubs/$downloader"
    done
    chmod +x "$root/stubs/"*
}

# make_build_root ROOT: a scratch repository with the scripts under test, a
# toolchain and prepared OpenGL files that pass build_mac's checks, a passing
# check_retro68 and retail-like game data for both games.
make_build_root() {
    local root="$1" sdk="$1/tools/Retro68-src/InterfacesAndLibraries/SharedLibraries"
    rm -rf "$root"
    mkdir -p "$root/logs" "$root/tools/Retro68-build/bin" \
        "$root/tools/Retro68-build/powerpc-apple-macos/include" "$sdk"
    cp "$Q3_SCRIPTS/build_mac.sh" "$Q3_SCRIPTS/build_mac.ps1" "$root/"
    for compiler in powerpc-apple-macos-gcc powerpc-apple-macos-gcc.exe; do
        printf '#!/bin/sh\nexit 0\n' > "$root/tools/Retro68-build/bin/$compiler"
        chmod +x "$root/tools/Retro68-build/bin/$compiler"
    done
    : > "$root/tools/Retro68-build/powerpc-apple-macos/include/gl.h"
    : > "$root/tools/Retro68-build/powerpc-apple-macos/include/agl.h"
    echo "!<arch>" > "$sdk/libOpenGLLibraryStub.a"
    # The toolchain check is covered by run_retro68_readiness_tests.sh.
    printf '#!/bin/bash\nexit 0\n' > "$root/check_retro68.sh"
    printf 'exit 0\n' > "$root/check_retro68.ps1"
    # The toolchain is complete, so setup must never run.
    printf '#!/bin/bash\necho "setup ran" >> %q\nexit 1\n' "$root/logs/setup.log" \
        > "$root/setup_retro68.sh"
    printf 'Add-Content -LiteralPath %s -Value "setup ran"\nexit 1\n' "'$root/logs/setup.log'" \
        > "$root/setup_retro68.ps1"
    chmod +x "$root/setup_retro68.sh"
    make_game_data "$root"
    write_stubs "$root"
}

# make_game_data ROOT: the retail data package mode looks for, one copy of
# each game's paks under ROOT/gamedata: baseq3 pak0-pak8 and missionpack
# pak0-pak3, so the point release is there and nothing is downloaded.
# Change this (and only this) if pak selection changes.
make_game_data() {
    local root="$1" pak
    mkdir -p "$root/gamedata/baseq3" "$root/gamedata/missionpack"
    for pak in 0 1 2 3 4 5 6 7 8; do
        echo "base pak$pak" > "$root/gamedata/baseq3/pak$pak.pk3"
    done
    for pak in 0 1 2 3; do
        echo "missionpack pak$pak" > "$root/gamedata/missionpack/pak$pak.pk3"
    done
}

# run_build SCRIPT ROOT [ARG...]: SCRIPT (sh or ps1) from ROOT with the stubs
# first on PATH, after clearing ROOT/logs. build_mac/ is kept.
run_build() {
    local script="$1" root="$2"
    shift 2
    rm -rf "$root/logs" "$root/release_mac/Quake3_Install.img.bin"
    mkdir -p "$root/logs"
    Q3_STATUS=0
    Q3_LAST_ROOT="$root"
    if [ "$script" = sh ]; then
        (cd "$root" && PATH="$root/stubs:$PATH" bash ./build_mac.sh "$@") \
            > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    else
        # pwsh downloads with Invoke-WebRequest, which PATH cannot stub: send
        # it to a closed port. Its cache and telemetry stay in the scratch.
        mkdir -p "$root/pwsh-home"
        (cd "$root" && PATH="$root/stubs:$PATH" HOME="$root/pwsh-home" \
            XDG_CACHE_HOME="$root/pwsh-home/.cache" XDG_CONFIG_HOME="$root/pwsh-home/.config" \
            XDG_DATA_HOME="$root/pwsh-home/.local/share" \
            POWERSHELL_TELEMETRY_OPTOUT=1 POWERSHELL_UPDATECHECK=Off \
            http_proxy=http://127.0.0.1:9 https_proxy=http://127.0.0.1:9 \
            HTTP_PROXY=http://127.0.0.1:9 HTTPS_PROXY=http://127.0.0.1:9 no_proxy= NO_PROXY= \
            pwsh -NoProfile -NonInteractive -File ./build_mac.ps1 "$@") \
            > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
    fi
    # Both scripts announce a download first; curl and wget also log it.
    if [ -s "$root/logs/download.log" ] || grep -q -F "Downloading Updates" "$Q3_OUT"; then
        echo "download attempted: $(cat "$root/logs/download.log" 2> /dev/null)" >> "$Q3_OUT"
        Q3_DOWNLOADED=1
    else
        Q3_DOWNLOADED=0
    fi
}

# expect_targets NAME ROOT TA PACKAGE: the last run succeeded, gave CMake
# -DBUILD_TEAM_ARENA=TA, and validated (and with PACKAGE=1 staged) Quake3,
# plus Quake3_TeamArena and the missionpack data exactly when TA is ON.
expect_targets() {
    local name="$1" root="$2" ta="$3" package="$4" logs="$2/logs" problems=""
    local configure

    [ "$Q3_STATUS" -eq 0 ] || problems="$problems; exit status $Q3_STATUS"
    [ "$Q3_DOWNLOADED" -eq 0 ] || problems="$problems; a download was attempted"
    configure=$(grep -v -e ' --build ' "$logs/cmake.log" 2> /dev/null | tail -n 1)
    case " $configure " in
        *" -DBUILD_TEAM_ARENA=$ta "*) ;;
        *) problems="$problems; CMake was configured with '${configure:-nothing}', not -DBUILD_TEAM_ARENA=$ta" ;;
    esac
    [ ! -e "$logs/setup.log" ] || problems="$problems; setup_retro68 ran"
    grep -q -F "PEF validation OK: Quake3.pef" "$Q3_OUT" ||
        problems="$problems; Quake3.pef was not validated"
    grep -q -F "mac_app.py verify --pef Quake3.pef Quake3.bin %Quake3.ad" \
        "$logs/python.log" 2> /dev/null || problems="$problems; Quake3 was not verified"
    if [ "$ta" = ON ]; then
        grep -q -F "PEF validation OK: Quake3_TeamArena.pef" "$Q3_OUT" ||
            problems="$problems; Quake3_TeamArena.pef was not validated"
        grep -q -F "mac_app.py verify --pef Quake3_TeamArena.pef Quake3_TeamArena.bin" \
            "$logs/python.log" 2> /dev/null || problems="$problems; Quake3_TeamArena was not verified"
    else
        if grep -q -F "Quake3_TeamArena" "$Q3_OUT" "$logs/python.log" 2> /dev/null; then
            problems="$problems; a disabled Quake3_TeamArena was validated or reported"
        fi
    fi
    if [ "$package" -eq 1 ]; then
        # The install image (the one genisoimage run) and the bare
        # applications copied beside it.
        [ "$(grep -c -F -x "Quake 3 Arena: Quake 3 Arena/Quake3" "$logs/staged.log" 2> /dev/null)" = 1 ] ||
            problems="$problems; Quake3 is not in the install image"
        grep -q -x -F "Quake 3 Arena: Quake 3 Arena/baseq3/pak0.pk3" "$logs/staged.log" 2> /dev/null ||
            problems="$problems; baseq3 data is not in the install image"
        grep -q -x -F "python mac_app.py set-dates release_mac/content" "$logs/python.log" 2> /dev/null &&
            grep -q -x -F "python mac_app.py set-dates release_mac/Quake3_Install.img" "$logs/python.log" 2> /dev/null ||
            problems="$problems; the staged content and the image were not given SOURCE_DATE_EPOCH dates (#405)"
        grep -q -F "ndif_image.py release_mac/Quake3_Install.img release_mac/Quake3_Install.img.bin" \
            "$logs/python.log" 2> /dev/null || problems="$problems; ndif_image.py did not encode the install image"
        [ -s "$root/release_mac/Quake3_Install.img.bin" ] ||
            problems="$problems; no install image was written"
        [ -s "$root/release_mac/Quake3.bin" ] && [ -s "$root/release_mac/Quake3.manifest.txt" ] ||
            problems="$problems; the bare Quake3 application was not copied to release_mac"
        grep -q -x -F "python pak_identity.py verify baseq3 release_mac/content/Quake 3 Arena/baseq3" \
            "$logs/python.log" 2> /dev/null &&
            grep -q -F "baseq3/pak8.pk3" "$logs/verified.log" 2> /dev/null ||
            problems="$problems; the staged baseq3 paks were not checked (#22)"
        if [ "$ta" = ON ]; then
            grep -q -F "pak_identity.py verify missionpack " "$logs/python.log" 2> /dev/null ||
                problems="$problems; the missionpack paks were not checked (#22)"
        elif grep -q -F "pak_identity.py verify missionpack" "$logs/python.log" 2> /dev/null; then
            problems="$problems; missionpack paks were checked with Team Arena off"
        fi
        if [ "$ta" = ON ]; then
            grep -q -x -F "Quake 3 Arena: Quake 3 Arena/Quake3_TeamArena" "$logs/staged.log" ||
                problems="$problems; Quake3_TeamArena is not in the install image"
            grep -q -x -F "Quake 3 Arena: Quake 3 Arena/missionpack/pak0.pk3" "$logs/staged.log" ||
                problems="$problems; missionpack data is not in the install image"
            grep -q -F -- "--hfs-name Quake3_TeamArena" "$logs/python.log" ||
                problems="$problems; Quake3_TeamArena was not checked in the image"
            cmp -s "$root/build_mac/Quake3_TeamArena.bin" "$root/release_mac/Quake3_TeamArena.bin" ||
                problems="$problems; the bare Quake3_TeamArena application was not copied to release_mac"
        else
            if grep -q -e "TeamArena" -e "missionpack" "$logs/staged.log" 2> /dev/null; then
                problems="$problems; Team Arena files were staged:$(grep -e TeamArena -e missionpack \
                    "$logs/staged.log" | sed 's/^/ [/; s/$/]/' | tr -d '\n')"
            fi
            if ls "$root/release_mac" | grep TeamArena > /dev/null; then
                problems="$problems; release_mac holds Team Arena files: $(ls "$root/release_mac" | grep TeamArena | tr '\n' ' ')"
            fi
        fi
    fi
    if [ -z "$problems" ]; then
        pass "$name"
    else
        fail "$name" "${problems#; }"
    fi
}

# expect_failure NAME TEXT: the last run failed, saying TEXT (any case),
# and wrote no install image.
expect_failure() {
    local name="$1" text="$2" problems=""
    [ "$Q3_STATUS" -ne 0 ] || problems="$problems; it succeeded"
    grep -q -i -F -- "$text" "$Q3_OUT" || problems="$problems; it did not say '$text'"
    [ ! -e "$Q3_LAST_ROOT/release_mac/Quake3_Install.img.bin" ] ||
        problems="$problems; an install image was written"
    if [ -z "$problems" ]; then
        pass "$name"
    else
        fail "$name" "${problems#; }"
    fi
}

# plant_stale_team_arena ROOT corrupt|valid: Quake3_TeamArena.* left in
# ROOT/build_mac by an earlier build. A corrupt one fails PEF validation; a
# valid-looking one would pass every check and could be packaged.
plant_stale_team_arena() {
    local dir="$1/build_mac" container
    mkdir -p "$dir"
    if [ "$2" = corrupt ]; then
        echo "stale XCOFF, not a PEF" > "$dir/Quake3_TeamArena.pef"
    else
        { printf 'Joy!peffpwpc'; head -c 1048576 /dev/zero; echo stale; } > "$dir/Quake3_TeamArena.pef"
    fi
    for container in bin ad; do
        echo "stale" > "$dir/Quake3_TeamArena.$container"
    done
    echo "stale" > "$dir/%Quake3_TeamArena.ad"
    echo "pef_sha256=0000" > "$dir/Quake3_TeamArena.manifest.txt"
}

# run_cases SCRIPT LABEL: every case for build_mac.SCRIPT.
run_cases() {
    local script="$1" label="$2" root="$Q3_TEST_WORK/root-$1" package pkg_args pkg_name
    local flag ta step

    # --- #2: a stale Team Arena binary in an OFF build directory ---
    # A directory last built with --team-arena (cache ON, both applications
    # present), whose Team Arena PEF is now corrupt.
    make_build_root "$root"
    run_build "$script" "$root" --team-arena
    expect_targets "$label --team-arena builds and validates both applications" "$root" ON 0
    plant_stale_team_arena "$root" corrupt
    run_build "$script" "$root" --base-only
    expect_targets "$label --base-only ignores a corrupt stale Quake3_TeamArena.pef left by a --team-arena build" \
        "$root" OFF 0
    # The same with no flag (the default is --base-only).
    run_build "$script" "$root"
    expect_targets "$label with no flag ignores a corrupt stale Quake3_TeamArena.pef" "$root" OFF 0

    # A valid-looking stale Team Arena must not be packaged.
    make_build_root "$root"
    run_build "$script" "$root" package --team-arena
    expect_targets "$label package --team-arena stages both applications and missionpack" "$root" ON 1
    plant_stale_team_arena "$root" valid
    run_build "$script" "$root" package --base-only
    expect_targets "$label package --base-only leaves a stale Quake3_TeamArena and missionpack out of the package" \
        "$root" OFF 1

    # Stale files planted in a directory whose cache already says OFF.
    make_build_root "$root"
    run_build "$script" "$root" --base-only
    plant_stale_team_arena "$root" corrupt
    run_build "$script" "$root" package
    expect_targets "$label package ignores Quake3_TeamArena.* planted in an OFF build directory" "$root" OFF 1

    # --- #27: the flag always reaches CMake ---
    for package in 0 1; do
        pkg_args=()
        pkg_name=""
        if [ "$package" -eq 1 ]; then
            pkg_args=(package)
            pkg_name="package "
        fi
        # A clean build directory for each flag.
        for flag in --team-arena --base-only ""; do
            case "$flag" in --team-arena) ta=ON ;; *) ta=OFF ;; esac
            make_build_root "$root"
            run_build "$script" "$root" ${pkg_args[@]+"${pkg_args[@]}"} ${flag:+"$flag"}
            expect_targets "$label ${pkg_name}${flag:-(no flag)} in a clean build directory gives BUILD_TEAM_ARENA=$ta" \
                "$root" "$ta" "$package"
        done
        # A build directory configured by hand (cmake -DBUILD_TEAM_ARENA=...)
        # with the opposite value: the flag wins over the cache.
        for flag in --base-only --team-arena; do
            case "$flag" in --team-arena) ta=ON ;; *) ta=OFF ;; esac
            make_build_root "$root"
            mkdir -p "$root/build_mac"
            echo "BUILD_TEAM_ARENA:BOOL=$([ "$ta" = ON ] && echo OFF || echo ON)" \
                > "$root/build_mac/CMakeCache.txt"
            run_build "$script" "$root" ${pkg_args[@]+"${pkg_args[@]}"} "$flag"
            expect_targets "$label ${pkg_name}$flag overrides a cached BUILD_TEAM_ARENA=$([ "$ta" = ON ] && echo OFF || echo ON)" \
                "$root" "$ta" "$package"
        done
        # One build directory reused while the flags alternate.
        make_build_root "$root"
        step=0
        for flag in --team-arena --base-only --team-arena "" --team-arena --base-only; do
            step=$((step + 1))
            case "$flag" in --team-arena) ta=ON ;; *) ta=OFF ;; esac
            run_build "$script" "$root" ${flag:+"$flag"} ${pkg_args[@]+"${pkg_args[@]}"}
            expect_targets "$label ${pkg_name}reused build directory, run $step: ${flag:-(no flag)} gives BUILD_TEAM_ARENA=$ta" \
                "$root" "$ta" "$package"
        done
    done

    # --- #22: the data root, offline mode and the pak check ---
    # --data picks one install out of two that would otherwise be refused.
    make_build_root "$root"
    mkdir -p "$root/gamedata-demo/baseq3" "$Q3_TEST_WORK/alt-$script/baseq3"
    echo "other pak0" > "$root/gamedata-demo/baseq3/pak0.pk3"
    for pak in 0 1 2 3 4 5 6 7 8; do
        echo "alt base pak$pak" > "$Q3_TEST_WORK/alt-$script/baseq3/pak$pak.pk3"
    done
    ln -sfn "$Q3_TEST_WORK/alt-$script" "$Q3_TEST_WORK/link-$script"
    run_build "$script" "$root" package
    expect_failure "$label package refuses two candidate retail installs" "more than one"
    run_build "$script" "$root" package "--data=../alt-$script"
    expect_targets "$label package --data=DIR (relative) packages that install alone" "$root" OFF 1
    if ! grep -q -x -F "alt base pak0" "$root/release_mac/content/Quake 3 Arena/baseq3/pak0.pk3" 2> /dev/null; then
        fail "$label package --data=DIR staged the paks from DIR" "pak0 is not the --data install's"
    else
        pass "$label package --data=DIR staged the paks from DIR"
    fi
    run_build "$script" "$root" package "--data=$Q3_TEST_WORK/link-$script"
    expect_targets "$label package --data=SYMLINK packages the install it points to" "$root" OFF 1
    for missing in "$Q3_TEST_WORK/missing-$script" ""; do
        run_build "$script" "$root" package "--data=$missing"
        if [ "$Q3_STATUS" -ne 0 ] && [ ! -e "$root/logs/cmake.log" ] &&
           grep -q -F -- "--data directory" "$Q3_OUT"; then
            pass "$label rejects --data='${missing:+missing}' before CMake"
        else
            fail "$label rejects --data='${missing:+missing}' before CMake" "exit status $Q3_STATUS"
        fi
    done

    # An install updated only to an earlier point release (no pak7, pak8)
    # tries the download, which the stubs refuse.
    make_build_root "$root"
    rm -f "$root/gamedata/baseq3/pak7.pk3" "$root/gamedata/baseq3/pak8.pk3"
    run_build "$script" "$root" package
    expect_failure "$label package downloads the point release for an install without pak7 and pak8" \
        "could not be downloaded"
    [ "$Q3_DOWNLOADED" -eq 1 ] || fail "$label package tries the download without pak7 and pak8" "no download"
    make_build_root "$root"
    rm -f "$root/gamedata/missionpack/pak3.pk3"
    run_build "$script" "$root" package --team-arena
    expect_failure "$label package --team-arena downloads the point release without missionpack pak3" \
        "could not be downloaded"

    # Without the point release paks: --offline stops; online, the download
    # is tried (the stubs refuse it) and its failure stops packaging too.
    make_build_root "$root"
    rm -f "$root/gamedata/baseq3/pak1.pk3"
    run_build "$script" "$root" package --offline
    expect_failure "$label package --offline stops when the point release paks are missing" "forbids downloading"
    [ "$Q3_DOWNLOADED" -eq 0 ] || fail "$label package --offline downloads nothing" "a download was attempted"
    run_build "$script" "$root" package
    expect_failure "$label package stops when the point release download fails" "could not be downloaded"
    [ "$Q3_DOWNLOADED" -eq 1 ] || fail "$label package tries to download the point release paks" "no download"

    # A pak that is not id's stops packaging before the image is made.
    make_build_root "$root"
    echo bad > "$root/gamedata/baseq3/pak1.pk3"
    run_build "$script" "$root" package
    expect_failure "$label package refuses a baseq3 pak that is not id's" "not id's"
    make_build_root "$root"
    echo bad > "$root/gamedata/missionpack/pak1.pk3"
    run_build "$script" "$root" package --team-arena
    expect_failure "$label package --team-arena refuses a missionpack pak that is not id's" "not id's"

    # Contradictory flags stop before anything is configured.
    make_build_root "$root"
    run_build "$script" "$root" --base-only --team-arena
    if [ "$Q3_STATUS" -ne 0 ] && [ ! -e "$root/logs/cmake.log" ] &&
       grep -q -F "exactly one of --base-only or --team-arena" "$Q3_OUT"; then
        pass "$label rejects --base-only with --team-arena before CMake"
    else
        fail "$label rejects --base-only with --team-arena before CMake" "exit status $Q3_STATUS"
    fi
}

run_cases sh "build_mac.sh"
if command -v pwsh > /dev/null 2>&1; then
    run_cases ps1 "build_mac.ps1"
else
    echo "SKIP: pwsh is not installed; build_mac.ps1 not exercised"
fi

if [ "$Q3_FAILED" -ne 0 ]; then
    echo "build_mac flag tests FAILED"
    exit 1
fi
echo "All build_mac flag tests passed."
