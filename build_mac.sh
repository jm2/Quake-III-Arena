#!/bin/bash
set -e

# Create build directory
mkdir -p build_mac
cd build_mac

# Check if compiler exists in PATH or local dir
COMPILER="powerpc-apple-macos-gcc"
LOCAL_BIN="$(pwd)/../tools/Retro68-build/bin"

if ! command -v "$COMPILER" &> /dev/null && [ ! -x "$LOCAL_BIN/$COMPILER" ]; then
    echo "Retro68 compiler not found."
    echo "Running setup_retro68.sh..."
    cd ..
    ./setup_retro68.sh
    cd build_mac
fi

# Add local bin to PATH for this session
export PATH="$LOCAL_BIN:$PATH"

# Configure with CMake using Retro68 toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Retro68.toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Build complete. Check build_mac/Quake3 or similar."
