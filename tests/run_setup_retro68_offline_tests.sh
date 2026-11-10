#!/usr/bin/env bash
# Issue #51: setup_retro68.sh needs wget only when an SDK archive must be
# downloaded, and never needs hfsutils.
#
# Every case runs setup_retro68.sh in a scratch copy of the repository with a
# controlled PATH: stub cmake, git, bison, flex, makeinfo, unar, ruby, sleep,
# g++ and uname, then links to the host's other programs except wget, wget2,
# curl and every hfsutils program (hmount, hcopy, hformat, ...). Retro68 is
# checked out at the pinned commit (stub git), its build-toolchain.bash is a
# stub that installs MakeImport and the prepared include directory, and the
# SDK archives are small fixtures whose SHA-256s replace the pinned ones in
# the scratch retro68-versions.txt. Their URLs point at downloads.invalid.
#
#   - Both archives cached (in tools/ or at the repository root): setup runs
#     to the end and says nothing about wget or hfsutils.
#   - One archive cached: setup stops before extracting anything and names
#     the missing archive, only that one, in the wget diagnostic.
#   - Neither archive cached: the same, for the first archive it needs.
#   - With a stub wget (it copies a fixture; nothing real is downloaded),
#     wget is called only for the archives that are not cached.
#
# Q3_SCRIPTS_UNDER_TEST (default: the repository root) names the directory
# whose setup_retro68.sh is tested, so a modified copy can be checked against
# these cases.
set -uo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
export LC_ALL=C
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_SCRIPTS="${Q3_SCRIPTS_UNDER_TEST:-$Q3_TEST_ROOT}"
Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-setup-offline.XXXXXX")"
trap 'chmod -R u+w "$Q3_TEST_WORK" 2> /dev/null; rm -rf -- "$Q3_TEST_WORK"' EXIT

Q3_OUT="$Q3_TEST_WORK/output.log"
Q3_STATUS=0
Q3_FAILED=0

Q3_VERSIONS="$Q3_TEST_ROOT/retro68-versions.txt"
q3_version() {
    sed -n "s/^$1=//p" "$Q3_VERSIONS"
}
Q3_PIN="$(q3_version RETRO68_COMMIT)"
Q3_PIN_SUBMODULES="$(q3_version RETRO68_SUBMODULE)"
Q3_MPW_FILE="$(q3_version MPW_FILE)"
Q3_OPENGL_FILE="$(q3_version OPENGL_FILE)"
Q3_MPW_URL="https://downloads.invalid/mpw/$Q3_MPW_FILE"
Q3_OPENGL_URL="https://downloads.invalid/opengl/$Q3_OPENGL_FILE"

q3_sha256() {
    if command -v sha256sum > /dev/null 2>&1; then
        sha256sum < "$1" | cut -d ' ' -f 1
    else
        shasum -a 256 < "$1" | cut -d ' ' -f 1
    fi
}

# Programs the controlled PATH leaves out: the downloaders and hfsutils.
Q3_LEFT_OUT="wget wget2 curl hattrib hcd hcopy hdel hdir hformat hfs hfsck hfssh
    hls hmkdir hmount hpwd hrename hrmdir humount hvol xhfs"

# A directory of links to every program on $PATH except $Q3_LEFT_OUT.
Q3_HOST_PATH="$Q3_TEST_WORK/host-path"
mkdir -p "$Q3_HOST_PATH"
while IFS= read -r -d : entry; do
    [ -d "$entry" ] || continue
    for program in "$entry"/*; do
        name="${program##*/}"
        [ -x "$program" ] && [ ! -d "$program" ] || continue
        [ -e "$Q3_HOST_PATH/$name" ] && continue
        case " $(echo $Q3_LEFT_OUT) " in *" $name "*) continue ;; esac
        ln -s "$program" "$Q3_HOST_PATH/$name"
    done
done <<< "$PATH:"

pass() { echo "PASS: $1"; }
fail() {
    echo "FAIL: $1"
    [ -z "${2:-}" ] || echo "  $2"
    echo "  output:"
    sed 's/^/    /' "$Q3_OUT"
    Q3_FAILED=1
}

