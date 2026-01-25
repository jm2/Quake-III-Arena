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
MPW_URL="https://download.macintoshgarden.org/apps/MPW_fully_updated.sit"
OPENGL_URL="https://download.macintoshgarden.org/apps/OpenGL_SDK_1.2.sit"

echo "=========================================="
echo "Retro68 Setup Script"
echo "=========================================="
echo "This will:"
echo "1. Clone/Update Retro68"
echo "2. Download MPW & OpenGL SDKs"
echo "3. Inject them into Retro68-src/InterfacesAndLibraries"
echo "4. CLEAN build directories"
echo "5. Rebuild Retro68 completely"
echo "=========================================="
echo "Press Ctrl+C to cancel in 5 seconds..."
sleep 5

# Dependency Checking
REQUIRED_CMDS="cmake git wget bison flex makeinfo unar"
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

# 1. Clone/Update Retro68
echo "Step 1: Cloning/Updating Retro68..."
if [ ! -d "$SOURCE_DIR" ]; then
    git clone --recursive "$RETRO68_URL" "$SOURCE_DIR"
else
    echo "Source directory exists, pulling updates..."
    cd "$SOURCE_DIR"
    git pull
    git submodule update --init --recursive
    cd -
fi

# 2. Prepare InterfacesAndLibraries
echo "Step 2: Preparing SDKs..."
SDK_DEST="$SOURCE_DIR/InterfacesAndLibraries"
mkdir -p "$SDK_DEST"

# Download MPW
if [ ! -f "tools/MPW_fully_updated.sit" ]; then
    echo "Downloading MPW..."
    wget "$MPW_URL" -O "tools/MPW_fully_updated.sit"
fi

# Download OpenGL
if [ ! -f "tools/OpenGL_SDK_1.2.sit" ]; then
    echo "Downloading OpenGL SDK..."
    wget "$OPENGL_URL" -O "tools/OpenGL_SDK_1.2.sit"
fi

# Extract MPW
echo "Extracting MPW..."
rm -rf "tools/temp_mpw"
unar -f "tools/MPW_fully_updated.sit" -o "tools/temp_mpw"

# Move Interfaces&Libraries content from MPW to Retro68 src
echo "Injecting MPW Interfaces&Libraries..."
# Find the MPW/Interfaces&Libraries folder specifically
MPW_I_AND_L=$(find tools/temp_mpw -type d -name "Interfaces&Libraries" | head -n 1)

if [ -n "$MPW_I_AND_L" ] && [ -d "$MPW_I_AND_L" ]; then
    echo "Found MPW I&L at: $MPW_I_AND_L"
    cp -r "$MPW_I_AND_L/"* "$SDK_DEST/"
else
    echo "Error: Could not find Interfaces&Libraries in extracted MPW"
    # Fallback/Debug: list extracted
    ls -R tools/temp_mpw | head -n 20
    exit 1
fi

# Extract OpenGL
echo "Extracting OpenGL SDK..."
rm -rf "tools/temp_opengl"
unar -f "tools/OpenGL_SDK_1.2.sit" -o "tools/temp_opengl"

echo "Injecting OpenGL headers/libs..."
# OpenGL SDK has 'Libraries', 'Headers', etc.
# We need to map them to 'Libraries' and 'Interfaces' in Retro68 struct
# Ensure destination dirs exist
mkdir -p "$SDK_DEST/Libraries"
mkdir -p "$SDK_DEST/Interfaces/CIncludes"

# Copy OpenGL Libraries
# Searching for 'Libraries' folder inside extracted OpenGL dir
OGL_LIBS=$(find tools/temp_opengl -type d -name "Libraries" | head -n 1)
if [ -n "$OGL_LIBS" ]; then
    echo "Copying OpenGL Libs from $OGL_LIBS..."
    cp -r "$OGL_LIBS/"* "$SDK_DEST/Libraries/"
else
    echo "Warning: Could not find Libraries in OpenGL SDK"
fi

# Copy OpenGL Headers
# Searching for 'Headers' folder inside extracted OpenGL dir
OGL_HEADERS=$(find tools/temp_opengl -type d -name "Headers" | head -n 1)
if [ -n "$OGL_HEADERS" ]; then
    echo "Copying OpenGL Headers from $OGL_HEADERS..."
    cp -r "$OGL_HEADERS/"* "$SDK_DEST/Interfaces/CIncludes/"
else
    echo "Warning: Could not find Headers in OpenGL SDK"
fi

# Cleanup temps
rm -rf "tools/temp_mpw" "tools/temp_opengl"

echo "Pruning conflicting MPW headers..."
# Remove standard C headers from MPW that conflict with GCC
# We keep standard Mac Toolbox headers and GL headers
for header in assert.h ctype.h errno.h float.h limits.h locale.h math.h setjmp.h signal.h stdarg.h stddef.h stdio.h stdlib.h string.h time.h; do
    find "$SDK_DEST/Interfaces/CIncludes" -name "$header" -delete
done

# 3. Patching
echo "Step 3: Patching CMakeLists..."
if [ "$OS_NAME" = "Darwin" ]; then
    find "$SOURCE_DIR" -name "CMakeLists.txt" -exec sed -i '' '/find_package(Boost/s/ system//g' {} +
else
    find "$SOURCE_DIR" -name "CMakeLists.txt" -exec sed -i '/find_package(Boost/s/ system//g' {} +
fi

# 4. Clean Build
echo "Step 4: Cleaning previous builds..."
# Removing build dir to force clean build using new headers/libs
if [ -d "$INSTALL_DIR" ]; then
    rm -rf "$INSTALL_DIR"
fi
if [ -d "$BUILD_WORK_DIR" ]; then
    rm -rf "$BUILD_WORK_DIR"
fi

# 5. Build
echo "Step 5: Building Retro68..."
mkdir -p "$BUILD_WORK_DIR"
cd "$BUILD_WORK_DIR"

# Boost paths
if [ "$OS_NAME" = "Darwin" ] && command -v brew &> /dev/null; then
    export BOOST_ROOT="$(brew --prefix boost)"
    export Boost_INCLUDE_DIR="$(brew --prefix boost)/include"
    export Boost_LIBRARY_DIR="$(brew --prefix boost)/lib"
elif [ "$OS_NAME" = "Linux" ]; then
    echo "Using system Boost."
fi

bash "$SOURCE_DIR/build-toolchain.bash" --prefix="$INSTALL_DIR"

echo "=========================================="
echo "Retro68 Re-Build Complete!"
echo "Check $INSTALL_DIR for results."
echo "=========================================="
