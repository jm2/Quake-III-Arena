#!/bin/bash
set -e

# Detect OS
OS_NAME=$(uname -s)

# Homebrew/Path setup for macOS
if [ "$OS_NAME" = "Darwin" ]; then
    export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
fi

RETRO68_URL="https://github.com/autc04/Retro68.git"
INSTALL_DIR="$(pwd)/tools/Retro68-build"
SOURCE_DIR="$(pwd)/tools/Retro68-src"
BUILD_WORK_DIR="$(pwd)/tools/Retro68-work"

# macintoshgarden.org has been intermittently unreachable; if you can't fetch
# fresh copies you can drop the originals at the repo root or in tools/ and
# this script will pick them up. URLs are kept for reference but only used as
# a last resort.
MPW_URL="https://download.macintoshgarden.org/apps/MPW_fully_updated.sit"
MPW_FILE="MPW_fully_updated.sit"
OPENGL_URL="https://download.macintoshgarden.org/apps/OpenGL_SDK_1.2.sit"
OPENGL_FILE="OpenGL_SDK_1.2.sit"

echo "=========================================="
echo "Retro68 Setup Script"
echo "=========================================="
echo "This will:"
echo "1. Clone/Update Retro68"
echo "2. Locate or download MPW & OpenGL SDKs (.sit)"
echo "3. Inject them into Retro68-src/InterfacesAndLibraries"
echo "4. CLEAN build directories"
echo "5. Rebuild Retro68 completely"
echo "=========================================="
echo "Press Ctrl+C to cancel in 5 seconds..."
sleep 5

# Dependency Checking
# Note: ruby is required by Retro68's build-toolchain.bash to generate the
# Multiversal Interfaces (it runs make-multiverse.rb even when Universal
# Interfaces are present). Failing late in the toolchain build for a missing
# ruby is painful, so check up front.
REQUIRED_CMDS="cmake git wget bison flex makeinfo unar ruby"
MISSING_DEPS=0

for cmd in $REQUIRED_CMDS; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: Required command '$cmd' not found."
        MISSING_DEPS=1
    fi
done

# Check for hfsutils specific binaries
if ! command -v hmount &> /dev/null; then
     echo "Error: hfsutils (hmount) not found."
     MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
    echo "--------------------------------------------------------"
    echo "Please install missing dependencies manually."
    exit 1
fi

mkdir -p tools

# 1. Clone Retro68 if missing. We do NOT auto-pull here: this script makes
# in-tree edits (Boost patch, InterfacesAndLibraries population) that block
# rebases, and the user can `git pull` manually if they want fresh upstream.
echo "Step 1: Ensuring Retro68 source is present..."
if [ ! -d "$SOURCE_DIR" ]; then
    git clone --recursive "$RETRO68_URL" "$SOURCE_DIR"
else
    echo "Retro68 source already present at $SOURCE_DIR (not pulling)."
fi

# 2. Prepare InterfacesAndLibraries
echo "Step 2: Preparing SDKs..."
SDK_DEST="$SOURCE_DIR/InterfacesAndLibraries"
mkdir -p "$SDK_DEST"

# Resolve a SIT file: prefer tools/<name>, fall back to repo-root <name>, else
# download. The repo-root fallback is the friendly path for offline setups
# when macintoshgarden is down — drop the file alongside the script and re-run.
locate_or_fetch_sit() {
    local fname="$1"
    local url="$2"

    if [ -s "tools/$fname" ]; then
        echo "  Using cached tools/$fname"
        echo "tools/$fname"
        return 0
    fi
    if [ -s "$fname" ]; then
        echo "  Found local $fname at repo root; copying to tools/" >&2
        cp "$fname" "tools/$fname"
        echo "tools/$fname"
        return 0
    fi
    echo "  No local copy of $fname; attempting download..." >&2
    if wget --quiet --show-progress -O "tools/$fname" "$url"; then
        echo "tools/$fname"
        return 0
    fi
    echo "Error: could not obtain $fname (and download failed)." >&2
    echo "Place it at the repo root or in tools/ and re-run." >&2
    return 1
}

# Drop the legacy 0-byte placeholder so locate_or_fetch_sit picks the real one.
[ -f "tools/MPW_fully_updated.sit" ] && [ ! -s "tools/MPW_fully_updated.sit" ] && rm -f "tools/MPW_fully_updated.sit"

MPW_SIT=$(locate_or_fetch_sit "$MPW_FILE" "$MPW_URL")
OPENGL_SIT=$(locate_or_fetch_sit "$OPENGL_FILE" "$OPENGL_URL")

# Extract MPW
echo "Extracting MPW..."
rm -rf "tools/temp_mpw"
unar -q -f "$MPW_SIT" -o "tools/temp_mpw"

# Move Interfaces&Libraries content from MPW to Retro68 src
echo "Injecting MPW Interfaces&Libraries..."
MPW_I_AND_L=$(find tools/temp_mpw -type d -name "Interfaces&Libraries" | head -n 1)

if [ -n "$MPW_I_AND_L" ] && [ -d "$MPW_I_AND_L" ]; then
    echo "Found MPW I&L at: $MPW_I_AND_L"
    cp -r "$MPW_I_AND_L/"* "$SDK_DEST/"
else
    echo "Error: Could not find Interfaces&Libraries in extracted MPW"
    ls -R tools/temp_mpw | head -n 20
    exit 1