# make_setup_root ROOT: the scratch repository described above, with no SDK
# archive cached yet.
make_setup_root() {
    local root="$1" tool
    rm -rf "$root"
    mkdir -p "$root/stubs" "$root/wget-stub" "$root/logs" "$root/fixtures" \
        "$root/tools/Retro68-src/.git"
    cp "$Q3_SCRIPTS/setup_retro68.sh" "$root/"
    chmod +x "$root/setup_retro68.sh"
    # No toolchain is installed, so the check must not run.
    printf '#!/bin/bash\necho "check ran" >> %q\nexit 3\n' "$root/logs/check.log" \
        > "$root/check_retro68.sh"

    echo "MPW Golden Master fixture" > "$root/fixtures/$Q3_MPW_FILE"
    echo "OpenGL SDK fixture" > "$root/fixtures/$Q3_OPENGL_FILE"
    sed -e "s|^MPW_SHA256=.*|MPW_SHA256=$(q3_sha256 "$root/fixtures/$Q3_MPW_FILE")|" \
        -e "s|^OPENGL_SHA256=.*|OPENGL_SHA256=$(q3_sha256 "$root/fixtures/$Q3_OPENGL_FILE")|" \
        -e "s|^MPW_URL=.*|MPW_URL=$Q3_MPW_URL|" \
        -e "s|^OPENGL_URL=.*|OPENGL_URL=$Q3_OPENGL_URL|" \
        "$Q3_VERSIONS" > "$root/retro68-versions.txt"

    for tool in cmake bison flex makeinfo ruby sleep; do
        printf '#!/bin/sh\nexit 0\n' > "$root/stubs/$tool"
    done
    printf '#!/bin/sh\necho Linux\n' > "$root/stubs/uname"
    printf '#!/bin/sh\ncat > /dev/null\necho 201703L\n' > "$root/stubs/g++"

    # git: Retro68 is checked out at the pinned commit and submodules.
    printf '%s\n' "$Q3_PIN_SUBMODULES" > "$root/pinned-gitlinks"
    {
        printf '#!/bin/bash\nroot=%q pin=%q\n' "$root" "$Q3_PIN"
        cat <<'STUB'
echo "git $*" >> "$root/logs/git.log"
while :; do
    case "$1" in
        -C|-c) shift 2 ;;
        *) break ;;
    esac
done
case "$1" in
    rev-parse) echo "$pin" ;;
    submodule)
        [ "$2" = status ] || exit 1
        while read -r path link; do echo " $link $path (heads/master)"; done < "$root/pinned-gitlinks"
        ;;
    *) echo "stub git: unexpected git $*" >&2; exit 1 ;;
esac
STUB
    } > "$root/stubs/git"

    # unar -q -f ARCHIVE -o DEST: the layout of each SDK.
    {
        printf '#!/bin/bash\necho "unar $*" >> %q\n' "$root/logs/unar.log"
        cat <<'STUB'
out="" prev=""
for arg; do [ "$prev" = "-o" ] && out="$arg"; prev="$arg"; done
case "$out" in
    *mpw*) mkdir -p "$out/MPW/Interfaces&Libraries/Interfaces/CIncludes"
           : > "$out/MPW/Interfaces&Libraries/Interfaces/CIncludes/Types.h" ;;
    *) mkdir -p "$out/OpenGL SDK/Libraries" "$out/OpenGL SDK/Headers"
       : > "$out/OpenGL SDK/Libraries/OpenGLLibraryStub"
       : > "$out/OpenGL SDK/Libraries/OpenGLLibraryStub.rsrc"
       echo gl > "$out/OpenGL SDK/Headers/gl.h"
       echo agl > "$out/OpenGL SDK/Headers/agl.h" ;;
esac
STUB
    } > "$root/stubs/unar"

    # build-toolchain.bash --prefix=DIR installs MakeImport and the prepared
    # include directory.
    {
        printf '#!/bin/bash\necho "build-toolchain $*" >> %q\n' "$root/logs/build-toolchain.log"
        cat <<'STUB'
prefix=""
for arg; do case "$arg" in --prefix=*) prefix="${arg#--prefix=}" ;; esac; done
[ -n "$prefix" ] || exit 1
mkdir -p "$prefix/bin" "$prefix/powerpc-apple-macos/include"
printf '#!/bin/sh\necho "import library" > "$2"\n' > "$prefix/bin/MakeImport"
chmod +x "$prefix/bin/MakeImport"
STUB
    } > "$root/tools/Retro68-src/build-toolchain.bash"

    # wget -O OUT URL: copies the fixture named like the URL. Only on PATH
    # when a case adds ROOT/wget-stub.
    {
        printf '#!/bin/bash\nroot=%q\n' "$root"
        cat <<'STUB'
out="" prev=""
for arg; do [ "$prev" = "-O" ] && out="$arg"; prev="$arg"; done
url="${!#}"
echo "wget $url" >> "$root/logs/wget.log"
cp "$root/fixtures/${url##*/}" "$out"
STUB
    } > "$root/wget-stub/wget"
    chmod +x "$root/stubs/"* "$root/wget-stub/wget" "$root/tools/Retro68-src/build-toolchain.bash"
}

# cache ROOT WHERE FILE...: copy each fixture archive to ROOT/tools (WHERE is
# tools) or to the repository root (WHERE is root).
cache() {
    local root="$1" where="$2" file dest
    shift 2
    case "$where" in tools) dest="$root/tools" ;; *) dest="$root" ;; esac
    for file; do
        cp "$root/fixtures/$file" "$dest/$file"
    done
}

# run_setup ROOT [wget]: setup_retro68.sh with the controlled PATH, and with
# the stub wget on it when asked.
run_setup() {
    local root="$1" path="$1/stubs:$Q3_HOST_PATH"
    [ "${2:-}" != wget ] || path="$1/stubs:$1/wget-stub:$Q3_HOST_PATH"
    Q3_STATUS=0
    (cd "$root" && unset CXXFLAGS && PATH="$path" bash ./setup_retro68.sh) \
        > "$Q3_OUT" 2>&1 || Q3_STATUS=$?
}

# expect_complete NAME ROOT WGET_URL...: setup ran to the end, without a
# word about wget or hfsutils, extracted both archives, and called wget for
# exactly the URLs given.
expect_complete() {
    local name="$1" root="$2" problems="" url want got
    shift 2
    [ "$Q3_STATUS" -eq 0 ] || problems="$problems; exit status $Q3_STATUS"
    grep -q -F "Retro68 Re-Build Complete!" "$Q3_OUT" || problems="$problems; setup did not finish"
    if grep -q -i -e wget -e hfsutils -e hmount "$Q3_OUT"; then
        problems="$problems; output mentions wget or hfsutils:$(grep -i -e wget -e hfsutils -e hmount "$Q3_OUT" |
            sed 's/^/ [/; s/$/]/' | tr -d '\n')"
    fi
    [ "$(grep -c '^unar ' "$root/logs/unar.log" 2> /dev/null)" = 2 ] ||
        problems="$problems; both archives were not extracted"
    [ -s "$root/tools/Retro68-build/powerpc-apple-macos/include/gl.h" ] &&
        [ -s "$root/tools/Retro68-src/InterfacesAndLibraries/SharedLibraries/libOpenGLLibraryStub.a" ] ||
        problems="$problems; the prepared OpenGL files were not installed"
    want=""
    for url; do want="$want wget $url"; done
    got="$(cat "$root/logs/wget.log" 2> /dev/null | tr '\n' ' ' | sed 's/ $//')"
    [ "$got" = "${want# }" ] || problems="$problems; wget calls were [${got}], expected [${want# }]"
    [ ! -e "$root/logs/check.log" ] || problems="$problems; check_retro68.sh ran"
    if [ -z "$problems" ]; then
        pass "$name"
    else
        fail "$name" "${problems#; }"
    fi
}

# expect_needs_wget NAME ROOT FILE: setup stopped without wget, naming FILE
# (and no other archive) as the one to cache, and changed nothing.
expect_needs_wget() {
    local name="$1" root="$2" file="$3" problems="" other
    [ "$Q3_STATUS" -eq 1 ] || problems="$problems; exit status $Q3_STATUS, expected 1"
    grep -q -F "No local copy of $file; attempting download..." "$Q3_OUT" ||
        problems="$problems; no 'No local copy of $file' message"
    grep -q -F "Error: wget is required only when $file is not cached." "$Q3_OUT" ||
        problems="$problems; no wget diagnostic for $file"
    grep -q -F "Place it at the repo root or in tools/, or install wget." "$Q3_OUT" ||
        problems="$problems; no hint to cache $file or install wget"
    for other in "$Q3_MPW_FILE" "$Q3_OPENGL_FILE"; do
        [ "$other" = "$file" ] && continue
        if grep -q -F -e "wget is required only when $other" -e "No local copy of $other" "$Q3_OUT"; then
            problems="$problems; the cached $other was reported as missing"
        fi
    done
    [ "$(grep -c -i wget "$Q3_OUT")" -eq 2 ] ||
        problems="$problems; wget is mentioned other than in the diagnostic for $file"
    if grep -q -i -e hfsutils -e hmount "$Q3_OUT"; then
        problems="$problems; output mentions hfsutils"
    fi
    [ ! -e "$root/logs/unar.log" ] || problems="$problems; an archive was extracted"
    [ ! -e "$root/logs/build-toolchain.log" ] || problems="$problems; the toolchain build started"
    [ ! -e "$root/tools/$file.part" ] || problems="$problems; tools/$file.part was left behind"
    if [ -z "$problems" ]; then
        pass "$name"
    else
        fail "$name" "${problems#; }"
    fi
}