fi

# Extract OpenGL
echo "Extracting OpenGL SDK..."
rm -rf "tools/temp_opengl"
unar -q -f "$OPENGL_SIT" -o "tools/temp_opengl"

echo "Injecting OpenGL headers/libs..."
mkdir -p "$SDK_DEST/Libraries"
mkdir -p "$SDK_DEST/Interfaces/CIncludes"

# OpenGL SDK lays out 'Libraries' (PEF stubs) and 'Headers' (gl.h, glu.h, agl.h, glm.h, ...)
OGL_LIBS=$(find tools/temp_opengl -type d -name "Libraries" | head -n 1)
if [ -n "$OGL_LIBS" ]; then
    echo "Copying OpenGL Libs from $OGL_LIBS..."
    mkdir -p "$SDK_DEST/SharedLibraries"
    cp -r "$OGL_LIBS/"* "$SDK_DEST/SharedLibraries/"
else
    echo "Warning: Could not find Libraries in OpenGL SDK"
fi

OGL_HEADERS=$(find tools/temp_opengl -type d -name "Headers" | head -n 1)
if [ -n "$OGL_HEADERS" ]; then
    echo "Copying OpenGL Headers from $OGL_HEADERS..."
    cp -r "$OGL_HEADERS/"* "$SDK_DEST/Interfaces/CIncludes/"
else
    echo "Warning: Could not find Headers in OpenGL SDK"
fi

rm -rf "tools/temp_mpw" "tools/temp_opengl"

echo "Pruning conflicting MPW headers..."
# Remove standard C headers from MPW that conflict with the host GCC's libc.
for header in assert.h ctype.h errno.h float.h limits.h locale.h math.h \
              setjmp.h signal.h stdarg.h stddef.h stdio.h stdlib.h string.h \
              time.h; do
    find "$SDK_DEST/Interfaces/CIncludes" -name "$header" -delete 2>/dev/null || true
done

# 3. Patching
echo "Step 3: Patching CMakeLists for modern Boost..."
if [ "$OS_NAME" = "Darwin" ]; then
    find "$SOURCE_DIR" -name "CMakeLists.txt" -exec sed -i '' '/find_package(Boost/s/ system//g' {} +
else
    find "$SOURCE_DIR" -name "CMakeLists.txt" -exec sed -i '/find_package(Boost/s/ system//g' {} +
fi

# 4. Clean / resume.
#
# A full Retro68 build takes 30+ minutes; if we already have the host tools
# (ConvertDiskImage is the last one built and installed), keep the existing
# binutils/gcc/host-tool artifacts and pass --skip-thirdparty to
# build-toolchain.bash so it just runs the I&L + multiversal + target-lib
# steps. To force a full rebuild, delete tools/Retro68-build manually.
SKIP_FLAGS=()
if [ -x "$INSTALL_DIR/bin/ConvertDiskImage" ] && [ -d "$BUILD_WORK_DIR" ]; then
    echo "Step 4: Existing toolchain detected — resuming with --skip-thirdparty."
    SKIP_FLAGS=(--skip-thirdparty)
else
    echo "Step 4: Cleaning previous builds..."
    [ -d "$INSTALL_DIR" ]    && rm -rf "$INSTALL_DIR"
    [ -d "$BUILD_WORK_DIR" ] && rm -rf "$BUILD_WORK_DIR"
fi

# 5. Build
echo "Step 5: Building Retro68..."
mkdir -p "$BUILD_WORK_DIR"
cd "$BUILD_WORK_DIR"

if [ "$OS_NAME" = "Darwin" ] && command -v brew &> /dev/null; then
    export BOOST_ROOT="$(brew --prefix boost)"
    export Boost_INCLUDE_DIR="$(brew --prefix boost)/include"
    export Boost_LIBRARY_DIR="$(brew --prefix boost)/lib"
elif [ "$OS_NAME" = "Linux" ]; then
    echo "Using system Boost."
fi

bash "$SOURCE_DIR/build-toolchain.bash" --prefix="$INSTALL_DIR" "${SKIP_FLAGS[@]}"

# 6. Post-Build Fixes
echo "Step 6: Post-Build Fixes..."
SDK_DEST="$SOURCE_DIR/InterfacesAndLibraries"
STUB_SRC="$SDK_DEST/SharedLibraries/OpenGLLibraryStub"
STUB_RSRC="$SDK_DEST/SharedLibraries/OpenGLLibraryStub.rsrc"
STUB_AD="$SDK_DEST/SharedLibraries/%OpenGLLibraryStub"
STUB_LIB="$SDK_DEST/SharedLibraries/libOpenGLLibraryStub.a"

if [ -f "$STUB_SRC" ] && [ -f "$STUB_RSRC" ]; then
    echo "Generating OpenGLLibraryStub import library..."
    cp "$STUB_RSRC" "$STUB_AD"
    export PATH="$INSTALL_DIR/bin:$PATH"
    if command -v MakeImport &> /dev/null; then
        MakeImport "$STUB_SRC" "$STUB_LIB"
        echo "Created $STUB_LIB"
    else
        echo "Error: MakeImport not found in $INSTALL_DIR/bin"
        echo "Warning: MakeImport failed. Linux build might fail linking OpenGL."
    fi
fi

echo "=========================================="
echo "Retro68 Re-Build Complete!"
echo "Check $INSTALL_DIR for results."
echo "=========================================="