ROOT="$Q3_TEST_WORK/root"
make_setup_root "$ROOT"

# The controlled PATH really lacks the left-out programs.
Q3_FOUND=""
for name in $Q3_LEFT_OUT; do
    found="$(PATH="$ROOT/stubs:$Q3_HOST_PATH" command -v "$name" 2> /dev/null)" &&
        Q3_FOUND="$Q3_FOUND $name=$found"
done
if [ -z "$Q3_FOUND" ]; then
    pass "the controlled PATH has no wget, curl or hfsutils program"
else
    : > "$Q3_OUT"
    fail "the controlled PATH has no wget, curl or hfsutils program" "found:$Q3_FOUND"
fi

# --- Fully cached: neither wget nor hfsutils is needed. ---
make_setup_root "$ROOT"
cache "$ROOT" tools "$Q3_MPW_FILE" "$Q3_OPENGL_FILE"
run_setup "$ROOT"
expect_complete "both archives cached in tools/: setup completes without wget or hfsutils" "$ROOT"

make_setup_root "$ROOT"
cache "$ROOT" root "$Q3_MPW_FILE" "$Q3_OPENGL_FILE"
run_setup "$ROOT"
expect_complete "both archives cached at the repository root: setup completes without wget or hfsutils" "$ROOT"

make_setup_root "$ROOT"
cache "$ROOT" tools "$Q3_MPW_FILE"
cache "$ROOT" root "$Q3_OPENGL_FILE"
run_setup "$ROOT"
expect_complete "archives cached in tools/ and at the repository root: setup completes without wget" "$ROOT"

make_setup_root "$ROOT"
cache "$ROOT" tools "$Q3_MPW_FILE" "$Q3_OPENGL_FILE"
run_setup "$ROOT" wget
expect_complete "both archives cached, wget installed: wget is never called" "$ROOT"

# --- Partly cached: the missing archive, and only it, needs wget. ---
make_setup_root "$ROOT"
cache "$ROOT" tools "$Q3_MPW_FILE"
run_setup "$ROOT"
expect_needs_wget "only the MPW archive cached: setup asks for wget or $Q3_OPENGL_FILE" "$ROOT" "$Q3_OPENGL_FILE"

make_setup_root "$ROOT"
cache "$ROOT" root "$Q3_OPENGL_FILE"
run_setup "$ROOT"
expect_needs_wget "only the OpenGL archive cached: setup asks for wget or $Q3_MPW_FILE" "$ROOT" "$Q3_MPW_FILE"

make_setup_root "$ROOT"
cache "$ROOT" tools "$Q3_MPW_FILE"
run_setup "$ROOT" wget
expect_complete "only the MPW archive cached, wget installed: only the OpenGL archive is downloaded" \
    "$ROOT" "$Q3_OPENGL_URL"

make_setup_root "$ROOT"
cache "$ROOT" root "$Q3_OPENGL_FILE"
run_setup "$ROOT" wget
expect_complete "only the OpenGL archive cached, wget installed: only the MPW archive is downloaded" \
    "$ROOT" "$Q3_MPW_URL"

# --- Uncached: wget is needed. ---
make_setup_root "$ROOT"
run_setup "$ROOT"
expect_needs_wget "no archive cached: setup asks for wget or $Q3_MPW_FILE" "$ROOT" "$Q3_MPW_FILE"

make_setup_root "$ROOT"
run_setup "$ROOT" wget
expect_complete "no archive cached, wget installed: both archives are downloaded" \
    "$ROOT" "$Q3_MPW_URL" "$Q3_OPENGL_URL"

if [ "$Q3_FAILED" -ne 0 ]; then
    echo "setup_retro68.sh offline tests FAILED"
    exit 1
fi
echo "All setup_retro68.sh offline tests passed."
